# RISC-V RV32 bare-metal toolchain file (riscv64-unknown-elf, ILP32).
#
# Ubuntu's gcc-riscv64-unknown-elf ships no C library of its own, so the headers
# come from picolibc via --specs=picolibc.specs. Nothing here is ever linked for
# the target — only the static library is built — so that is enough, and
# CMAKE_TRY_COMPILE_TARGET_TYPE keeps CMake's compiler probe from trying to link
# one.
#
# Used by .github/workflows/build-gcc-riscv32.yaml and by tools/footprint.sh,
# which reports the RV32IMC row of the README footprint tables.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

set(CMAKE_C_COMPILER   riscv64-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_SIZE         riscv64-unknown-elf-size)

# The README row is RV32IMC; RISCV_MARCH/RISCV_MABI keep the other RV32 profiles
# reachable without a second toolchain file (e.g. -DRISCV_MARCH=rv32i for a
# no-multiply, no-compressed core).
if(NOT DEFINED RISCV_MARCH)
    set(RISCV_MARCH rv32imc)
endif()
if(NOT DEFINED RISCV_MABI)
    set(RISCV_MABI ilp32)
endif()

set(COMMON_FLAGS "-march=${RISCV_MARCH} -mabi=${RISCV_MABI} --specs=picolibc.specs")

set(CMAKE_C_FLAGS   "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS}")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
