# Renesas RL78 Toolchain file

# Set the target system
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR rl78)

# Specify cross compiler
set(CMAKE_C_COMPILER   ${LLVM_PATH}/clang)
set(CMAKE_CXX_COMPILER ${LLVM_PATH}/clang++)
set(CMAKE_ASM_COMPILER ${LLVM_PATH}/clang)
set(CMAKE_OBJCOPY      ${LLVM_PATH}/llvm-objcopy)
set(CMAKE_SIZE         ${LLVM_PATH}/llvm-size)

if(NOT DEFINED RL78_MCU)
    set(RL78_MCU rl78g13)
endif()

# Common flags
set(COMMON_FLAGS "-m64bit-doubles -ffreestanding")
# set(COMMON_FLAGS "-ffreestanding")

# C / ASM flags
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS}")
set(CMAKE_C_FLAGS   "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -fno-exceptions -fno-rtti")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "-T${CMAKE_SOURCE_DIR}/utils/rl78/linker-rl78.ld -nostartfiles -static")
