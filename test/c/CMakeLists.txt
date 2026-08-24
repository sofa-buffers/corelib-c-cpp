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
    test_utf8.c
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

# ----------------------------------------
# Standalone, feature-flag-tolerant conformance-vector runner
# ----------------------------------------
# The hand-written unit tests above are built only in the full "max"
# configuration. This separate runner links just the public library + JSON
# reader + shared vector engine, so it compiles under ANY combination of
# SOFAB_DISABLE_* flags and skips the vectors that need a disabled feature
# (each vector declares its "requires" capabilities). CI builds it across the
# feature matrix to exercise every configuration; build it on its own with
#   cmake --build <dir> --target sofab_vectortest
# It needs a filesystem to read the JSON, so it is hosted-targets only.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    add_executable(sofab_vectortest
        vectors_main.c
        ../shared/sofab_test_json.c
        ../shared/sofab_test_vectors.c
    )
    target_compile_options(sofab_vectortest
        PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
    )
    target_include_directories(sofab_vectortest PRIVATE ../shared)
    target_compile_definitions(sofab_vectortest PRIVATE
        SOFAB_TEST_VECTORS_PATH="${CMAKE_SOURCE_DIR}/assets/test_vectors.json"
    )
    target_link_libraries(sofab_vectortest sofabuffers)
    add_test(NAME test_vectors_c COMMAND sofab_vectortest)
endif()

# ----------------------------------------
# Constrained-ceiling suite (§6.2 / issue #150)
# ----------------------------------------
# SOFAB_FIXLEN_MAX / SOFAB_ARRAY_MAX drop to 65,535 where size_t is 16 bits, and
# the checks that read them live inside the library — so the lowered ceiling
# cannot be exercised by linking against `sofabuffers`, which was compiled with
# the wide one. This target therefore recompiles the library sources with the
# ceilings pinned to the constrained values, which is what lets a 64-bit host run
# the 16-bit profile's decode and encode behaviour. Nothing else in the build is
# affected: the shipped library keeps its own ceilings.
#
# The sources are listed here rather than reused from src/CMakeLists.txt because
# that file builds a *target*, not a source list. It is the same three files the
# stream codec is made of (object.c is not linked — this suite drives the stream
# API directly); if a fourth ever joins them and this list is not updated, the
# link fails loudly rather than silently testing less.
#
# Hosted-only (it has to run) and full-configuration-only (it writes blobs and
# arrays, which a reduced profile removes).
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic"
   AND NOT SOFAB_DISABLE_FIXLEN_SUPPORT
   AND NOT SOFAB_DISABLE_ARRAY_SUPPORT)
    add_executable(sofabtest_limits
        test_limits.c
        ${CMAKE_SOURCE_DIR}/src/istream.c
        ${CMAKE_SOURCE_DIR}/src/ostream.c
        ${CMAKE_SOURCE_DIR}/src/utf8.c
    )
    target_compile_options(sofabtest_limits
        PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
    )
    target_include_directories(sofabtest_limits
        PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    # Inherit whatever feature configuration the library was configured with, so
    # this suite tracks the surrounding build (notably SOFAB_ENABLE_STRICT_UTF8 in
    # the full-strict configuration) and differs from it in the ceilings alone.
    target_compile_definitions(sofabtest_limits PRIVATE
        $<TARGET_PROPERTY:sofabuffers,INTERFACE_COMPILE_DEFINITIONS>
        SOFAB_FIXLEN_MAX=65535
        SOFAB_ARRAY_MAX=65535
    )
    target_link_libraries(sofabtest_limits unity)
    add_test(NAME test_limits_c COMMAND sofabtest_limits)
endif()