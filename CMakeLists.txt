cmake_minimum_required(VERSION 3.10)
project(sofabuffers
    VERSION 0.8.0
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
        add_compile_options(-Os -fno-asynchronous-unwind-tables -fno-exceptions)
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

# --- compilation flags ---
add_compile_options(-ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections -Wl,-Map=mapfile.map)

# --- build SofaBuffers core library ---
add_subdirectory(src)

# --- enable cmake testing ---
enable_testing()

# --- build C tests ---
add_subdirectory(test/c)

# --- build C++ tests ---
option(SOFAB_ENABLE_CPP "Enable C++ test build" ON)
if(SOFAB_ENABLE_CPP)
    add_subdirectory(test/cpp)
endif()

# --- build fuzz tests ---
if(SOFAB_ENABLE_FUZZ)
    add_subdirectory(test/fuzz)
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
