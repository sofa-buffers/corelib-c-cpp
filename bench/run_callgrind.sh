#!/usr/bin/env bash
#
# SofaBuffers — machine-independent instruction cost (C vs C++).
#
# Runs each benchmark workload once under Callgrind and reports instructions
# retired per operation (Ir). Unlike wall-clock or CPU time, instruction counts
# are deterministic and independent of the host's clock speed and scheduler, so
# the numbers are comparable across machines.
#
# Prereqs: valgrind. The bench_c / bench_cpp binaries are built by CMake; this
# script builds them if missing. Run via: cmake --build build --target
# run_bench_callgrind   (or: BUILD=build bash bench/run_callgrind.sh)
#
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build}"
CBIN="$BUILD/bench/bench_c"
CPPBIN="$BUILD/bench/bench_cpp"

if ! command -v valgrind >/dev/null 2>&1; then
    echo "error: valgrind not found (needed for instruction counts)." >&2
    echo "       install it, e.g.  apt-get install valgrind" >&2
    exit 1
fi

if [ ! -x "$CBIN" ] || [ ! -x "$CPPBIN" ]; then
    echo ">> building bench_c / bench_cpp ..."
    cmake --build "$BUILD" --target bench_c bench_cpp >/dev/null
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
# In BENCH_SPEC row order. The blob rows are where the instruction count earns
# its keep: the one-shot-to-streaming delta is the divisible-run cost
# (CORELIB_PLAN §5.1) with the host's memory subsystem and scheduler taken out
# of it, which under MB/s drowns in memory bandwidth. The optional
# `blob 1MB passthrough` row is absent because this port does not implement
# pass-through -- a port that does not omits the row rather than faking it.
WORKLOADS=(
    encode_u64_array
    encode_typical
    encode_blob_oneshot
    encode_blob_streaming
    encode_composite
    decode_u64_array
    decode_typical
    decode_blob
    decode_composite
    decode_composite_skip
)

run_cg() { # $1 binary, $2 tag, $3 workload
    valgrind --tool=callgrind --collect-atstart=no --toggle-collect="run_$3" \
        --callgrind-out-file="$OUT/$2.$3.out" "$1" "$3" \
        >/dev/null 2>"$OUT/$2.$3.log"
}

echo ">> Measuring instructions/op under Callgrind (this is slow) ..."
for w in "${WORKLOADS[@]}"; do
    run_cg "$CBIN"   c   "$w"
    run_cg "$CPPBIN" cpp "$w"
done

ir_of()    { grep -m1 '^summary:' "$OUT/$1.$2.out" 2>/dev/null | awk '{print $2}'; }
bytes_of() { grep -ohE 'BYTES=[0-9]+' "$OUT/c.$1.log" 2>/dev/null | head -1 | cut -d= -f2; }

label() {
    case "$1" in
        encode_u64_array)      echo "encode: u64 array (1000)";;
        encode_typical)        echo "encode: typical message";;
        encode_blob_oneshot)   echo "encode: blob 1MB one-shot";;
        encode_blob_streaming) echo "encode: blob 1MB streaming";;
        encode_composite)      echo "encode: composite";;
        decode_u64_array)      echo "decode: u64 array (1000)";;
        decode_typical)        echo "decode: typical message";;
        decode_blob)           echo "decode: blob 1MB";;
        decode_composite)      echo "decode: composite";;
        decode_composite_skip) echo "decode: composite skip-all";;
    esac
}

echo
echo "==============================================================================="
echo " SofaBuffers instruction cost — C vs C++   (Callgrind, -O3)"
echo " instructions/op: lower is better. Deterministic & machine-independent."
echo "==============================================================================="
printf "%-26s %14s %14s %9s %9s\n" "Workload" "C instr/op" "C++ instr/op" "C++/C" "bytes"
printf "%-26s %14s %14s %9s %9s\n" "--------" "----------" "------------" "-----" "-----"

for w in "${WORKLOADS[@]}"; do
    c="$(ir_of c "$w")"; p="$(ir_of cpp "$w")"; b="$(bytes_of "$w")"
    ratio="$(awk -v c="${c:-0}" -v p="${p:-0}" 'BEGIN{ if (c+0 > 0) printf "%.2fx", p/c; else printf "-" }')"
    printf "%-26s %14s %14s %9s %9s\n" "$(label "$w")" "${c:--}" "${p:--}" "$ratio" "${b:--}"
done
echo
echo "Ir = instructions retired (Callgrind). Independent of CPU clock and OS"
echo "scheduling; depends only on the compiled code, so it compares across machines."
