#include <stdint.h>

void Reset_Handler(void);
void Default_Handler(void);

extern int main(void);
extern uint32_t  __data_rom_start;	/* Start of .data in ROM */
extern uint32_t  __data_start;		/* Start of .data in RAM */
extern uint32_t  __data_end;		/* End of .data in RAM */
extern uint32_t  __bss_start;		/* Start of .bss */
extern uint32_t  __bss_end;			/* End of .bss, start of heap */
extern uint32_t  __stack_top;		/* Top of stack, end of RAM - 4 */

typedef void (*isr_t)(void);

typedef struct
{
	uint32_t *stack_top;
	isr_t reset;
	isr_t nmi;
	isr_t hardfault;
} vector_table_t;

__attribute__((section(".isr_vector")))
const vector_table_t vector_table =
{
	(uint32_t *)&__stack_top,
	Reset_Handler,
	Default_Handler,
	Default_Handler
};

void Reset_Handler(void)
{
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

	// Call the application's entry point
	main();
	// while (1);
}

void Default_Handler(void)
{
	while (1);
}
