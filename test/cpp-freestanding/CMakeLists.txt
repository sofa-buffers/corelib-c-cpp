# ----------------------------------------
# C++ wrapper freestanding compile guard
# ----------------------------------------
#
# sofab.hpp splits into a core that compiles under -ffreestanding and a hosted
# convenience layer (std::string / std::vector overloads, the std::shared_ptr
# OStream, the std::function callback typedefs) gated behind
# SOFAB_CPP_HAVE_HOSTED. Nothing enforced that split before this target: every
# bare-metal CI job passes -DSOFAB_ENABLE_CPP=OFF, so no MCU build ever compiled
# a C++ translation unit, and the wrapper had drifted into being unbuildable
# with the project's own Cortex-M toolchain file.
#
# This target is that enforcement. It is an OBJECT library, not an executable:
# the point is to *compile*, and the bare-metal targets cannot run what they
# link anyway. -ffreestanding is set on the target rather than inherited from a
# toolchain file, so the guard also runs in the ordinary hosted CI jobs — which
# is what makes it cheap enough to be on everywhere. (The cross toolchains are
# not a substitute: only gcc-arm-none-eabi ships libstdc++ headers at all, so
# the AVR and RISC-V images cannot compile any C++ wrapper TU regardless.)
#
# Exceptions stay off: freestanding.cpp uses only the no-heap surface, which is
# noexcept throughout, and -fexceptions would misrepresent the MCU build.

# Not every cross toolchain ships a C++ standard library: gcc-avr and
# gcc-riscv64-unknown-elf have a C++ *compiler* but no libstdc++ headers, so
# even <array> is missing and no wrapper TU can be compiled there at all. The
# project enables CXX unconditionally (LANGUAGES C CXX), so without this probe a
# default-ON guard would break exactly those bare-metal builds. Probe with the
# same -ffreestanding the target uses, so a partial library is caught too.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-ffreestanding")
check_cxx_source_compiles("
    #include <array>
    #include <span>
    #include <string_view>
    int main() { return 0; }
" SOFAB_CXX_FREESTANDING_LIB_AVAILABLE)
unset(CMAKE_REQUIRED_FLAGS)

if(NOT SOFAB_CXX_FREESTANDING_LIB_AVAILABLE)
    message(STATUS
        "sofab: skipping the C++ freestanding guard -- this toolchain has no "
        "freestanding C++ standard library (the wrapper cannot be built here at all)")
    return()
endif()

add_library(sofab_cpp_freestanding OBJECT
    freestanding.cpp
)

target_compile_options(sofab_cpp_freestanding
    PRIVATE -ffreestanding -fno-exceptions -fno-rtti
            -Wall -Wextra -Wpedantic -Werror -Wno-error=cpp
)

target_link_libraries(sofab_cpp_freestanding
    PRIVATE sofabuffers
)
