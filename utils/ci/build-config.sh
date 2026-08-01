#!/usr/bin/env bash
#
# build-config.sh — the four build configurations, defined once.
#
# The README footprint tables describe four configurations of this corelib, and
# three different consumers need to agree on exactly what each one means:
# tools/footprint.sh (which regenerates the tables), the per-target CI workflows
# (which build them), and anyone reproducing either by hand. Spelling the flags
# out in each place is how the tables and CI drift apart, so they are spelled out
# here instead and everything else sources this file.
#
# Usage (source it, then call the helpers):
#
#   . utils/ci/build-config.sh
#   mapfile -t CFG < <(sofab_config_args minimal)
#   cmake -S . -B build "${CFG[@]}"
#   cmake --build build --target "$(sofab_config_target minimal baremetal)"
#
# sofab_config_args emits one CMake argument per line; read it with mapfile,
# never with $(...) unquoted, so an argument that ever grows a space stays one
# argv element.
#
# The switches are passed as CMake *options*, not as raw compiler flags. Two
# reasons, both of which have bitten this repo:
#   - the per-target toolchain files assign CMAKE_C_FLAGS with set(), so a
#     -DCMAKE_C_FLAGS=... on the command line is silently discarded and the
#     build comes out non-minimal while looking configured;
#   - CMAKE_C_FLAGS_RELEASE only applies to Release builds, so the same override
#     would quietly do nothing in the hosted CI workflows, which build Debug.
# As options they also reach the tests and any consumer, because src/CMakeLists
# applies them PUBLIC — which is required: they change the public headers.

# --- the configurations ------------------------------------------------------
#   full           everything on — the shipped default
#   full-strict    plus SOFAB_ENABLE_STRICT_UTF8, the only configuration that
#                  compiles utf8.c in (CORELIB_PLAN §6.4)
#   minimal        the constrained wire profile: no fixlen, arrays or sequences,
#                  no integer overflow check, smallest descriptor profile; the
#                  object API is still built
#   minimal-noobj  as minimal, additionally without object.c
SOFAB_CONFIGS=(full full-strict minimal minimal-noobj)

# Feature switches that define the "minimal" wire format.
SOFAB_MIN_OPTIONS=(
    -DSOFAB_DISABLE_FIXLEN_SUPPORT=ON
    -DSOFAB_DISABLE_ARRAY_SUPPORT=ON
    -DSOFAB_DISABLE_SEQUENCE_SUPPORT=ON
    -DSOFAB_DISABLE_INTEGER_OVERFLOW_CHECK=ON
    -DSOFAB_OBJECT_DESCR_PROFILE=SOFAB_OBJECT_DESCR_SMALL
)

# ---------------------------------------------------------------------------
# sofab_config_args <config>
#
# Print the extra CMake arguments for one configuration, one per line. `full`
# prints nothing — it is the default configuration and adds no flags.
# ---------------------------------------------------------------------------
sofab_config_args() {
    case "$1" in
        full)
            ;;
        full-strict)
            printf '%s\n' "-DSOFAB_ENABLE_STRICT_UTF8=ON"
            ;;
        minimal)
            printf '%s\n' "${SOFAB_MIN_OPTIONS[@]}"
            ;;
        minimal-noobj)
            printf '%s\n' "-DSOFAB_DISABLE_OBJECT_API=ON" "${SOFAB_MIN_OPTIONS[@]}"
            ;;
        *)
            echo "unknown configuration: $1 (expected one of: ${SOFAB_CONFIGS[*]})" >&2
            return 1
            ;;
    esac
}

# ---------------------------------------------------------------------------
# sofab_config_cpp <config>
#
# Print ON or OFF for SOFAB_ENABLE_CPP. The header-only C++ wrapper requires
# fixlen support and says so with an #error, so it cannot be built in a reduced
# configuration at all — and leaving it enabled there only makes the configure
# step fetch Catch2 for tests that will never be built.
# ---------------------------------------------------------------------------
sofab_config_cpp() {
    case "$1" in
        full|full-strict)       echo "ON" ;;
        minimal|minimal-noobj)  echo "OFF" ;;
        *) echo "unknown configuration: $1" >&2; return 1 ;;
    esac
}

# ---------------------------------------------------------------------------
# sofab_config_target <config> <profile>
#
# Print the build target for a configuration. <profile> is `hosted` (the target
# can run its own binaries, natively or under QEMU) or `baremetal`.
#
# The hand-written unit tests are deliberately max-only — they exercise APIs a
# reduced configuration removes — so a reduced build cannot build everything.
# What it CAN build is the flag-tolerant vector runner, which skips the vectors
# whose capabilities are compiled out; on bare metal even that is unavailable
# (it reads the JSON from a filesystem), leaving the library itself.
# ---------------------------------------------------------------------------
sofab_config_target() {
    case "$1" in
        full|full-strict)
            echo "all"
            ;;
        minimal|minimal-noobj)
            case "$2" in
                hosted)    echo "sofab_vectortest" ;;
                baremetal) echo "sofabuffers" ;;
                *) echo "unknown profile: $2 (expected hosted or baremetal)" >&2; return 1 ;;
            esac
            ;;
        *)
            echo "unknown configuration: $1" >&2
            return 1
            ;;
    esac
}
