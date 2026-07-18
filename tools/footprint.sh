#!/usr/bin/env bash
#
# footprint.sh — reproduce the README "Footprint" tables.
#
# For each target architecture and each build configuration this builds only the
# sofabuffers static library with -Os (matching the CI build jobs under
# .github/workflows/build-gcc-*.yaml) and reports `size libsofabuffers.a`.
# Because the C core never allocates, .data/.bss are always 0 and the whole cost
# is .text (flash); the tables therefore track .text.
#
# The configurations mirror the README:
#   full            — everything on (ostream.c + istream.c + object.c)
#   full-strict     — same as full, plus SOFAB_ENABLE_STRICT_UTF8 (compiles the
#                     otherwise-empty utf8.c validator in); isolates its .text cost
#   minimal         — SOFAB_DISABLE_{FIXLEN,ARRAY,SEQUENCE}_SUPPORT +
#                     SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK + OBJECT_DESCR_SMALL,
#                     object API still built
#   minimal-noobj   — same, additionally SOFAB_DISABLE_OBJECT_API (drops object.c)
#
# Toolchains (Debian/Ubuntu package names):
#   ARM Cortex-M      gcc-arm-none-eabi
#   AVR               gcc-avr avr-libc binutils-avr
#   RISC-V (bare)     gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf
#
# Usage:
#   tools/footprint.sh            # build everything and print the tables
#   BUILD_DIR=/path tools/footprint.sh
#
set -euo pipefail

# --- locate the repo root (this script lives in <root>/tools) --------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build/footprint}"

# --- shared CMake flags (match the CI build jobs) --------------------------
# Only the static library is built, so tests/bench/install/C++ are all off to
# keep the configure step dependency-free (no FetchContent of Unity/Catch2).
COMMON=(
  -DCMAKE_BUILD_TYPE=Release
  -DSOFAB_ENABLE_CPP=OFF
  -DSOFAB_ENABLE_BENCH=OFF
  -DSOFAB_BUILD_TESTS=OFF
  -DSOFAB_INSTALL=OFF
)

# Feature switches that define the "minimal" wire format (README).
MIN_MACROS="-DSOFAB_DISABLE_FIXLEN_SUPPORT \
-DSOFAB_DISABLE_ARRAY_SUPPORT \
-DSOFAB_DISABLE_SEQUENCE_SUPPORT \
-DSOFAB_DISABLE_INTEGER_OVERFLOW_CHECK \
-DSOFAB_OBJECT_DESCR_PROFILE=SOFAB_OBJECT_DESCR_SMALL"

# --- target architectures --------------------------------------------------
# One representative per README row. The label matches the README table; the
# ARMv7 row uses the fp.dp target (cortex-m7) — the dependency-free core emits no
# FP instructions, so the +fp.dp suffix does not change the generated .text.
#
# Each entry: "label|size-tool|cmake toolchain/compiler args..."
ARCHES=(
  "ARMv6-m|arm-none-eabi-size|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/cortex-m/toolchain-arm-none-eabi.cmake -DARM_MARCH=armv6-m -DARM_MTUNE=cortex-m0"
  "ARMv7-m+fp.dp|arm-none-eabi-size|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/cortex-m/toolchain-arm-none-eabi.cmake -DARM_MARCH=armv7e-m+fp.dp -DARM_MTUNE=cortex-m7"
  "RV32IMC|riscv64-unknown-elf-size|-DCMAKE_TOOLCHAIN_FILE=${BUILD_DIR}/toolchain-rv32.cmake"
  "atmega8|avr-size|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/avr/toolchain-avr.cmake -DAVR_MCU=atmega8"
)

# --- build configurations --------------------------------------------------
# Each entry: "key|Display Name". The extra CMake args per config are built as a
# proper array in config_extra() below, because -DCMAKE_C_FLAGS_RELEASE holds a
# space-separated value that must survive as a single argv element.
CONFIGS=(
  "full|Full configuration"
  "full-strict|Full configuration, strict UTF-8 on"
  "minimal|Minimal configuration (object API on)"
  "minimal-noobj|Minimal configuration, without object.c"
)

# Populate the global array CFG_EXTRA with the extra CMake args for a config key.
config_extra() {
  case "$1" in
    full)          CFG_EXTRA=() ;;
    full-strict)   CFG_EXTRA=( -DSOFAB_ENABLE_STRICT_UTF8=ON ) ;;
    minimal)       CFG_EXTRA=( -DCMAKE_C_FLAGS_RELEASE="-Os -DNDEBUG ${MIN_MACROS}" ) ;;
    minimal-noobj) CFG_EXTRA=( -DSOFAB_DISABLE_OBJECT_API=ON
                               -DCMAKE_C_FLAGS_RELEASE="-Os -DNDEBUG ${MIN_MACROS}" ) ;;
    *) echo "unknown config: $1" >&2; exit 1 ;;
  esac
}

# ---------------------------------------------------------------------------
# Preflight: make sure the toolchains are installed.
# ---------------------------------------------------------------------------
declare -A PKG_HINT=(
  [arm-none-eabi-gcc]="gcc-arm-none-eabi"
  [arm-none-eabi-size]="gcc-arm-none-eabi"
  [avr-gcc]="gcc-avr avr-libc binutils-avr"
  [avr-size]="binutils-avr"
  [riscv64-unknown-elf-gcc]="gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf"
  [riscv64-unknown-elf-size]="gcc-riscv64-unknown-elf"
  [cmake]="cmake"
  [make]="make"
)
preflight() {
  local missing=() need=() tool
  for tool in cmake make \
              arm-none-eabi-gcc arm-none-eabi-size \
              avr-gcc avr-size \
              riscv64-unknown-elf-gcc riscv64-unknown-elf-size; do
    command -v "$tool" >/dev/null 2>&1 || { missing+=("$tool"); need+=("${PKG_HINT[$tool]}"); }
  done
  if ((${#missing[@]})); then
    echo "error: missing tools: ${missing[*]}" >&2
    # de-duplicate the suggested packages
    local pkgs; pkgs="$(printf '%s\n' "${need[@]}" | tr ' ' '\n' | sort -u | paste -sd' ')"
    echo "install them with:" >&2
    echo "    sudo apt-get install -y ${pkgs}" >&2
    exit 1
  fi
}

# ---------------------------------------------------------------------------
# Generate the RISC-V RV32IMC toolchain file.
#
# Ubuntu's gcc-riscv64-unknown-elf ships no C library, so headers come from
# picolibc via --specs=picolibc.specs. Only the static lib is compiled (never
# linked), so this is enough. CI does not currently build RV32IMC — the RISC-V
# workflow targets rv64 linux-gnu under QEMU — so this row is reproduced here.
# ---------------------------------------------------------------------------
write_rv32_toolchain() {
  cat > "${BUILD_DIR}/toolchain-rv32.cmake" <<'EOF'
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)
set(CMAKE_C_COMPILER   riscv64-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_C_FLAGS   "-march=rv32imc -mabi=ilp32 --specs=picolibc.specs")
set(CMAKE_CXX_FLAGS "-march=rv32imc -mabi=ilp32 --specs=picolibc.specs")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
EOF
}

# ---------------------------------------------------------------------------
# Build one (config, arch) pair and echo the .text byte count.
# ---------------------------------------------------------------------------
build_one() {
  local size_tool="$1" dir="$2" arch_args="$3"; shift 3
  # $@ now holds the per-config extra args (CFG_EXTRA), already correctly split
  # so the space-containing -DCMAKE_C_FLAGS_RELEASE value stays one argument.
  # arch_args is intentionally word-split (its flags contain no spaces).
  # shellcheck disable=SC2086
  cmake -S "${ROOT}" -B "${dir}" "${COMMON[@]}" ${arch_args} "$@" \
      >"${dir}.cmake.log" 2>&1
  cmake --build "${dir}" --target sofabuffers >"${dir}.build.log" 2>&1
  # sum .text across every archive member (header row skipped; no totals line)
  "${size_tool}" "${dir}/src/libsofabuffers.a" \
    | awk 'NR>1 && $1 ~ /^[0-9]+$/ {t+=$1} END {print t+0}'
}

# ---------------------------------------------------------------------------
main() {
  preflight
  rm -rf "${BUILD_DIR}"
  mkdir -p "${BUILD_DIR}"
  write_rv32_toolchain

  declare -A TEXT   # TEXT["cfgkey|label"] = bytes

  local entry cfgkey cfgname a label size_tool arch_args bytes dir
  for entry in "${CONFIGS[@]}"; do
    IFS='|' read -r cfgkey cfgname <<<"${entry}"
    config_extra "${cfgkey}"
    echo ">> ${cfgname}"
    for a in "${ARCHES[@]}"; do
      IFS='|' read -r label size_tool arch_args <<<"${a}"
      dir="${BUILD_DIR}/${cfgkey}-${label//[^A-Za-z0-9]/_}"
      bytes="$(build_one "${size_tool}" "${dir}" "${arch_args}" "${CFG_EXTRA[@]}")"
      TEXT["${cfgkey}|${label}"]="${bytes}"
      printf '   %-16s .text=%5s B  (~%.1fKB)\n' "${label}" "${bytes}" \
        "$(awk "BEGIN{print ${bytes}/1024}")"
    done
  done

  # --- emit README-shaped markdown tables ---------------------------------
  echo
  echo "==================== README tables (paste-ready) ===================="
  for entry in "${CONFIGS[@]}"; do
    IFS='|' read -r cfgkey cfgname _ <<<"${entry}"
    echo
    echo "**${cfgname}**"
    echo
    echo "| Architecture | .text | .data | .bss |"
    echo "| - | - | - | - |"
    for a in "${ARCHES[@]}"; do
      IFS='|' read -r label _ _ <<<"${a}"
      bytes="${TEXT["${cfgkey}|${label}"]}"
      printf '| %s | ~%.1fKB | 0.0KB | 0.0KB |\n' "${label}" \
        "$(awk "BEGIN{print ${bytes}/1024}")"
    done
  done
}

main "$@"
