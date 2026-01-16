/*
 * FreeRTOS Kernel V10.0.0
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Modified for nRF52810 (Cortex-M4 without FPU)
 */

#include "FreeRTOS.h"
#include "task.h"

void vPortStartFirstTask( void ) __attribute__ (( naked ));
void vPortSVCHandler( void ) __attribute__ (( naked ));
void xPortPendSVHandler( void ) __attribute__ (( naked ));

/*-----------------------------------------------------------*/

void vPortStartFirstTask( void )
{
    __asm volatile(
                    " ldr r0, =__isr_vector \n"
                    " ldr r0, [r0]          \n"
                    " msr msp, r0           \n"
                    " cpsie i               \n"
                    " cpsie f               \n"
                    " dsb                   \n"
                    " isb                   \n"
#ifdef SOFTDEVICE_PRESENT
                    " mov r0, %0            \n"
                    " msr basepri, r0       \n"
#endif
                    " svc 0                 \n"
                    "                       \n"
                    " .align 2              \n"
#ifdef SOFTDEVICE_PRESENT
                    ::"i"(configKERNEL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#endif
                );
}

/*-----------------------------------------------------------*/

void vPortSVCHandler( void )
{
    __asm volatile (
                    "   ldr r3, =pxCurrentTCB           \n"
                    "   ldr r1, [r3]                    \n"
                    "   ldr r0, [r1]                    \n"
                    "   ldmia r0!, {r4-r11, r14}        \n"
                    "   msr psp, r0                     \n"
                    "   isb                             \n"
                    "   mov r0, #0                      \n"
                    "   msr basepri, r0                 \n"
                    "   bx r14                          \n"
                    "                                   \n"
                    "   .align 2                        \n"
                );
}

/*-----------------------------------------------------------*/

void xPortPendSVHandler( void )
{
    /* This is a naked function - no FPU version for nRF52810 */

    __asm volatile
    (
    "   mrs r0, psp                         \n"
    "   isb                                 \n"
    "                                       \n"
    "   ldr r3, =pxCurrentTCB               \n"
    "   ldr r2, [r3]                        \n"
    "                                       \n"
    /* No FPU context save - nRF52810 has no FPU */
    "   stmdb r0!, {r4-r11, r14}            \n"
    "                                       \n"
    "   str r0, [r2]                        \n"
    "                                       \n"
    "   stmdb sp!, {r3}                     \n"
    "   mov r0, %0                          \n"
    "   msr basepri, r0                     \n"
    "   dsb                                 \n"
    "   isb                                 \n"
    "   bl vTaskSwitchContext               \n"
    "   mov r0, #0                          \n"
    "   msr basepri, r0                     \n"
    "   ldmia sp!, {r3}                     \n"
    "                                       \n"
    "   ldr r1, [r3]                        \n"
    "   ldr r0, [r1]                        \n"
    "                                       \n"
    "   ldmia r0!, {r4-r11, r14}            \n"
    /* No FPU context restore */
    "                                       \n"
    "   msr psp, r0                         \n"
    "   isb                                 \n"
    "                                       \n"
    "   bx r14                              \n"
    "                                       \n"
    "   .align 2                            \n"
    ::"i"(configMAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
    );
}
