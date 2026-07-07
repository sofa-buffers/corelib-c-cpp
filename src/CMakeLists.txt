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

# Namespaced alias for modern-CMake consumers (matches the vcpkg/Conan port
# name sofa-buffers-corelib-c-cpp). Additive: the sofabuffers target is unchanged.
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
# (in-tree, find_package, vcpkg, Conan) compiles the headers with the same
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
        INTEGER_OVERFLOW_CHECK)
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
# These make the library consumable via find_package() and drive the vcpkg and
# Conan ports (see ports/sofa-buffers-corelib and conanfile.py). Guarded so the
# rules stay out of the way when this project is embedded via add_subdirectory.
option(SOFAB_INSTALL "Generate install and CMake package-config rules" ON)
if(SOFAB_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(SOFAB_PACKAGE_NAME sofa-buffers-corelib-c-cpp)
    set(SOFAB_CMAKE_CONFIG_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${SOFAB_PACKAGE_NAME})

    # Export the underlying `sofabuffers` target as `sofa-buffers::corelib` so
    # the installed name matches the in-tree ALIAS.
    set_target_properties(sofabuffers PROPERTIES EXPORT_NAME corelib)

    install(TARGETS sofabuffers
        EXPORT ${SOFAB_PACKAGE_NAME}-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    # Public headers: src/include/sofab/*.{h,hpp} -> <prefix>/include/sofab/.
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/sofab
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT ${SOFAB_PACKAGE_NAME}-targets
        FILE ${SOFAB_PACKAGE_NAME}-targets.cmake
        NAMESPACE sofa-buffers::
        DESTINATION ${SOFAB_CMAKE_CONFIG_DESTINATION})

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/${SOFAB_PACKAGE_NAME}-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${SOFAB_PACKAGE_NAME}-config.cmake
        INSTALL_DESTINATION ${SOFAB_CMAKE_CONFIG_DESTINATION})

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${SOFAB_PACKAGE_NAME}-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${SOFAB_PACKAGE_NAME}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${SOFAB_PACKAGE_NAME}-config-version.cmake
        DESTINATION ${SOFAB_CMAKE_CONFIG_DESTINATION})
endif()