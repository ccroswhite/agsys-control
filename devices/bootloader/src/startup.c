/**
 * @file startup.c
 * @brief Bootloader startup code for nRF52832
 *
 * Minimal startup - no SDK dependencies.
 * Sets up stack, copies .data, zeros .bss, calls main().
 */

#include <stdint.h>

/*******************************************************************************
 * External Symbols (from linker script)
 ******************************************************************************/

extern uint32_t _sidata;    /* Start of .data in flash */
extern uint32_t _sdata;     /* Start of .data in RAM */
extern uint32_t _edata;     /* End of .data in RAM */
extern uint32_t _sbss;      /* Start of .bss */
extern uint32_t _ebss;      /* End of .bss */
extern uint32_t _stack_end; /* Top of stack */

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/

extern int main(void);
void Reset_Handler(void);
void Default_Handler(void);

/* Weak aliases for interrupt handlers */
void NMI_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)          __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)      __attribute__((weak, alias("Default_Handler")));

/*******************************************************************************
 * Vector Table
 ******************************************************************************/

__attribute__((section(".isr_vector"), used))
const void *vector_table[] = {
    &_stack_end,            /* Initial stack pointer */
    Reset_Handler,          /* Reset handler */
    NMI_Handler,            /* NMI handler */
    HardFault_Handler,      /* Hard fault handler */
    MemManage_Handler,      /* MPU fault handler */
    BusFault_Handler,       /* Bus fault handler */
    UsageFault_Handler,     /* Usage fault handler */
    0,                      /* Reserved */
    0,                      /* Reserved */
    0,                      /* Reserved */
    0,                      /* Reserved */
    SVC_Handler,            /* SVCall handler */
    DebugMon_Handler,       /* Debug monitor handler */
    0,                      /* Reserved */
    PendSV_Handler,         /* PendSV handler */
    SysTick_Handler,        /* SysTick handler */
    /* nRF52 peripheral interrupts - not used by bootloader */
};

/*******************************************************************************
 * Reset Handler
 ******************************************************************************/

void Reset_Handler(void)
{
    uint32_t *src, *dst;
    
    /* Copy .data from flash to RAM */
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }
    
    /* Zero .bss */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }
    
    /* Call main */
    main();
    
    /* Should never return */
    while (1);
}

/*******************************************************************************
 * Default Handler
 ******************************************************************************/

void Default_Handler(void)
{
    /* Hang on unexpected interrupt */
    while (1);
}
