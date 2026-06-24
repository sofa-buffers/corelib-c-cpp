# --- core library sources ---
set(SOFAB_SOURCES
    ostream.c
    istream.c
)

# The object descriptor API lives entirely in object.c; disabling it drops
# that API and shrinks the library to the bare stream corelib. Opt-out: the
# object API is built by default unless SOFAB_DISABLE_OBJECT_API is set.
option(SOFAB_DISABLE_OBJECT_API "Exclude the object descriptor API (object.c)" OFF)
if(NOT SOFAB_DISABLE_OBJECT_API)
    list(APPEND SOFAB_SOURCES object.c)
endif()

add_library(sofabuffers ${SOFAB_SOURCES})

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