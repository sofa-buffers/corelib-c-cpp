/* Linker Script for Renesas RL78 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x0000, LENGTH = 0x40000   /* 256 KB Flash */
    RAM   (rw!x): ORIGIN = 0xFF00, LENGTH = 0x8000    /* 32 KB RAM */
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* Reset / Code */
    .text :
    {
        KEEP(*(.reset))      /* Reset Handler */
        *(.text*)            /* alle Text-Segmente */
        *(.rodata*)          /* read-only data / Konstanten */
    } > FLASH

    /* Initialized data */
    .data : AT (ADDR(.text) + SIZEOF(.text))
    {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM

    /* Uninitialized data (BSS) */
    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > RAM

    /* Stack */
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
