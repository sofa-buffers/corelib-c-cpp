#include <stdint.h>

void Reset_Handler(void);
void Default_Handler(void);
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);

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

void exit_qemu(int code)
{
    // Semihosting SYS_EXIT
    register int r0 __asm__("r0") = code;
    register int r1 __asm__("r1") = 0x18;  // SYS_EXIT
    __asm__ volatile (
        "bkpt 0xAB"
        :
        : "r"(r0), "r"(r1)
        : "memory"
    );
}

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
    exit_qemu(main());
}

void Default_Handler(void)
{
    while (1);
}

void NMI_Handler(void)
{
    while(1);
}

void HardFault_Handler(void)
{
    while(1);
}

void MemManage_Handler(void)
{
    while (1);
}

void BusFault_Handler(void)
{
    while (1);
}

void UsageFault_Handler(void)
{
    while (1);
}
