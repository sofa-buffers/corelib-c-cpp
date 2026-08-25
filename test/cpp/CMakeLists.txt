# ----------------------------------------
# Catch2 via FetchContent
# ----------------------------------------

# Catch2 requires C++ exceptions. The core library is built with
# -fno-exceptions for non-coverage (embedded) builds, so re-enable them for the
# whole C++ test build — both Catch2 and the test executable — to override that
# inherited flag.
add_compile_options(-fexceptions)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.11.0
)

FetchContent_MakeAvailable(Catch2)

# --- test executable ---
# The conformance vectors validate the C core; the C++ layer is only a thin
# wrapper over it, so there is no separate C++ vector runner. These are the
# hand-written tests that exercise the C++ API (sofab.hpp) directly.
add_executable(sofabpptest
    main.cpp
    test_ostream.cpp
    test_istream.cpp
    test_seq.cpp
    test_receiver_limits.cpp
    test_allocation.cpp
)

target_compile_options(sofabpptest
    PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
)

# --- link libraries ---
target_link_libraries(sofabpptest
    sofabuffers
    Catch2::Catch2WithMain
)

# --- ctest registration ---
add_test(NAME test_cpp COMMAND sofabpptest)

# The README's generator example, compiled and run. The stand-in lives here as a
# real translation unit; the test also checks that README.md still shows calls it
# implements, so a documented example cannot drift from one that compiles. Kept
# out of sofabpptest because it brings no Catch2 and must build wherever the C++
# wrapper does.
add_executable(test_readme_example test_readme_example.cpp)
target_compile_options(test_readme_example PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(test_readme_example PRIVATE
    SOFAB_README_PATH="${PROJECT_SOURCE_DIR}/README.md"
    SOFAB_SELF_PATH="${CMAKE_CURRENT_LIST_DIR}/test_readme_example.cpp")
target_link_libraries(test_readme_example sofabuffers)
add_test(NAME test_readme_example COMMAND test_readme_example)
