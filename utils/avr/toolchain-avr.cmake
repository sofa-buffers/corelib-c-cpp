# AVR Toolchain file

# Set the target system
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)

# Specify cross compiler
set(CMAKE_C_COMPILER   avr-gcc)
set(CMAKE_CXX_COMPILER avr-g++)
set(CMAKE_ASM_COMPILER avr-gcc)

if(NOT DEFINED AVR_MCU)
    set(AVR_MCU atmega328p)
endif()

# Common flags
set(COMMON_FLAGS "-mmcu=${AVR_MCU}")

# C / ASM flags
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS}")
set(CMAKE_C_FLAGS   "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS} -fno-exceptions -fno-rtti")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "-mmcu=${AVR_MCU} -T${CMAKE_SOURCE_DIR}/utils/avr/linker-avr.ld -static")
