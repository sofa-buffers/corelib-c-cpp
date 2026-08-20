#!/usr/bin/env bash
#
# build-one.sh — build, measure and (where the target can run) test ONE
# configuration of this corelib.
#
# The four configurations used to be a CI matrix axis, which meant every leg
# re-installed the same cross toolchain before doing 8 seconds of work: 20
# Cortex-M legs spent 12.9 of the workflow's 15.6 minutes on apt alone. They are
# steps now, four per job, and the toolchain is installed once per target. Each
# step still reports its own name, duration and log, and each is marked
# `if: always()` so a failing configuration never hides the other three.
#
# Everything that differs per target arrives through the environment, so the
# per-target workflows stay a package list plus a handful of CMake flags:
#
#   SOFAB_CMAKE_ARGS   newline-separated extra CMake arguments (one per line, so
#                      an argument containing a space stays one argv element —
#                      the same reason build-config.sh emits one per line)
#   SOFAB_TARGET_KIND  hosted | baremetal — picks the build target set
#   SOFAB_RUN_TESTS    1 to run ctest after the build (hosted targets only)
#   SOFAB_VERIFY_CXX   1 to also inspect the C++ test binary
#   SOFAB_VERIFY_C     1 to inspect the C test binary (default 1)
#   SOFAB_CPP_FROM_CONFIG  1 to derive -DSOFAB_ENABLE_CPP from the configuration
#   SOFAB_BUILD_TARGET     override the CMake target (riscv32 builds the library
#                          alone, with SOFAB_BUILD_TESTS=OFF)
#
# Usage:  utils/ci/build-one.sh full
set -euo pipefail

config="${1:?usage: build-one.sh <config>}"
kind="${SOFAB_TARGET_KIND:-hosted}"
dir="build/$config"

. utils/ci/build-config.sh
mapfile -t CFG < <(sofab_config_args "$config")

EXTRA=()
if [[ -n "${SOFAB_CMAKE_ARGS:-}" ]]; then
  mapfile -t EXTRA < <(printf '%s\n' "$SOFAB_CMAKE_ARGS" | sed '/^[[:space:]]*$/d')
fi

if [[ "${SOFAB_CPP_FROM_CONFIG:-0}" == 1 ]]; then
  EXTRA+=("-DSOFAB_ENABLE_CPP=$(sofab_config_cpp "$config")")
fi

target="${SOFAB_BUILD_TARGET:-$(sofab_config_target "$config" "$kind")}"

echo "::group::configure and build ($config)"
cmake -S . -B "$dir" "${EXTRA[@]}" "${CFG[@]}"
cmake --build "$dir" --target "$target" --parallel "$(nproc)"
echo "::endgroup::"

echo "::group::library size ($config)"
size "$dir/src/libsofabuffers.a"
echo "::endgroup::"

# The test binaries only exist for the `full*` configurations; the reduced ones
# build the library alone (see build-config.sh).
if [[ "$config" == full* ]]; then
  if [[ "${SOFAB_VERIFY_C:-1}" == 1 ]]; then
    echo "::group::verify C binary ($config)"
    ( cd "$dir/test/c" && file sofabtest && readelf -h -A sofabtest )
    echo "::endgroup::"
  fi
  if [[ "${SOFAB_VERIFY_CXX:-0}" == 1 ]]; then
    echo "::group::verify C++ binary ($config)"
    ( cd "$dir/test/cpp" && file sofabpptest && readelf -h -A sofabpptest )
    echo "::endgroup::"
  fi
fi

if [[ "${SOFAB_RUN_TESTS:-0}" == 1 ]]; then
  echo "::group::tests ($config)"
  if [[ "$config" == full* ]]; then
    ( cd "$dir" && ctest --output-on-failure --verbose )
  else
    # A reduced configuration cannot build the max-only suites; the shared
    # vectors are what it still has to satisfy.
    ( cd "$dir" && ctest -R test_vectors_c --output-on-failure --verbose )
  fi
  echo "::endgroup::"
fi
