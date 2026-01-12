# --- test executable ---
add_executable(sofabfuzz
    main.c
)

target_compile_options(sofabfuzz
    PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
)

# --- link libraries ---
target_link_libraries(sofabfuzz
    sofabuffers
)

# --- ctest registration ---
add_test(NAME fuzz COMMAND sofabfuzz)