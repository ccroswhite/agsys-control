/*
 * FreeRTOS Kernel V10.0.0
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Modified for nRF52810 (Cortex-M4 without FPU)
 */

#include "FreeRTOS.h"
#include "task.h"
#ifdef SOFTDEVICE_PRESENT
#include "nrf_soc.h"
#include "app_util.h"
#include "app_util_platform.h"
#endif

#if configMAX_SYSCALL_INTERRUPT_PRIORITY == 0
    #error configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to 0.
#endif

#define portCORTEX_M4_r0p1_ID               ( 0x410FC241UL )
#define portFIRST_USER_INTERRUPT_NUMBER     ( 16 )
#define portMAX_8_BIT_VALUE                 ( ( uint8_t ) 0xff )
#define portTOP_BIT_OF_BYTE                 ( ( uint8_t ) 0x80 )
#define portINITIAL_XPSR                    (((xPSR_Type){.b.T = 1}).w)
#define portINITIAL_EXEC_RETURN             ( 0xfffffffd )

#ifdef configTASK_RETURN_ADDRESS
    #define portTASK_RETURN_ADDRESS configTASK_RETURN_ADDRESS
#else
    #define portTASK_RETURN_ADDRESS prvTaskExitError
#endif

static UBaseType_t uxCriticalNesting = 0;

extern void vPortSetupTimerInterrupt( void );
void xPortSysTickHandler( void );
extern void vPortStartFirstTask( void );
static void prvTaskExitError( void );

#if ( configASSERT_DEFINED == 1 )
     static uint8_t ucMaxSysCallPriority = 0;
     static uint32_t ulMaxPRIGROUPValue = 0;
#endif

/*-----------------------------------------------------------*/

StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
    pxTopOfStack--;
    *pxTopOfStack = portINITIAL_XPSR;
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pxCode;
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portTASK_RETURN_ADDRESS;
    pxTopOfStack -= 5;
    *pxTopOfStack = ( StackType_t ) pvParameters;
    pxTopOfStack--;
    *pxTopOfStack = portINITIAL_EXEC_RETURN;
    pxTopOfStack -= 8;
    return pxTopOfStack;
}

/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
    configASSERT( uxCriticalNesting == ~0UL );
    portDISABLE_INTERRUPTS();
    for ( ;; );
}

/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
    configASSERT( configMAX_SYSCALL_INTERRUPT_PRIORITY );

    /* Skip CPUID check for nRF52810 - it's a valid Cortex-M4 */

    #if ( configASSERT_DEFINED == 1 )
    {
        volatile uint32_t ulOriginalPriority;
        volatile uint8_t * const pucFirstUserPriorityRegister = &NVIC->IP[0];
        volatile uint8_t ucMaxPriorityValue;

        ulOriginalPriority = *pucFirstUserPriorityRegister;
        *pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;
        ucMaxPriorityValue = *pucFirstUserPriorityRegister;
        ucMaxSysCallPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY & ucMaxPriorityValue;

        ulMaxPRIGROUPValue = SCB_AIRCR_PRIGROUP_Msk >> SCB_AIRCR_PRIGROUP_Pos;
        while ( ( ucMaxPriorityValue & portTOP_BIT_OF_BYTE ) == portTOP_BIT_OF_BYTE )
        {
            ulMaxPRIGROUPValue--;
            ucMaxPriorityValue <<= ( uint8_t ) 0x01;
        }
        ulMaxPRIGROUPValue &= SCB_AIRCR_PRIGROUP_Msk >> SCB_AIRCR_PRIGROUP_Pos;
        *pucFirstUserPriorityRegister = ulOriginalPriority;
    }
    #endif

    NVIC_SetPriority(PendSV_IRQn, configKERNEL_INTERRUPT_PRIORITY);
    vPortSetupTimerInterrupt();
    uxCriticalNesting = 0;

    /* No FPU on nRF52810 - skip VFP enable */

    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;
    vPortStartFirstTask();
    prvTaskExitError();

    return 0;
}

/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
    configASSERT( uxCriticalNesting == 1000UL );
}

/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;

    if ( uxCriticalNesting == 1 )
    {
        configASSERT( ( SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk ) == 0 );
    }
}

/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting );
    uxCriticalNesting--;
    if ( uxCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}

/*-----------------------------------------------------------*/

#if ( configASSERT_DEFINED == 1 )

    void vPortValidateInterruptPriority( void )
    {
    uint32_t ulCurrentInterrupt;
    uint8_t ucCurrentPriority;
    IPSR_Type ipsr;

        ipsr.w = __get_IPSR();
        ulCurrentInterrupt = ipsr.b.ISR;

        if ( ulCurrentInterrupt >= portFIRST_USER_INTERRUPT_NUMBER )
        {
            ucCurrentPriority = NVIC->IP[ ulCurrentInterrupt - portFIRST_USER_INTERRUPT_NUMBER ];
            configASSERT( ucCurrentPriority >= ucMaxSysCallPriority );
        }

        configASSERT( NVIC_GetPriorityGrouping() <= ulMaxPRIGROUPValue );
    }

#endif
