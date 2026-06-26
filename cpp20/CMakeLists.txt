# Standalone build for the pure-C++20 SofaBuffers implementation.
#
# This is intentionally separate from the root project (which builds the C
# library and its C++ wrapper) — the two C++ headers share the `sofab::` API and
# cannot be compiled into the same translation unit. Configure this directory on
# its own: cmake -S cpp20 -B cpp20/build && cmake --build cpp20/build && ctest --test-dir cpp20/build

cmake_minimum_required(VERSION 3.10)
project(sofabuffers_cpp20 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sofab_cpp20 INTERFACE)
target_include_directories(sofab_cpp20 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

enable_testing()

add_executable(test_roundtrip test/test_roundtrip.cpp)
target_compile_options(test_roundtrip PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(test_roundtrip PRIVATE sofab_cpp20)

add_test(NAME test_roundtrip COMMAND test_roundtrip)
