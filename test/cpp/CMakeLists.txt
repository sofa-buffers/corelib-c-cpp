# ----------------------------------------
# Catch2 via FetchContent
# ----------------------------------------
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
