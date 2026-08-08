/* Linker Script for stm32vldiscovery */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

MEMORY
{
    /* The unit-test image is ~124K of Unity plus test data; the library
     * itself is under 4K. This target is a link-only smoke test -- the
     * workflow inspects the ELF with file(1) and readelf and never runs it --
     * so the device model is a fiction, and sizing it to a real
     * STM32VLDISCOVERY only caps how far the test suite may grow. Library
     * footprint is tracked by the `size libsofabuffers.a` step and the README
     * tables, not by this region. */
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    SRAM  (rw!x): ORIGIN = 0x20000000, LENGTH = 8K
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
    } > SRAM AT > FLASH

    /* static cleared read-write data */
    .bss (NOLOAD) : ALIGN(4)
    {
        PROVIDE(__bss_start = .);

        *(.bss .bss.*)
        *(COMMON)

        . = ALIGN(4);
        PROVIDE(__bss_end = .);
    } > SRAM

    __stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
}
