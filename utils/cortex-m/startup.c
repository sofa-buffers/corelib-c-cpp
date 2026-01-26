#include <stdint.h>

/* Symbols defined in the linker script */
extern uint32_t  __data_rom_start;	/* Start of .data in ROM */
extern uint32_t  __data_start;		/* Start of .data in RAM */
extern uint32_t  __data_end;		/* End of .data in RAM */
extern uint32_t  __bss_start;		/* Start of .bss */
extern uint32_t  __bss_end;			/* End of .bss, start of heap */
extern uint32_t  __stack_top;		/* Top of stack, end of RAM */

/* Function Prototypes */
void Reset_Handler (void);
void Default_Handler (void);
void NMI_Handler (void);
void HardFault_Handler (void);
void MemManage_Handler (void);
void BusFault_Handler (void);
void UsageFault_Handler (void);

extern int main (void);

__attribute__((noreturn))
extern void _exit(int status);
extern int _write(int file, char *ptr, int len);

typedef void (*isr_t)(void);
typedef struct
{
    uint32_t *stack_top;
    isr_t reset;
    isr_t nmi;
    isr_t hardfault;
    isr_t memmanage;
    isr_t busfault;
    isr_t usagefault;
} vector_table_t;

__attribute__((section(".isr_vector")))
const vector_table_t vector_table =
{
    (uint32_t *)&__stack_top,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler
};

/**
 * Entry point after hardware reset
 */
__attribute__((used, noreturn))
void Reset_Handler (void)
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
    _exit(main());
}

void Default_Handler (void)
{
    _write(1, "Default_Handler called\n", 23);
    _exit(1);
}

void NMI_Handler (void)
{
    _write(1, "NMI_Handler called\n", 19);
    _exit(1);
}

void HardFault_Handler (void)
{
    _write(1, "HardFault_Handler called\n", 25);
    _exit(1);
}

void MemManage_Handler (void)
{
    _write(1, "MemManage_Handler called\n", 25);
    _exit(1);
}

void BusFault_Handler (void)
{
    _write(1, "BusFault_Handler called\n", 24);
    _exit(1);
}

void UsageFault_Handler (void)
{
    _write(1, "UsageFault_Handler called\n", 25);
    _exit(1);
}
