#include <stdint.h>
#include <avr/io.h>

extern int main(void);
extern uint32_t  __data_rom_start;	/* Start of .data in ROM */
extern uint32_t  __data_start;		/* Start of .data in RAM */
extern uint32_t  __data_end;		/* End of .data in RAM */
extern uint32_t  __bss_start;		/* Start of .bss */
extern uint32_t  __bss_end;			/* End of .bss, start of heap */
extern uint32_t  __stack_top;		/* Top of stack, end of RAM - 4 */

/* Reset-Handler */
void Reset_Handler(void) __attribute__((naked, section(".init0")));
void Reset_Handler(void)
{
    /* Stackpointer */
    __asm__ volatile (
        "ldi r16, hi8(%0)\n\t"
        "out %1, r16\n\t"
        "ldi r16, lo8(%0)\n\t"
        "out %2, r16\n\t"
        :
        : "i" (&__stack_top),
          "I" (_SFR_IO_ADDR(SPH)),
          "I" (_SFR_IO_ADDR(SPL))
        : "r16"
    );

    // Zero initialize .bss section
    uint32_t *bss = &__bss_start;
    while (bss < &__bss_end)
    {
        *bss++ = 0;
    }

    // Initialize .data section
    uint32_t *data_rom = &__data_rom_start;
    uint32_t *data_ram = &__data_start;
    while (data_ram < &__data_end)
    {
        *data_ram++ = *data_rom++;
    }

    main();

    for (;;);
}
