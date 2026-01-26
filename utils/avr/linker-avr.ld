/* Linker Script for Atmel/Microchip AVR */

OUTPUT_FORMAT("elf32-avr")
OUTPUT_ARCH(avr)

/* AVR uses a Harvard architecture:
 * - Flash (program memory) starts at address 0
 * - SRAM (data memory) has its own address space
 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0, LENGTH = 128K   /* Placeholder, overridden by -mmcu */
    RAM   (rw!x): ORIGIN = 0, LENGTH = 8K     /* Placeholder, overridden by -mmcu */
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* startup code */
    .init0 :
    {
        KEEP(*(.init0))
    } > FLASH

    /* list of init functions */
    .constructors :
    {
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } > FLASH

    /* list of fini functions */
    .destructors :
    {
        __fini_array_start = .;
        KEEP(*(.fini_array))
        KEEP(*(SORT(.fini_array.*)))
        __fini_array_end = .;
    } > FLASH

    /* default program text */
    .text :
    {
        *(.text .text.*)
        *(.progmem*)   /* For AVR PROGMEM variables */

        /* compiler stubs */
        *(.gcc*)
    } > FLASH

    /* default read-only data */
    .rodata :
    {
        *(.rodata .rodata.*)
    } > FLASH

    /* initialized read-write data */
    .data :
    {
        PROVIDE(__data_rom_start = LOADADDR(.data));
        PROVIDE(__data_start = .);

        *(.data .data.*)

        PROVIDE(__data_end = .);
    } > RAM AT > FLASH

    /* static cleared read-write data */
    .bss (NOLOAD) :
    {
        PROVIDE(__bss_start = .);

        *(.bss .bss.*)
        *(COMMON)

        PROVIDE(__bss_end = .);
    } > RAM

    __stack_top = ORIGIN(RAM) + LENGTH(RAM) - 1;
}
