/* Generic AVR Linker Script for bare-metal C / C++ projects */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x40010000, LENGTH = 512K
    RAM   (rwx): ORIGIN = 0x40110000 , LENGTH = 64K
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* interrupt vector table */
    .startup : ALIGN(4)
    {
        KEEP(*(.isr_vector))
    } > FLASH

    /* list of init functions */
    .constructors : ALIGN(4) SUBALIGN(4)
    {
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } > FLASH

    /* list of fini functions */
    .destructors : ALIGN(4) SUBALIGN(4)
    {
        __fini_array_start = .;
        KEEP(*(.fini_array))
        KEEP(*(SORT(.fini_array.*)))
        __fini_array_end = .;
    } > FLASH

    /* default program text */
    .text : ALIGN(4)
    {
        *(.text .text.*)

        /* linker armv7 stubs (ARM, Thumb)*/
        *(.glue_7t)
        *(.glue_7)

        /* compiler stubs */
        *(.gcc*)
    } > FLASH

    /* default read-only data */
    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.*)
    } > FLASH

    /* ARM special sections */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

     /* exceptions frames*/
    .eh_frame_hdr :
    {
        *(.eh_frame_hdr)
    } > FLASH

    .eh_frame : ONLY_IF_RO
    {
        *(.eh_frame)
    } > FLASH

    /* initialized read-write data */
    .data : ALIGN(4)
    {
        PROVIDE(__data_rom_start = LOADADDR(.data));
        PROVIDE(__data_start = .);

        *(.data .data.*)

        . = ALIGN(4);
        PROVIDE(__data_end = .);
    } > RAM AT > FLASH

    /* static cleared read-write data */
    .bss (NOLOAD) : ALIGN(4)
    {
        PROVIDE(__bss_start = .);

        *(.bss .bss.*)
        *(COMMON)

        . = ALIGN(4);
        PROVIDE(__bss_end = .);
    } > RAM

    __stack_top = ORIGIN(RAM) + LENGTH(RAM) - 4;
}
