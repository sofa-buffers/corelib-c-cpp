#include <stdint.h>

/* Symbols defined in the linker script */
extern char __bss_start			__asm__("__bss_start");     /* Start of .data in ROM */
extern char __bss_end   	    __asm__("__bss_end");       /* Start of .data in RAM */
extern char __data_rom_start    __asm__("__data_rom_start");/* End of .data in RAM */
extern char __data_start   	    __asm__("__data_start");    /* Start of .bss */
extern char __data_end          __asm__("__data_end");      /* End of .bss, start of heap */
extern char __stack_top         __asm__("__stack_top");     /* Top of stack, end of RAM */

/* Function Prototypes */
/* Force the symbol name to match the ENTRY(Reset_Handler) in your .ld file */
void Reset_Handler(void)    __asm__("Reset_Handler") __attribute__((naked, used, noreturn, section(".text.init")));
void start_c(void)          __asm__("start_c");

extern int main(void);

/**
 * RL78 Fixed Vector Table (0x0000 - 0x007F)
 * Each entry is a 16-bit address.
 */
__attribute__((section(".isr_vector")))
const void *const vector_table[] =
{
    (const void *)Reset_Handler,  /* 0x00: Reset Vector */
    /* Add peripheral interrupt vectors here according to the MCU manual */
};

/**
 * 1. The actual entry point.
 * Naked, so we can safely set the Stack Pointer first.
 */
void Reset_Handler(void)
{
    /* Initialize SP immediately.
       Note: Using the symbol name directly as defined in linker script. */
    __asm volatile ("movw sp, #__stack_top");

    /* Jump to the C-based initialization part */
    __asm volatile ("br !!start_c");
}

/**
 * 2. C-runtime initialization.
 * Now that SP is set, we can use normal C code.
 */
void start_c(void)
{
    /* Global Interrupt Disable */
    __asm volatile ("di");

    /* Initialize .bss section (Zero fill) */
    uint8_t *bss_ptr = (uint8_t *)&__bss_start;
    while (bss_ptr < (uint8_t *)&__bss_end)
    {
        *bss_ptr++ = 0;
    }

    /* Initialize .data section (Copy from Flash to RAM) */
    uint8_t *src = (uint8_t *)&__data_rom_start;
    uint8_t *dst = (uint8_t *)&__data_start;
    while (dst < (uint8_t *)&__data_end)
    {
        *dst++ = *src++;
    }

    /* Jump to Application Entry Point */
    main();

    /* Safety loop in case main returns */
    while (1)
    {
        __asm volatile ("halt");
    }
}
