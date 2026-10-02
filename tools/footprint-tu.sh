#!/usr/bin/env bash
#
# footprint-tu.sh — reproduce the README "The C++ wrapper on bare metal" table.
#
# tools/footprint.sh measures the library. This script measures the OTHER half of
# the footprint story: what a consumer's own translation unit costs. That cost
# exists because the C++ wrapper is header-only -- it contributes 0 B to
# libsofabuffers.a in every configuration, and its templates instantiate into
# the caller instead, once per TU.
#
# The measured TUs live in test/footprint-tu/: consumer.c and consumer.cpp encode
# and decode the SAME message (five scalar fields, one nested sequence holding two
# scalars, one string), so the C row and the wrapper rows are comparable. Each is
# compiled alone (-c) and `size` is read off the object, so the library calls stay
# external references and only the TU's own code is counted.
#
# The four rows differ only in how the wrapper is configured:
#   C API directly              consumer.c, no wrapper at all
#   C++ wrapper, freestanding   -ffreestanding, so __STDC_HOSTED__ is 0 and
#                               sofab.hpp selects its no-heap surface
#                               (SOFAB_CPP_HAVE_HOSTED off)
#   C++ wrapper, hosted         the same TU without -ffreestanding, so the
#                               hosted convenience layer compiles in
#   C++ wrapper, hosted, RTTI   as hosted, but without -fno-rtti, which is what
#                               the top-level CMakeLists passes for C++
#
# consumer.cpp deliberately uses only the heap-free surface, which exists in both
# wrapper modes -- so the freestanding and hosted rows differ by what the header
# gates, not by what the TU asks for.
#
# Flags mirror a real build of this project for Cortex-M0: the Release flags from
# the top-level CMakeLists.txt plus the toolchain flags from
# utils/cortex-m/toolchain-arm-none-eabi.cmake. ARMv6-m is the architecture the
# README table uses, being the smallest row of the library tables.
#
# Toolchain (Debian/Ubuntu): gcc-arm-none-eabi
#
# Usage:
#   tools/footprint-tu.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT="${OUT:-$(mktemp -d)}"
TU_DIR="${ROOT}/test/footprint-tu"

# --- preflight -------------------------------------------------------------
for tool in arm-none-eabi-gcc arm-none-eabi-g++ arm-none-eabi-size; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: missing ${tool}" >&2
        echo "install it with:" >&2
        echo "    sudo apt-get install -y gcc-arm-none-eabi" >&2
        exit 1
    }
done

# --- flags -----------------------------------------------------------------
# utils/cortex-m/toolchain-arm-none-eabi.cmake, minus -ffreestanding: that one is
# per-row below, because dropping it is exactly what selects the hosted wrapper.
ARCH=(-march=armv6-m -mtune=cortex-m0 -mthumb -mno-unaligned-access)

# CMakeLists.txt: the Release block plus the size flags. -fno-exceptions is in
# both the C and C++ sets; -fno-rtti is C++-only and is per-row below.
OPT=(-Os -DNDEBUG -fno-asynchronous-unwind-tables -fno-exceptions
     -ffunction-sections -fdata-sections)

WARN=(-Wall -Wextra -Wpedantic -Werror)

# ---------------------------------------------------------------------------
# text_of <object> — the .text byte count of one object file.
# ---------------------------------------------------------------------------
text_of() {
    arm-none-eabi-size "$1" | awk 'NR==2 {print $1}'
}

declare -A TEXT

echo ">> Consumer translation unit, ARMv6-m, -Os"

# Row 1: the C API, no wrapper. -ffreestanding as the toolchain file sets it.
arm-none-eabi-gcc "${ARCH[@]}" -ffreestanding "${OPT[@]}" "${WARN[@]}" \
    -std=c99 -I"${ROOT}/src/include" \
    -c "${TU_DIR}/consumer.c" -o "${OUT}/c.o"
TEXT[c]="$(text_of "${OUT}/c.o")"

# Rows 2-4: the wrapper, three configurations of the same TU.
#   label|extra flags
for row in \
    "free|-ffreestanding -fno-rtti" \
    "hosted|-fno-rtti" \
    "hosted-rtti|"
do
    label="${row%%|*}"
    read -r -a extra <<<"${row#*|}"
    arm-none-eabi-g++ "${ARCH[@]}" "${OPT[@]}" "${WARN[@]}" \
        -std=c++20 "${extra[@]}" -I"${ROOT}/src/include" \
        -c "${TU_DIR}/consumer.cpp" -o "${OUT}/${label}.o"
    TEXT[${label}]="$(text_of "${OUT}/${label}.o")"
done

printf '   %-32s %5s B\n' "C API directly"               "${TEXT[c]}"
printf '   %-32s %5s B\n' "C++ wrapper, freestanding"    "${TEXT[free]}"
printf '   %-32s %5s B\n' "C++ wrapper, hosted"          "${TEXT[hosted]}"
printf '   %-32s %5s B\n' "C++ wrapper, hosted, RTTI on" "${TEXT[hosted-rtti]}"

cat <<EOF

==================== README table (paste-ready) ====================

| Consumer translation unit | \`.text\` |
| - | -: |
| C API directly | ${TEXT[c]}&nbsp;B |
| C++ wrapper, freestanding | ${TEXT[free]}&nbsp;B |
| C++ wrapper, hosted | ${TEXT[hosted]}&nbsp;B |
| C++ wrapper, hosted, RTTI on | ${TEXT[hosted-rtti]}&nbsp;B |
EOF
