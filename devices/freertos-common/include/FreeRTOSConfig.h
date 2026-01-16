/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS configuration for AgSys devices (nRF52832/nRF52840)
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ==========================================================================
 * BASIC CONFIGURATION
 * ========================================================================== */

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configENABLE_FPU                        1   /* nRF52832 has FPU */
#define configCPU_CLOCK_HZ                      64000000
#define configTICK_RATE_HZ                      1024
#define configMAX_PRIORITIES                    6
#define configMINIMAL_STACK_SIZE                64
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  0
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

/* ==========================================================================
 * MEMORY ALLOCATION
 * ========================================================================== */

#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   8192  /* nRF52832 has 64KB RAM */
#define configAPPLICATION_ALLOCATED_HEAP        0

/* ==========================================================================
 * HOOK FUNCTIONS
 * ========================================================================== */

#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* ==========================================================================
 * RUNTIME STATS
 * ========================================================================== */

#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ==========================================================================
 * CO-ROUTINES (not used)
 * ========================================================================== */

#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

/* ==========================================================================
 * SOFTWARE TIMERS
 * ========================================================================== */

#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               2
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* ==========================================================================
 * INTERRUPT PRIORITIES (Nordic specific)
 * ========================================================================== */

/* Interrupt priorities used by the kernel port layer itself. */
#define configPRIO_BITS                         3   /* nRF52 has 3 priority bits */
#define configKERNEL_INTERRUPT_PRIORITY         7   /* Lowest priority */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    1   /* Highest priority for FreeRTOS API calls */

/* Priority 0 is reserved for SoftDevice */
#define configMAX_API_CALL_INTERRUPT_PRIORITY   configMAX_SYSCALL_INTERRUPT_PRIORITY

/* ==========================================================================
 * OPTIONAL FUNCTIONS
 * ========================================================================== */

#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              1

/* ==========================================================================
 * ASSERT
 * ========================================================================== */

#define configASSERT(x) if ((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

#endif /* FREERTOS_CONFIG_H */
