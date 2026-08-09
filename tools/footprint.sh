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
# The per-architecture toolchain files live under utils/; the CI target
# workflows use the same ones, so this script and CI compile identically.
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

# The four configurations, and the flags that define them, come from the file the
# CI target workflows also source — so a table regenerated here and a library
# built by CI cannot disagree about what "minimal" means.
# shellcheck source=../utils/ci/build-config.sh
. "${ROOT}/utils/ci/build-config.sh"

# --- target architectures --------------------------------------------------
# One representative per README row. The label matches the README table; the
# ARMv7 row uses the fp.dp target (cortex-m7) — the dependency-free core emits no
# FP instructions, so the +fp.dp suffix does not change the generated .text.
#
# Each entry: "label|size-tool|cmake toolchain/compiler args..."
ARCHES=(
  "ARMv6-m|arm-none-eabi-size|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/cortex-m/toolchain-arm-none-eabi.cmake -DARM_MARCH=armv6-m -DARM_MTUNE=cortex-m0"
  "ARMv7-m+fp.dp|arm-none-eabi-size|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/cortex-m/toolchain-arm-none-eabi.cmake -DARM_MARCH=armv7e-m+fp.dp -DARM_MTUNE=cortex-m7"
  "RV32IMC|riscv64-unknown-elf-size|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/riscv32/toolchain-riscv32.cmake"
  "atmega8|avr-size|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/avr/toolchain-avr.cmake -DAVR_MCU=atmega8"
)

# --- build configurations --------------------------------------------------
# The keys come from SOFAB_CONFIGS (build-config.sh); only the table headings
# below are this script's own. sofab_config_args emits one CMake argument per
# line, so mapfile is what preserves the space-containing
# -DCMAKE_C_FLAGS_RELEASE as a single argv element.
declare -A CONFIG_TITLE=(
  [full]="Full configuration"
  [full-strict]="Full configuration, strict UTF-8 on"
  [minimal]="Minimal configuration (object API on)"
  [minimal-noobj]="Minimal configuration, without object.c"
)

# Populate the global array CFG_EXTRA with the extra CMake args for a config key.
config_extra() {
  mapfile -t CFG_EXTRA < <(sofab_config_args "$1")
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

# --- per-switch table ------------------------------------------------------
# The four configurations above answer "how small can it get". They do not
# answer "what is this one switch worth", which is the question a reader who
# needs exactly one feature gone actually has. Each entry below is the full
# configuration plus one switch, measured on one architecture (ARMv6-m, the
# README's smallest row) — a delta per switch, all against the same baseline.
#
# Each entry: "label|cmake args..."
#
# SOFAB_DISABLE_LAZY_SEQ_SUPPORT is the one row passed as a compiler define
# rather than a CMake option, because it is the one switch that has no option:
# src/CMakeLists.txt declares the other five SOFAB_DISABLE_*_SUPPORT flags in a
# foreach and this one is not in it. -DSOFAB_DISABLE_LAZY_SEQ_SUPPORT=ON on the
# command line is therefore silently ignored, which is worth knowing before
# trusting a number measured that way. Overriding CMAKE_C_FLAGS_RELEASE means
# restating its default (-O3 -DNDEBUG); the -Os comes from add_compile_options()
# in the top-level CMakeLists and is unaffected.
SWITCHES=(
  "SOFAB_DISABLE_FIXLEN_SUPPORT|-DSOFAB_DISABLE_FIXLEN_SUPPORT=ON"
  "SOFAB_DISABLE_ARRAY_SUPPORT|-DSOFAB_DISABLE_ARRAY_SUPPORT=ON"
  "SOFAB_DISABLE_SEQUENCE_SUPPORT|-DSOFAB_DISABLE_SEQUENCE_SUPPORT=ON"
  "SOFAB_DISABLE_LAZY_SEQ_SUPPORT|-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -DSOFAB_DISABLE_LAZY_SEQ_SUPPORT=1"
  "SOFAB_DISABLE_FP64_SUPPORT|-DSOFAB_DISABLE_FP64_SUPPORT=ON"
  "SOFAB_DISABLE_INT64_SUPPORT|-DSOFAB_DISABLE_INT64_SUPPORT=ON"
  "SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK|-DSOFAB_DISABLE_INTEGER_OVERFLOW_CHECK=ON"
  "SOFAB_DISABLE_OBJECT_API|-DSOFAB_DISABLE_OBJECT_API=ON"
  "SOFAB_ENABLE_STRICT_UTF8|-DSOFAB_ENABLE_STRICT_UTF8=ON"
  "SOFAB_ENABLE_SKIP_COUNTER|-DSOFAB_ENABLE_SKIP_COUNTER=ON"
  "SOFAB_OBJECT_DESCR_PROFILE=SOFAB_OBJECT_DESCR_SMALL|-DSOFAB_OBJECT_DESCR_PROFILE=SOFAB_OBJECT_DESCR_SMALL"
  "SOFAB_OBJECT_DESCR_PROFILE=SOFAB_OBJECT_DESCR_BIG|-DSOFAB_OBJECT_DESCR_PROFILE=SOFAB_OBJECT_DESCR_BIG"
)

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

  declare -A TEXT   # TEXT["cfgkey|label"] = bytes

  local cfgkey cfgname a label size_tool arch_args bytes dir
  for cfgkey in "${SOFAB_CONFIGS[@]}"; do
    cfgname="${CONFIG_TITLE[${cfgkey}]}"
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

  # --- per-switch deltas on one architecture ------------------------------
  # ARMv6-m is the README's smallest row, so it is the one an embedded reader
  # is budgeting against. The baseline is the `full` figure already measured.
  local arm_args base label sw_args sw_bytes
  IFS='|' read -r _ _ arm_args <<<"${ARCHES[0]}"
  base="${TEXT["full|ARMv6-m"]}"
  echo
  echo ">> What each switch is worth (ARMv6-m, against full = ${base} B)"
  declare -A SWITCH_TEXT
  local s
  for s in "${SWITCHES[@]}"; do
    IFS='|' read -r label sw_args <<<"${s}"
    dir="${BUILD_DIR}/switch-${label//[^A-Za-z0-9]/_}"
    # sw_args holds exactly one argument, which may contain spaces (the
    # CMAKE_C_FLAGS_RELEASE row), so it is passed quoted rather than split.
    sw_bytes="$(build_one "arm-none-eabi-size" "${dir}" "${arm_args}" "${sw_args}")"
    SWITCH_TEXT["${label}"]="${sw_bytes}"
    printf '   %-38s %6s B  (%+d B)\n' "${label}" "${sw_bytes}" \
      "$((sw_bytes - base))"
  done

  # --- emit README-shaped markdown tables ---------------------------------
  echo
  echo "==================== README tables (paste-ready) ===================="
  for cfgkey in "${SOFAB_CONFIGS[@]}"; do
    cfgname="${CONFIG_TITLE[${cfgkey}]}"
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

  echo
  echo "**What each switch is worth** (ARMv6-m, full = ${base}&nbsp;B \`.text\`)"
  echo
  echo "| Switch | \`.text\` | delta |"
  echo "| - | -: | -: |"
  for s in "${SWITCHES[@]}"; do
    IFS='|' read -r label _ <<<"${s}"
    sw_bytes="${SWITCH_TEXT["${label}"]}"
    printf '| `%s` | %s&nbsp;B | %+d&nbsp;B |\n' "${label}" "${sw_bytes}" \
      "$((sw_bytes - base))"
  done
}

main "$@"
