from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, rmdir
from conan.tools.build import check_min_cppstd
import os

required_conan_version = ">=2.0"


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
        "object_api": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "object_api": True,
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
        tc.variables["SOFAB_DISABLE_OBJECT_API"] = not self.options.object_api
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
        # Consumers get the same names as the upstream CMake / vcpkg port:
        #   find_package(sofa-buffers-corelib-c-cpp) -> sofa-buffers::corelib
        self.cpp_info.set_property("cmake_file_name", "sofa-buffers-corelib-c-cpp")
        self.cpp_info.set_property("cmake_target_name", "sofa-buffers::corelib")
