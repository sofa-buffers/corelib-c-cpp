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
# Most of these are hand-written tests that exercise the C++ API (sofab.hpp)
# directly. test_vector_blocks.cpp is the exception: it runs the two
# test_vectors.json blocks the shared C engine cannot, because they need the
# receiver caps and the growable containers only this wrapper has.
add_executable(sofabpptest
    main.cpp
    test_ostream.cpp
    test_istream.cpp
    test_seq.cpp
    test_floats.cpp
    test_receiver_limits.cpp
    test_allocation.cpp
    test_vector_blocks.cpp
    test_encode_bounds.cpp
    ../shared/sofab_test_json.c
)

target_include_directories(sofabpptest PRIVATE ../shared)
target_compile_definitions(sofabpptest PRIVATE
    SOFAB_TEST_VECTORS_PATH="${CMAKE_SOURCE_DIR}/assets/test_vectors.json"
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

# SOFAB_DISABLE_ENCODE_BOUNDS: compile-only (static_asserts), an object of its
# own so the two values of sofab::ENCODE_BOUNDS never meet in one program.
add_library(sofab_encode_bounds_off OBJECT encode_bounds_off.cpp)
target_compile_options(sofab_encode_bounds_off PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(sofab_encode_bounds_off PRIVATE sofabuffers)

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
