from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, rmdir
from conan.tools.build import check_min_cppstd
import os

required_conan_version = ">=2.0"

# Optional feature toggles: Conan option name -> upstream SOFAB_DISABLE_* macro.
# Each option is positive-sense (True = feature present); a False value disables
# the feature by defining the corresponding macro. Object-API selects a source
# file; the rest are compile-time guards that also appear in the public headers,
# so they are re-exported as cpp_info.defines below.
_SOFAB_FEATURES = {
    "object_api": "SOFAB_DISABLE_OBJECT_API",
    "array": "SOFAB_DISABLE_ARRAY_SUPPORT",
    "sequence": "SOFAB_DISABLE_SEQUENCE_SUPPORT",
    "fixlen": "SOFAB_DISABLE_FIXLEN_SUPPORT",
    "fp64": "SOFAB_DISABLE_FP64_SUPPORT",
    "int64": "SOFAB_DISABLE_INT64_SUPPORT",
    "overflow_check": "SOFAB_DISABLE_INTEGER_OVERFLOW_CHECK",
}

# Macros consumed only by the .c sources (not the public headers); no need to
# propagate these to downstream compilations.
_SOFAB_SOURCE_ONLY_MACROS = {"SOFAB_DISABLE_OBJECT_API"}


class SofaBuffersCorelibConan(ConanFile):
    name = "sofa-buffers-corelib-c-cpp"
    version = "0.8.0"
    license = "MIT"
    author = "andste82"
    url = "https://github.com/sofa-buffers/corelib-c-cpp"
    homepage = "https://github.com/sofa-buffers/corelib-c-cpp"
    description = (
        "Dependency-free, heap-free, streaming C99/C++20 implementation of the "
        "SofaBuffers (Sofab) serialization format. It packs structured fields "
        "into a caller-owned buffer and decodes them through a callback-driven "
        "decoder, with no allocator and no third-party dependencies."
    )
    topics = ("serialization", "embedded", "streaming", "no-alloc", "c99", "cpp20")

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        # Feature toggles (all default to on / full wire format).
        "object_api": [True, False],
        "array": [True, False],
        "sequence": [True, False],
        "fixlen": [True, False],
        "fp64": [True, False],
        "int64": [True, False],
        "overflow_check": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "object_api": True,
        "array": True,
        "sequence": True,
        "fixlen": True,
        "fp64": True,
        "int64": True,
        "overflow_check": True,
    }

    # Only the pieces needed to build the library are exported; the test and
    # benchmark trees (which pull in Unity/Catch2 via FetchContent) are omitted
    # because SOFAB_BUILD_TESTS/SOFAB_ENABLE_BENCH are OFF below.
    exports_sources = (
        "CMakeLists.txt",
        "src/*",
        "cmake/*",
        "LICENSE",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def validate(self):
        check_min_cppstd(self, 20)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SOFAB_INSTALL"] = True
        tc.variables["SOFAB_BUILD_TESTS"] = False
        tc.variables["SOFAB_ENABLE_BENCH"] = False
        # A disabled feature (option False) sets its SOFAB_DISABLE_* CMake option.
        for opt, macro in _SOFAB_FEATURES.items():
            tc.variables[macro] = not getattr(self.options, opt)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        # Conan generates its own CMake config files via CMakeDeps; drop the
        # ones installed by the project to avoid a clash.
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.libs = ["sofabuffers"]
        # Consumers get the same names as the upstream CMake project:
        #   find_package(sofa-buffers-corelib-c-cpp) -> sofa-buffers::corelib
        self.cpp_info.set_property("cmake_file_name", "sofa-buffers-corelib-c-cpp")
        self.cpp_info.set_property("cmake_target_name", "sofa-buffers::corelib")
        # Re-export the header-guard macros for every disabled feature so a
        # consumer's headers match the compiled library. (Conan builds its own
        # CMake config from cpp_info, so the library's exported PUBLIC defines
        # are not otherwise visible downstream.)
        for opt, macro in _SOFAB_FEATURES.items():
            if macro in _SOFAB_SOURCE_ONLY_MACROS:
                continue
            if not getattr(self.options, opt):
                self.cpp_info.defines.append(macro)
