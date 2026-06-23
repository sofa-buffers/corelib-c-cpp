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
add_executable(sofabpptest
    main.cpp
    test_ostream.cpp
    test_istream.cpp
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
