# SofaBuffers corelib is a header + static-library port built straight from the
# upstream CMake project. No third-party build/runtime dependencies.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sofa-buffers/corelib-c-cpp
    REF "v${VERSION}"
    # Replace with the real tarball hash for tag v${VERSION}. Set to 0 and run
    # `vcpkg install sofa-buffers-corelib --overlay-ports=...`; vcpkg prints the
    # expected SHA512 to paste here (overlay: --overlay-ports=ports).
    SHA512 0
    HEAD_REF main
)

# Map the port features onto the upstream CMake feature-toggle options. Each
# option applies a PUBLIC compile definition, so it is carried in the installed
# CMake config and inherited by consumers of sofa-buffers::corelib.
vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        no-object-api     SOFAB_DISABLE_OBJECT_API
        no-array          SOFAB_DISABLE_ARRAY_SUPPORT
        no-sequence       SOFAB_DISABLE_SEQUENCE_SUPPORT
        no-fixlen         SOFAB_DISABLE_FIXLEN_SUPPORT
        no-fp64           SOFAB_DISABLE_FP64_SUPPORT
        no-int64          SOFAB_DISABLE_INT64_SUPPORT
        no-overflow-check SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DSOFAB_INSTALL=ON
        -DSOFAB_BUILD_TESTS=OFF
        -DSOFAB_ENABLE_BENCH=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME sofa-buffers-corelib-c-cpp
    CONFIG_PATH lib/cmake/sofa-buffers-corelib-c-cpp
)

# A static library needs no debug headers.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# The build drops a linker mapfile next to the artifacts; keep it out of the
# package.
file(REMOVE
    "${CURRENT_PACKAGES_DIR}/mapfile.map"
    "${CURRENT_PACKAGES_DIR}/debug/mapfile.map"
)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
