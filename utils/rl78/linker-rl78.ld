/* Linker Script for Renesas RL78 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000, LENGTH = 0x40000  /* 256 KB Flash */
    RAM   (rw!x): ORIGIN = 0xFEF00, LENGTH = 0x8000   /* 32 KB RAM */
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* interrupt vector table */
    .startup :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    /* default program text */
    .text : ALIGN(2)
    {
        *(.text .text.*)

        /* compiler stubs */
        *(.gcc*)
    } > FLASH

    /* default read-only data */
    .rodata : ALIGN(2)
    {
        *(.rodata .rodata.*)
    } > FLASH

    /* initialized read-write data */
    .data : ALIGN(2)
    {
        PROVIDE(__data_rom_start = LOADADDR(.data));
        PROVIDE(__data_start = .);

        *(.data*)

        PROVIDE(__data_end = .);
    } > RAM AT > FLASH

    /* static cleared read-write data */
    .bss (NOLOAD) : ALIGN(2)
    {
        PROVIDE(__bss_start = .);

        *(.bss*)
        *(COMMON)

        PROVIDE(__bss_end = .);
        PROVIDE(__end = .);
    } > RAM

    /* Stack */
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
