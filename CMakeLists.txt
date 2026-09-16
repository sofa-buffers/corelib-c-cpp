cmake_minimum_required(VERSION 3.10)
# The project VERSION feeds the installed CMake package-config version file
# (src/CMakeLists.txt) and is kept in lockstep with the package manifests by the
# "Version consistency" workflow. Keep VERSION on its own line for that check.
project(sofabuffers
    VERSION 0.11.0
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_LIBDIR lib)

# --- coverage ---
option(SOFAB_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
if(SOFAB_ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU")
        add_compile_options(-O0 -g --coverage)
        add_link_options(--coverage)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(-O0 -g -fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)
    endif()
else()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # -fno-exceptions keeps the C++ wrapper free of the libstdc++
        # exception/unwinder runtime, whose static state (emergency pool, DWARF
        # frame tables + mutexes, terminate handler) otherwise lands in .bss.
        # -fno-rtti additionally drops the RTTI type_info the wrapper's virtual
        # bases emit; that lives in .data.rel.ro (not .bss), so it is a small
        # rodata/code saving, not a .bss reduction. -fno-rtti is C++-only, so
        # scope it to CXX to avoid a "not valid for C" warning on the C sources.
        # Catch2-based C++ tests re-enable exceptions via -fexceptions in their
        # own CMakeLists (Catch2 builds fine with RTTI off, so -fno-rtti is left
        # inherited there).
        add_compile_options(-Os -fno-asynchronous-unwind-tables -fno-exceptions
            $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    endif()
endif()

# --- fuzzing ---
option(SOFAB_ENABLE_FUZZ "Enable fuzzing instrumentation" OFF)
if(SOFAB_ENABLE_FUZZ)
    if(COMPILER_CLANG)
        add_compile_options(
            -O1 -g -fno-omit-frame-pointer
            -fsanitize=address,undefined,integer
        )
        add_link_options(
            -fsanitize=address,undefined,integer
        )
    elseif(COMPILER_GCC)
        add_compile_options(
            -O1 -g -fno-omit-frame-pointer
            -fsanitize=address,undefined,signed-integer-overflow,shift
        )
        add_link_options(
            -fsanitize=address,undefined,signed-integer-overflow,shift
        )
    endif()
endif()

# --- link-time optimization ---
# Off by default; CI turns it on for one host leg per compiler (see the
# "x86_64 (GCC, LTO)" / "x86_64 (Clang, LTO)" entries in .github/workflows/ci.yml).
#
# Why this exists at all: the C core and the C++ wrapper are separate
# translation units, so ordinarily no compiler sees a C++ type and the C code
# that reads it at the same time. LTO is the one configuration that does, and it
# is not exotic for this library's audience -- it is a standard way to claw back
# .text on the embedded targets this corelib exists for. Any type-based aliasing
# assumption across that boundary is only exercised here, and no sanitizer
# substitutes: UBSan does not model strict aliasing, so a green
# -fsanitize=undefined run says nothing about it.
#
# -fstrict-aliasing is stated rather than inherited from the optimisation level.
# Both -Os and -O3 already imply it on GCC and Clang, so the flag changes no
# behaviour today; it is here so that this configuration keeps meaning what its
# name says if the default flags below ever pick up a -fno-strict-aliasing.
#
# A raw -DCMAKE_C_FLAGS=-flto would not work: the per-target toolchain files
# assign CMAKE_C_FLAGS with set(), which discards it (see utils/ci/build-config.sh).
option(SOFAB_ENABLE_LTO "Build with LTO and strict aliasing across the C/C++ boundary" OFF)

# --- compilation flags ---
# GCC/Clang-only size flags; skipped on other toolchains (e.g. MSVC) so the
# library still configures cleanly when built as a Conan package.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections -Wl,-Map=mapfile.map)

    if(SOFAB_ENABLE_LTO)
        # -O3 rather than the -Os above: this leg is not measuring footprint, it
        # is trying to give the optimiser every reason to act on a type-based
        # alias assumption. It lands after the -Os in add_compile_options order,
        # so it is the -O that survives onto the command line.
        add_compile_options(-O3 -flto -fstrict-aliasing)
        add_link_options(-O3 -flto -fstrict-aliasing)

        # With -fno-fat-lto-objects (the default since GCC 4.9, and always for
        # Clang) an object file carries bytecode instead of symbols, so plain
        # ar/ranlib write an archive index the linker cannot resolve against.
        # The compiler-prefixed wrappers load the LTO plugin and index it
        # properly. Not left to binutils plugin auto-loading, which is a
        # distribution build option rather than something this build can assume.
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            find_program(SOFAB_LTO_AR NAMES gcc-ar)
            find_program(SOFAB_LTO_RANLIB NAMES gcc-ranlib)
        else()
            find_program(SOFAB_LTO_AR NAMES llvm-ar)
            find_program(SOFAB_LTO_RANLIB NAMES llvm-ranlib)
        endif()
        if(SOFAB_LTO_AR AND SOFAB_LTO_RANLIB)
            set(CMAKE_AR "${SOFAB_LTO_AR}")
            set(CMAKE_RANLIB "${SOFAB_LTO_RANLIB}")
        else()
            message(WARNING
                "SOFAB_ENABLE_LTO: no LTO-aware ar/ranlib found; falling back to "
                "${CMAKE_AR}, which needs binutils plugin auto-loading to link.")
        endif()
    endif()
endif()

# --- build SofaBuffers core library ---
# The namespaced sofa-buffers::corelib alias is defined in src/CMakeLists.txt,
# next to the target it aliases.
add_subdirectory(src)

# --- tests ---
# The test suites pull in Unity/Catch2 via FetchContent, so package builds
# (Conan) turn them off with -DSOFAB_BUILD_TESTS=OFF to keep the build
# dependency-free.
option(SOFAB_BUILD_TESTS "Build the C/C++ test suites" ON)
if(SOFAB_BUILD_TESTS)
    # --- enable cmake testing ---
    enable_testing()

    # --- build C tests ---
    add_subdirectory(test/c)

    # --- build C++ tests ---
    option(SOFAB_ENABLE_CPP "Enable C++ test build" ON)
    if(SOFAB_ENABLE_CPP)
        add_subdirectory(test/cpp)
    endif()

    # --- build C++ wrapper smoke test ---
    # A Catch2-free single-TU round-trip used to compile (and run) the C++
    # wrapper under reduced SOFAB_DISABLE_* configs, where the full Catch2 unit
    # tests cannot build. Kept separate so the feature matrix can enable it
    # without pulling in Catch2 (and with SOFAB_ENABLE_CPP=OFF).
    option(SOFAB_ENABLE_CPP_SMOKE "Build the C++ wrapper smoke test" OFF)
    if(SOFAB_ENABLE_CPP_SMOKE)
        add_subdirectory(test/cpp-smoke)
    endif()

    # --- C++ wrapper freestanding compile guard ---
    # Compiles one no-heap translation unit with -ffreestanding, holding the
    # SOFAB_CPP_HAVE_HOSTED split in sofab.hpp closed. Compile-only and
    # Catch2-free, so it costs a single object file; on by default because
    # nothing else covers the bare-metal C++ surface (every MCU CI job builds
    # with SOFAB_ENABLE_CPP=OFF). Needs a C++ compiler but not the hosted
    # library, hence its own option rather than riding on SOFAB_ENABLE_CPP.
    option(SOFAB_ENABLE_CPP_FREESTANDING
        "Build the C++ wrapper freestanding compile guard" ON)
    if(SOFAB_ENABLE_CPP_FREESTANDING)
        add_subdirectory(test/cpp-freestanding)
    endif()
endif()

# --- build throughput benchmarks ---
option(SOFAB_ENABLE_BENCH "Build the throughput benchmarks (bench_c / bench_cpp)" ON)
if(SOFAB_ENABLE_BENCH)
    add_subdirectory(bench)
endif()

# --- build fuzz tests ---
if(SOFAB_ENABLE_FUZZ)
    add_subdirectory(test/fuzz)
endif()

# --- test-vector generator (host only) ---
option(SOFAB_ENABLE_VECTORGEN "Build the JSON test-vector generator" OFF)
if(SOFAB_ENABLE_VECTORGEN)
    add_subdirectory(test/vectorgen)
endif()

# --- doxygen target ---
option(SOFAB_ENABLE_DOXYGEN "Enable Doxygen documentation generation" OFF)
if(SOFAB_ENABLE_DOXYGEN)
    find_package(Doxygen REQUIRED)

    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_BINARY_DIR}/Doxyfile)
    set(DOXYGEN_HTML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/docs/html)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(doc
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            ${DOXYGEN_HTML_OUTPUT_DIR}/assets
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()
