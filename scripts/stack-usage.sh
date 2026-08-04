#!/usr/bin/env bash
#
# stack-usage.sh — static worst-case stack usage of the encode and decode paths.
#
# The README footprint tables (tools/footprint.sh) answer "how much flash and
# static RAM does the library cost". They say nothing about the *stack*, which
# on an MCU is the other half of the RAM budget. This script answers that half,
# statically — no target hardware, no QEMU, no run-time instrumentation.
#
# METHOD
#   The library is compiled with:
#     -fstack-usage        -> one .su per TU: frame size of every function
#     -fcallgraph-info=su,da -> one .ci per TU: the same frame sizes *plus* the
#                             call graph, in VCG format
#   Both land next to the object files under the build directory. The .ci files
#   are then parsed and, for every public entry point, the worst-case call path
#   is computed as
#       total(f) = frame(f) + max over callees c of total(c)
#   i.e. the deepest chain of frames that call can push.
#
#   RECURSION. The *stream* API is non-recursive by construction — nesting depth
#   lives in the decoder struct, not on the stack (CORELIB_PLAN 4.9) — so for
#   sofab_ostream_*/sofab_istream_* the graph is a DAG and the maximum above is
#   exact and final. The *object* API is not: sofab_object_encode and
#   sofab_object_init walk a descriptor tree by calling themselves once per
#   nesting level. For a directly self-recursive f the total is therefore
#       total(f, depth d) = d * frame(f) + <deepest non-recursive path below f>
#   and the report gives both parts: the depth-1 figure and the "B/level" that
#   each further level of *descriptor* nesting adds. Multiply by the deepest
#   descriptor you actually encode — that depth is a property of your schema and
#   is not visible here. Mutual recursion between two functions would break this
#   decomposition; none exists today, and it is reported as an error if it ever
#   appears.
#
# WHAT THE NUMBER DOES *NOT* INCLUDE — read this before quoting it
#   1. Calls out of the library. The ostream flush callback and the istream
#      field callback are indirect calls into *your* code; their stack is
#      unknowable here and counted as 0. Every entry point that can reach one is
#      marked "+cb" and listed under "indirect calls".
#      Anything else the library calls but does not define (libc memcpy on some
#      targets) is likewise counted as 0 and listed under "external callees".
#   2. Header-inline API. sofab_ostream_write_string/_blob/_boolean/_fp32/... are
#      static inline wrappers in ostream.h, so they have no frame *here* — their
#      cost is inlined into your calling function and shows up in your TU's .su.
#      What is measured is the extern function each of them tails into.
#   3. The caller's own frame, interrupt frames, and any CPU-pushed exception
#      context. A host (`--arch host`) build additionally inherits the distro
#      compiler's -fstack-protector default, which inflates frames relative to
#      the bare-metal targets; the cross targets are the meaningful rows.
#   4. On targets where the call instruction pushes the return address instead
#      of the callee's prologue saving it (x86), -fstack-usage does not count
#      that word. --call-overhead adds it back per call edge; the default is
#      chosen per target (0 for ARM/RISC-V/AVR, which save lr/ra/PC in the
#      measured frame; 8 for the x86-64 host).
#
# The figure is a *ceiling*: it assumes the deepest path is taken, which for a
# single field write it is. It is the number to size a task stack with, not the
# number a particular message will actually use.
#
# Configurations and toolchains are the ones utils/ci/build-config.sh defines
# and tools/footprint.sh builds, so a stack figure and a footprint figure always
# describe the same library.
#
# Usage:
#   scripts/stack-usage.sh                        # all four README targets, config `full`
#   scripts/stack-usage.sh --arch armv6-m         # one target, quick
#   scripts/stack-usage.sh --arch host --paths    # show the worst-case call path
#   scripts/stack-usage.sh --config minimal
#   scripts/stack-usage.sh --arch armv6-m --budget 128   # exit 1 if over -> CI gate
#   BUILD_DIR=/path scripts/stack-usage.sh
#
# SPDX-License-Identifier: MIT
#
set -euo pipefail

# --- locate the repo root (this script lives in <root>/scripts) --------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build/stack-usage}"

# The configuration flags come from the file the CI workflows and
# tools/footprint.sh also source — `minimal` must mean one thing repo-wide.
# shellcheck source=../utils/ci/build-config.sh
. "${ROOT}/utils/ci/build-config.sh"

# --- target architectures ----------------------------------------------------
# The four README rows plus `host`. Each entry:
#   "key|label|default call overhead|cmake toolchain/compiler args..."
# The call overhead is the return-address word -fstack-usage does not attribute
# to the callee (see caveat 4 above): zero everywhere the prologue saves the
# link register itself, 8 on x86-64 where the call instruction pushes it.
ARCHES=(
  "armv6-m|ARMv6-m (cortex-m0)|0|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/cortex-m/toolchain-arm-none-eabi.cmake -DARM_MARCH=armv6-m -DARM_MTUNE=cortex-m0"
  "armv7-m|ARMv7-m (cortex-m7)|0|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/cortex-m/toolchain-arm-none-eabi.cmake -DARM_MARCH=armv7e-m+fp.dp -DARM_MTUNE=cortex-m7"
  "rv32imc|RV32IMC|0|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/riscv32/toolchain-riscv32.cmake"
  "atmega8|atmega8 (AVR)|0|-DCMAKE_TOOLCHAIN_FILE=${ROOT}/utils/avr/toolchain-avr.cmake -DAVR_MCU=atmega8"
  "host|host $(uname -m)|8|"
)
DEFAULT_ARCHES="armv6-m,armv7-m,rv32imc,atmega8"

# Compiler driver per arch key, for the preflight check.
declare -A ARCH_CC=(
  [armv6-m]=arm-none-eabi-gcc
  [armv7-m]=arm-none-eabi-gcc
  [rv32imc]=riscv64-unknown-elf-gcc
  [atmega8]=avr-gcc
  [host]=cc
)
declare -A PKG_HINT=(
  [arm-none-eabi-gcc]="gcc-arm-none-eabi"
  [avr-gcc]="gcc-avr avr-libc binutils-avr"
  [riscv64-unknown-elf-gcc]="gcc-riscv64-unknown-elf picolibc-riscv64-unknown-elf"
  [cc]="gcc"
  [cmake]="cmake"
  [make]="make"
)

# --- options -----------------------------------------------------------------
OPT_ARCHES="${DEFAULT_ARCHES}"
OPT_CONFIG="full"
OPT_BUDGET=""
OPT_PATHS=0
OPT_OVERHEAD=""   # empty => per-arch default from ARCHES

# Print the file header (everything down to the SPDX line) as the help text, so
# the documentation and --help cannot drift apart.
usage() { sed -n '3,/^# SPDX/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

while (($#)); do
  case "$1" in
    --arch)           OPT_ARCHES="$2"; shift 2 ;;
    --config)         OPT_CONFIG="$2"; shift 2 ;;
    --budget)         OPT_BUDGET="$2"; shift 2 ;;
    --call-overhead)  OPT_OVERHEAD="$2"; shift 2 ;;
    --paths)          OPT_PATHS=1; shift ;;
    -h|--help)        usage; exit 0 ;;
    *) echo "unknown option: $1 (try --help)" >&2; exit 2 ;;
  esac
done

[[ "${OPT_ARCHES}" == "all" ]] && OPT_ARCHES="armv6-m,armv7-m,rv32imc,atmega8,host"

# --- the analyser ------------------------------------------------------------
# Reads the .ci files of one build, writes tab-separated records:
#   ROOT  <total> <own> <name> <group> <callback?> <recursion> <path>
#         <recursion> is "-", or "f=N,g=M": each self-recursive function
#         reachable from this entry point and the bytes one more level costs
#   DYN   <name>  <detail>       function with a non-static frame
#   EXT   <name>                 callee we do not define; counted as 0
#   IND   <name>                 function reaching an indirect (callback) call
#   CYCLE <name>                 mutual recursion; the decomposition breaks down
read -r -d '' ANALYSER <<'AWK' || true
# Value of `key: "..."` on the current line, "" if absent.
function qval(line, key,   p, rest, q) {
    p = index(line, key ": \"")
    if (p == 0) return ""
    rest = substr(line, p + length(key) + 3)
    q = index(rest, "\"")
    return (q ? substr(rest, 1, q - 1) : "")
}

# The .ci label packs "name\nfile:line:col\nN bytes (qual)\nM dynamic objects"
# with a literal backslash-n as separator. A node with no "bytes" part is a
# declaration of something this build does not define (libc, a callback stub).
/^node:/ {
    title = qval($0, "title")
    lbl   = qval($0, "label")
    p     = index(lbl, "\\n")
    short = (p ? substr(lbl, 1, p - 1) : lbl)
    NAME[title] = short
    # statics get a file-qualified title; show them as basename:name so two
    # same-named statics in different TUs stay distinguishable. The file comes
    # from the label's second component rather than from the title, because GCC
    # appends IPA suffixes (.isra.0, .constprop.0) to the title only.
    if (title != short) {
        rest = substr(lbl, p + 2)
        q = index(rest, "\\n"); if (q) rest = substr(rest, 1, q - 1)
        q = index(rest, ":");   if (q) rest = substr(rest, 1, q - 1)
        q = split(rest, seg, "/")
        DISP[title] = seg[q] ":" short
    } else {
        DISP[title] = short
    }
    if (match(lbl, /[0-9]+ bytes \(/)) {
        FRAME[title] = substr(lbl, RSTART, RLENGTH) + 0
        rest = substr(lbl, RSTART + RLENGTH)
        QUAL[title] = substr(rest, 1, index(rest, ")") - 1)
        DYNOBJ[title] = 0
        if (match(lbl, /[0-9]+ dynamic objects/))
            DYNOBJ[title] = substr(lbl, RSTART, RLENGTH) + 0
    }
    next
}

/^edge:/ {
    src = qval($0, "sourcename")
    tgt = qval($0, "targetname")
    if (src == "" || tgt == "") next
    if (!((src SUBSEP tgt) in SEEN)) {
        SEEN[src SUBSEP tgt] = 1
        ADJ[src] = ADJ[src] " " tgt
    }
    next
}

# Add every element of `list` to the recursion set of n, keeping it unique.
function addset(n, list,   i, c, a) {
    c = split(list, a, " ")
    for (i = 1; i <= c; i++)
        if (index(" " RSET[n] " ", " " a[i] " ") == 0) RSET[n] = RSET[n] " " a[i]
}

# Worst-case stack below and including n, at recursion depth 1.
# Memoised. A self-edge is not followed — it is recorded in PERLVL as the cost
# of one further level (see the RECURSION note in the file header). A back edge
# to any *other* function still on the stack is mutual recursion, which this
# decomposition cannot express, so it is flagged and reported as an error.
function worst(n,   i, cnt, arr, t, v, best, bestn) {
    if (n in DONE)    return TOT[n]
    if (n in ONSTACK) { CYCLE[n] = 1; return 0 }
    ONSTACK[n] = 1
    best = 0; bestn = ""; RSET[n] = ""
    cnt = split(ADJ[n], arr, " ")
    for (i = 1; i <= cnt; i++) {
        t = arr[i]
        if (t == n) {                   # direct recursion: costed per level
            SELFREC[n] = 1
            continue
        }
        if (t == "__indirect_call") {
            INDIRECT[n] = 1
            continue                    # user code; unknowable, counted as 0
        }
        if (t in FRAME) {
            v = worst(t) + OVERHEAD
            if (t in CBREACH) CBREACH[n] = 1
            addset(n, RSET[t])
        } else {
            EXT[t] = 1                  # libc &c; unknowable, counted as 0
            v = OVERHEAD
        }
        if (v > best) { best = v; bestn = t }
    }
    if (n in INDIRECT) CBREACH[n] = 1
    if (n in SELFREC) { PERLVL[n] = FRAME[n] + OVERHEAD; addset(n, n) }
    delete ONSTACK[n]
    NEXT[n] = bestn
    TOT[n]  = FRAME[n] + best
    DONE[n] = 1
    return TOT[n]
}

# "name=bytes,name=bytes" for every self-recursive function reachable from n.
# "name=bytes,name=bytes" for every self-recursive function reachable from n,
# or "-" if there is none. Never empty: a bare tab-separated empty field would
# be swallowed by `read`, for which a tab is whitespace.
function recnote(n,   i, c, a, s) {
    c = split(RSET[n], a, " ")
    for (i = 1; i <= c; i++) s = s (s ? "," : "") DISP[a[i]] "=" PERLVL[a[i]]
    return (s == "" ? "-" : s)
}

function pathof(n,   s, hop, cur) {
    s = DISP[n] ((n in SELFREC) ? " (recursive)" : "")
    cur = n
    for (hop = 0; hop < 64; hop++) {
        cur = NEXT[cur]
        if (cur == "") break
        s = s " > " ((cur in DISP) ? DISP[cur] : cur) ((cur in SELFREC) ? " (recursive)" : "")
    }
    if (n in INDIRECT) s = s " > <callback>"
    return s
}

END {
    for (n in FRAME) {
        short = NAME[n]
        if (short !~ /^sofab_(ostream|istream|object)_/) continue
        if (short ~ /^sofab_ostream_/)      grp = "encode"
        else if (short ~ /^sofab_istream_/) grp = "decode"
        else                                grp = "object"
        printf "ROOT\t%d\t%d\t%s\t%s\t%d\t%s\t%s\n", worst(n), FRAME[n], short, grp,
               ((n in CBREACH) ? 1 : 0), recnote(n), pathof(n)
    }
    for (n in FRAME)
        if (QUAL[n] != "static" || DYNOBJ[n] > 0)
            printf "DYN\t%s\t%s frame, %d dynamic objects\n", DISP[n], QUAL[n], DYNOBJ[n]
    for (n in EXT)      printf "EXT\t%s\n", n
    for (n in INDIRECT) printf "IND\t%s\n", DISP[n]
    for (n in CYCLE)    printf "CYCLE\t%s\n", DISP[n]
}
AWK

# ---------------------------------------------------------------------------
preflight() {
  local missing=() need=() tool key
  for tool in cmake make; do
    command -v "${tool}" >/dev/null 2>&1 || { missing+=("${tool}"); need+=("${PKG_HINT[${tool}]}"); }
  done
  for key in "${ARCH_LIST[@]}"; do
    tool="${ARCH_CC[${key}]}"
    command -v "${tool}" >/dev/null 2>&1 || { missing+=("${tool}"); need+=("${PKG_HINT[${tool}]}"); }
  done
  if ((${#missing[@]})); then
    echo "error: missing tools: ${missing[*]}" >&2
    local pkgs; pkgs="$(printf '%s\n' "${need[@]}" | tr ' ' '\n' | sort -u | paste -sd' ')"
    echo "install them with:" >&2
    echo "    sudo apt-get install -y ${pkgs}" >&2
    exit 1
  fi
  # -fcallgraph-info is GCC >= 8 only; clang has no equivalent.
  for key in "${ARCH_LIST[@]}"; do
    if "${ARCH_CC[${key}]}" --version 2>/dev/null | head -1 | grep -qi clang; then
      echo "error: ${ARCH_CC[${key}]} is clang; -fcallgraph-info is a GCC feature" >&2
      exit 1
    fi
  done
}

# Build the library for one arch/config with the stack instrumentation on, and
# echo the directory the .ci/.su files ended up in.
build_one() {
  local dir="$1" arch_args="$2"; shift 2
  # Only the static library: no tests/bench/C++ keeps the configure step from
  # fetching Unity/Catch2 and keeps the graph to library code.
  # CMAKE_C_FLAGS is set() by the toolchain files, so the instrumentation goes
  # into CMAKE_C_FLAGS_RELEASE, which they leave alone. -DNDEBUG is restated
  # because overriding the variable drops CMake's own default for it — and
  # NDEBUG matters here: assert() would otherwise add calls and frames that no
  # shipped build has. Optimisation is left to the root CMakeLists, which
  # appends -Os after these flags, exactly as tools/footprint.sh gets it.
  # arch_args is intentionally word-split (its flags contain no spaces).
  # shellcheck disable=SC2086
  cmake -S "${ROOT}" -B "${dir}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS_RELEASE="-DNDEBUG -fstack-usage -fcallgraph-info=su,da" \
      -DSOFAB_ENABLE_CPP=OFF -DSOFAB_ENABLE_BENCH=OFF \
      -DSOFAB_BUILD_TESTS=OFF -DSOFAB_INSTALL=OFF \
      ${arch_args} "$@" >"${dir}.cmake.log" 2>&1
  cmake --build "${dir}" --target sofabuffers >"${dir}.build.log" 2>&1
}

# ---------------------------------------------------------------------------
main() {
  IFS=',' read -r -a ARCH_LIST <<<"${OPT_ARCHES}"
  local key found a
  for key in "${ARCH_LIST[@]}"; do
    found=0
    for a in "${ARCHES[@]}"; do
      if [[ "${a%%|*}" == "${key}" ]]; then found=1; fi
    done
    ((found)) || { echo "unknown arch: ${key} (known: armv6-m armv7-m rv32imc atmega8 host)" >&2; exit 2; }
  done
  mapfile -t CFG_EXTRA < <(sofab_config_args "${OPT_CONFIG}")   # validates the key too
  preflight

  rm -rf "${BUILD_DIR}"; mkdir -p "${BUILD_DIR}"

  local label overhead arch_args dir out worst_overall=0 rc=0
  declare -A SUMMARY   # SUMMARY["arch|group"] = "bytes name"

  for key in "${ARCH_LIST[@]}"; do
    for a in "${ARCHES[@]}"; do
      IFS='|' read -r _k label overhead arch_args <<<"${a}"
      [[ "${_k}" == "${key}" ]] || continue

      dir="${BUILD_DIR}/${OPT_CONFIG}-${key}"
      build_one "${dir}" "${arch_args}" "${CFG_EXTRA[@]}"

      out="${dir}.report"
      find "${dir}" -name '*.ci' -print0 \
        | xargs -0 awk -v OVERHEAD="${OPT_OVERHEAD:-${overhead}}" "${ANALYSER}" >"${out}"

      echo
      echo "== ${label} · config ${OPT_CONFIG} · call overhead ${OPT_OVERHEAD:-${overhead}} B/edge"

      if grep -q '^CYCLE' "${out}"; then
        echo "   ERROR: mutual recursion in the call graph — the per-level"
        echo "          decomposition does not hold; the figures below are wrong:"
        awk -F'\t' '$1=="CYCLE"{print "     " $2}' "${out}"
        rc=1
      fi

      # per entry point, deepest first
      local grp
      for grp in encode decode object; do
        awk -F'\t' -v g="${grp}" '$1=="ROOT" && $5==g' "${out}" | sort -t$'\t' -k2,2nr \
        | while IFS=$'\t' read -r _ total own name _ cb rec path; do
            note=""
            if ((cb)); then note="${note}, +callback"; fi
            if [[ "${rec}" != "-" ]]; then
              # "f=96,g=64" -> ", +160 B/level": both can be nested at the same
              # descriptor depth, so one more level costs the sum of their
              # frames. The breakdown is printed per target below.
              note="${note}, +$(echo "${rec}" | awk -F, \
                '{s=0; for (i=1;i<=NF;i++) {split($i,a,"="); s+=a[2]} print s}') B/level"
            fi
            printf '   %-7s %-42s %5s B  (own %4s B%s)\n' \
                   "${grp}" "${name}" "${total}" "${own}" "${note}"
            if ((OPT_PATHS)); then printf '   %-7s   %s\n' "" "${path}"; fi
          done
        # deepest of this group for the cross-arch summary
        SUMMARY["${key}|${grp}"]="$(awk -F'\t' -v g="${grp}" '
            $1=="ROOT" && $5==g && $2+0 > m { m=$2+0; n=$4 }
            END { if (n != "") printf "%d %s", m, n }' "${out}")"
      done

      local mx
      mx="$(awk -F'\t' '$1=="ROOT" && $2+0>m {m=$2+0} END{print m+0}' "${out}")"
      if ((mx > worst_overall)); then worst_overall="${mx}"; fi
      printf '   %-7s %-42s %5s B\n' "WORST" "any entry point, recursion depth 1" "${mx}"

      if awk -F'\t' '$1=="ROOT" && $7!="-"' "${out}" | grep -q .; then
        echo "   recursive — each further level of descriptor nesting adds:"
        awk -F'\t' '$1=="ROOT" && $7!="-"{n=split($7,a,","); for(i=1;i<=n;i++) print a[i]}' "${out}" \
          | sort -u | awk -F= '{printf "     %-40s %4s B\n", $1, $2}'
      fi
      if grep -q '^DYN' "${out}"; then
        echo "   non-static frames (the figure above is not a ceiling for these):"
        awk -F'\t' '$1=="DYN"{printf "     %s — %s\n", $2, $3}' "${out}"
        rc=1
      fi
      if grep -q '^IND' "${out}"; then
        printf '   indirect calls into caller code (counted as 0): '
        awk -F'\t' '$1=="IND"{printf "%s ", $2} END{print ""}' "${out}"
      fi
      if grep -q '^EXT' "${out}"; then
        printf '   external callees (counted as 0): '
        awk -F'\t' '$1=="EXT"{printf "%s ", $2} END{print ""}' "${out}"
      fi
      echo "   per-function detail: ${dir}/src/CMakeFiles/sofabuffers.dir/*.su"
    done
  done

  # --- cross-target summary ------------------------------------------------
  echo
  echo "==================== worst case per target (config ${OPT_CONFIG}) ===================="
  printf '| %-22s | %-9s | %-9s | %-9s |\n' "Target" "encode" "decode" "object"
  printf '| %-22s | %-9s | %-9s | %-9s |\n' "----------------------" "---------" "---------" "---------"
  for key in "${ARCH_LIST[@]}"; do
    for a in "${ARCHES[@]}"; do
      IFS='|' read -r _k label _ _ <<<"${a}"
      [[ "${_k}" == "${key}" ]] || continue
      printf '| %-22s | %-9s | %-9s | %-9s |\n' "${label}" \
        "$(echo "${SUMMARY["${key}|encode"]:-}" | awk '{print ($1=="")?"n/a":$1" B"}')" \
        "$(echo "${SUMMARY["${key}|decode"]:-}" | awk '{print ($1=="")?"n/a":$1" B"}')" \
        "$(echo "${SUMMARY["${key}|object"]:-}" | awk '{print ($1=="")?"n/a":$1" B"}')"
    done
  done
  echo
  echo "Deepest entry point per group, at recursion depth 1. Excludes the caller's"
  echo "own frame and any callback the library invokes; the object column grows by"
  echo "the per-level figure above for every level of descriptor nesting."

  if [[ -n "${OPT_BUDGET}" ]]; then
    if ((worst_overall > OPT_BUDGET)); then
      echo "FAIL: worst case ${worst_overall} B exceeds the budget of ${OPT_BUDGET} B" >&2
      rc=1
    else
      echo "OK: worst case ${worst_overall} B is within the budget of ${OPT_BUDGET} B"
    fi
  fi
  return "${rc}"
}

main "$@"
