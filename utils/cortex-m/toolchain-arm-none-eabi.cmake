# ARM Cortex-M None EABI Toolchain file

# Set the target system
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_SYSTEM_NAME Generic)

# Specify cross compiler
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_SIZE         arm-none-eabi-size)

if(NOT DEFINED ARM_MARCH)
    set(ARM_MARCH armv6-m)
endif()

if(NOT DEFINED ARM_MTUNE)
    set(ARM_MTUNE cortex-m0)
endif()

# Common flags
set(COMMON_FLAGS "-march=${ARM_MARCH} -mtune=${ARM_MTUNE} -mthumb -mno-unaligned-access -ffreestanding")

# C / ASM flags
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS}")
set(CMAKE_C_FLAGS   "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS}")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "-T${CMAKE_SOURCE_DIR}/utils/cortex-m/linker-cortex-m.ld -nostartfiles -static")
