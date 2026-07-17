# --- core library sources ---
# utf8.c holds the hand-written UTF-8 validator behind SOFAB_STRICT_UTF8; it
# compiles to nothing when the check is disabled (SOFAB_DISABLE_STRICT_UTF8), so
# it stays in the source list unconditionally and costs zero .text when OFF.
set(SOFAB_SOURCES
    ostream.c
    istream.c
    utf8.c
)

# The object descriptor API lives entirely in object.c; disabling it drops
# that API and shrinks the library to the bare stream corelib. Opt-out: the
# object API is built by default unless SOFAB_DISABLE_OBJECT_API is set.
option(SOFAB_DISABLE_OBJECT_API "Exclude the object descriptor API (object.c)" OFF)
if(NOT SOFAB_DISABLE_OBJECT_API)
    list(APPEND SOFAB_SOURCES object.c)
endif()

add_library(sofabuffers ${SOFAB_SOURCES})

# Namespaced alias for modern-CMake consumers (matches the Conan package name
# sofa-buffers-corelib-c-cpp). Additive: the sofabuffers target is unchanged.
add_library(sofa-buffers::corelib ALIAS sofabuffers)

# BUILD_INTERFACE points at the in-tree headers; INSTALL_INTERFACE at the
# headers copied under <prefix>/include so installed/packaged consumers resolve
# #include <sofab/...> correctly.
target_include_directories(sofabuffers PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_options(sofabuffers PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp)

# --- optional feature toggles ---
# Each option drops a slice of the wire format to shrink the build. The matching
# SOFAB_DISABLE_* macro also appears in the *public* headers, so it is applied
# PUBLIC: it is baked into the installed INTERFACE and every downstream consumer
# (in-tree, find_package, Conan) compiles the headers with the same
# configuration. Without this the library and a consumer's headers could
# disagree on which fields exist. Default OFF keeps the full feature set.
#
# (SOFAB_DISABLE_OBJECT_API is handled above because it selects a source file
# rather than a header guard.)
foreach(_sofab_feature
        ARRAY_SUPPORT
        SEQUENCE_SUPPORT
        FIXLEN_SUPPORT
        FP64_SUPPORT
        INT64_SUPPORT
        INTEGER_OVERFLOW_CHECK
        STRICT_UTF8)
    option(SOFAB_DISABLE_${_sofab_feature}
        "Exclude ${_sofab_feature} from the SofaBuffers wire format" OFF)
    if(SOFAB_DISABLE_${_sofab_feature})
        target_compile_definitions(sofabuffers PUBLIC SOFAB_DISABLE_${_sofab_feature})
    endif()
endforeach()

find_program(SIZE_EXECUTABLE NAMES size)
if(SIZE_EXECUTABLE)
    add_custom_command(TARGET sofabuffers POST_BUILD
        COMMAND "${SIZE_EXECUTABLE}" "$<TARGET_FILE:sofabuffers>"
        COMMENT "Displaying size of sofabuffers"
        VERBATIM
    )
endif()

# ----------------------------------------------------------------------------
# Install / export rules
# ----------------------------------------------------------------------------
# These make the library consumable via find_package() and drive the Conan
# recipe (see conanfile.py). Guarded so the
# rules stay out of the way when this project is embedded via add_subdirectory.
option(SOFAB_INSTALL "Generate install and CMake package-config rules" ON)
if(SOFAB_INSTALL)
    include(../cmake/sofab_install.cmake)
endif()