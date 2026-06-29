# ----------------------------------------
# C++ wrapper smoke test (reduced-config coverage)
# ----------------------------------------
#
# A standalone, Catch2-free C++ translation unit that includes sofab.hpp and
# round-trips a message using only capabilities that survive every gracefully
# supported reduced build (see smoke.cpp). It exists so the feature matrix can
# compile-and-run the C++ wrapper under each SOFAB_DISABLE_* configuration —
# coverage the full unit tests (max-build-only) cannot provide.

# The core library is built with -fno-exceptions for non-coverage builds; the
# std::string / std::vector used here may throw, so re-enable exceptions for
# this target as the full C++ tests do.
add_compile_options(-fexceptions)

add_executable(sofab_cpp_smoke
    smoke.cpp
)

target_compile_options(sofab_cpp_smoke
    PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
)

target_link_libraries(sofab_cpp_smoke
    sofabuffers
)

add_test(NAME test_cpp_smoke COMMAND sofab_cpp_smoke)
