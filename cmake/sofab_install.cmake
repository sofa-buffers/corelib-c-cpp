include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(SOFAB_PACKAGE_NAME sofa-buffers-corelib-c-cpp)
set(SOFAB_CMAKE_CONFIG_DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/${SOFAB_PACKAGE_NAME})

# Export the underlying `sofabuffers` target as `sofa-buffers::corelib` so the
# installed name matches the in-tree ALIAS.
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
