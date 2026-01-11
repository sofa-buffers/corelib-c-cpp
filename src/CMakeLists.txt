add_library(sofabuffers
    ostream.c
    istream.c
)

target_include_directories(sofabuffers PUBLIC include)
target_compile_options(sofabuffers PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp)

find_program(SIZE_EXECUTABLE NAMES size)
if(SIZE_EXECUTABLE)
    add_custom_command(TARGET sofabuffers POST_BUILD
        COMMAND "${SIZE_EXECUTABLE}" "$<TARGET_FILE:sofabuffers>"
        COMMENT "Displaying size of sofabuffers"
        VERBATIM
    )
endif()