# ----------------------------------------
# Unity via FetchContent
# ----------------------------------------
include(FetchContent)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
)

FetchContent_MakeAvailable(unity)

# --- unity configuration ---
target_compile_options(unity
    PUBLIC
        -DUNITY_SUPPORT_64
        -DUNITY_INCLUDE_DOUBLE
)

# --- test executable ---
add_executable(sofabtest
    main.c
    test_ostream.c
    test_istream.c
    test_object.c
    test_vectors.c
)

target_compile_options(sofabtest
    PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
)

# The shared-vector harness reads assets/test_vectors.json from disk, so it is
# only wired in for hosted targets (host + QEMU-run Linux cross). Bare-metal
# "Generic" targets have no filesystem; there test_vectors.c compiles to a no-op
# and the JSON parser / engine are not linked at all.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    target_sources(sofabtest PRIVATE
        ../shared/sofab_test_json.c
        ../shared/sofab_test_vectors.c
    )
    target_include_directories(sofabtest PRIVATE ../shared)
    target_compile_definitions(sofabtest PRIVATE
        SOFAB_TEST_VECTORS=1
        SOFAB_TEST_VECTORS_PATH="${CMAKE_SOURCE_DIR}/assets/test_vectors.json"
    )
endif()

# --- add startup code and stubs for bare metal targets ---
if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "avr")
        target_sources(sofabtest
            PRIVATE
                ../../utils/avr/startup.c)

    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "rl78")
        target_sources(sofabtest
            PRIVATE
                ../../utils/rl78/startup.c)

    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
        target_sources(sofabtest
            PRIVATE
                ../../utils/cortex-m/startup.c
                ../../utils/cortex-m/newlib_stubs.c)
    endif()
endif()

# --- link libraries ---
target_link_libraries(sofabtest
    sofabuffers
    unity
)

# --- ctest registration ---
add_test(NAME test_c COMMAND sofabtest)