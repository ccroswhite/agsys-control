/**
 * @file main.c
 * @brief Valve Controller FreeRTOS Application (nRF52832)
 * 
 * Task Architecture:
 * - CAN Task (Priority 5): Manages MCP2515 CAN bus to valve actuators
 * - LoRa Task (Priority 4): Communication with property controller
 * - Schedule Task (Priority 3): Time-based irrigation scheduling
 * - BLE Task (Priority 2): Local configuration via mobile app
 * - LED Task (Priority 1): Status indicators
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"

#include "SEGGER_RTT.h"

#include "board_config.h"
#include "can_task.h"
#include "lora_task.h"
#include "schedule_task.h"
#include "agsys_device.h"
#include "agsys_approtect.h"

/* ==========================================================================
 * SHARED RESOURCES
 * ========================================================================== */

/* SPI bus mutex - shared by CAN, LoRa, FRAM */
SemaphoreHandle_t g_spi_mutex = NULL;

/* Device context (BLE, FRAM, Flash, auth) - non-static for logging access */
agsys_device_ctx_t m_device_ctx;

/* Power state */
volatile bool g_on_battery_power = false;
volatile bool g_power_fail_flag = false;

/* Pairing mode */
bool g_pairing_mode_active = false;
TickType_t g_pairing_start_tick = 0;

/* Task handles */
static TaskHandle_t m_can_task_handle = NULL;
static TaskHandle_t m_lora_task_handle = NULL;
static TaskHandle_t m_schedule_task_handle = NULL;
static TaskHandle_t m_led_task_handle = NULL;

/* Forward declarations */
static void exit_pairing_mode(void);

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    SEGGER_RTT_printf(0, "LED task started\n");
    
    nrf_gpio_cfg_output(LED_3V3_PIN);
    nrf_gpio_cfg_output(LED_24V_PIN);
    nrf_gpio_cfg_output(LED_STATUS_PIN);
    
    /* 3.3V LED always on */
    nrf_gpio_pin_set(LED_3V3_PIN);
    
    bool led_state = false;
    TickType_t last_toggle = 0;
    
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        
        /* 24V LED - on when not on battery */
        if (g_on_battery_power) {
            nrf_gpio_pin_clear(LED_24V_PIN);
        } else {
            nrf_gpio_pin_set(LED_24V_PIN);
        }
        
        /* Status LED patterns */
        if (g_pairing_mode_active) {
            /* Check for pairing timeout */
            if ((now - g_pairing_start_tick) >= pdMS_TO_TICKS(BLE_PAIRING_TIMEOUT_MS)) {
                exit_pairing_mode();
            } else {
                /* Fast blink in pairing mode (100ms) */
                if (now - last_toggle >= pdMS_TO_TICKS(100)) {
                    led_state = !led_state;
                    nrf_gpio_pin_write(LED_STATUS_PIN, led_state);
                    last_toggle = now;
                }
            }
        } else if (g_on_battery_power) {
            /* Slow blink on battery (1000ms) */
            if (now - last_toggle >= pdMS_TO_TICKS(1000)) {
                led_state = !led_state;
                nrf_gpio_pin_write(LED_STATUS_PIN, led_state);
                last_toggle = now;
            }
        } else {
            /* Off in normal operation */
            nrf_gpio_pin_clear(LED_STATUS_PIN);
            led_state = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ==========================================================================
 * POWER FAIL HANDLING
 * ========================================================================== */

static void power_fail_check(void)
{
    /* Check power fail pin */
    if (nrf_gpio_pin_read(POWER_FAIL_PIN) == 0) {
        if (!g_on_battery_power) {
            SEGGER_RTT_printf(0, "POWER FAIL: Switching to battery\n");
            g_on_battery_power = true;
            
            /* Emergency close all valves via CAN */
            can_emergency_close_all();
        }
    } else {
        if (g_on_battery_power) {
            SEGGER_RTT_printf(0, "POWER RESTORED\n");
            g_on_battery_power = false;
        }
    }
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

static void softdevice_init(void)
{
    ret_code_t err_code;
    
    err_code = nrf_sdh_enable_request();
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "SoftDevice enable failed: %d\n", err_code);
        return;
    }
    
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(1, &ram_start);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "BLE config failed: %d\n", err_code);
        return;
    }
    
    err_code = nrf_sdh_ble_enable(&ram_start);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "BLE enable failed: %d\n", err_code);
        return;
    }
    
    SEGGER_RTT_printf(0, "SoftDevice initialized, RAM start: 0x%08X\n", ram_start);
}

static bool create_shared_resources(void)
{
    g_spi_mutex = xSemaphoreCreateMutex();
    if (g_spi_mutex == NULL) {
        SEGGER_RTT_printf(0, "Failed to create SPI mutex\n");
        return false;
    }
    
    /* Initialize device (FRAM, Flash, BLE auth, BLE service) */
    agsys_device_init_t dev_init = {
        .device_name = "AgValve",
        .device_type = AGSYS_DEVICE_TYPE_VALVE_CONTROLLER,
        .fram_cs_pin = SPI_CS_FRAM_PIN,
        .flash_cs_pin = SPI_CS_FLASH_PIN,
        .evt_handler = NULL
    };
    if (!agsys_device_init(&m_device_ctx, &dev_init)) {
        SEGGER_RTT_printf(0, "WARNING: Device init failed\n");
    }
    
    /* Initialize task modules */
    if (!can_task_init()) {
        SEGGER_RTT_printf(0, "Failed to init CAN task\n");
        return false;
    }
    
    if (!lora_task_init()) {
        SEGGER_RTT_printf(0, "Failed to init LoRa task\n");
        return false;
    }
    
    if (!schedule_task_init()) {
        SEGGER_RTT_printf(0, "Failed to init Schedule task\n");
        return false;
    }
    
    SEGGER_RTT_printf(0, "Shared resources created\n");
    return true;
}

/* ==========================================================================
 * PAIRING MODE
 * ========================================================================== */

static bool check_pairing_button(void)
{
    /* Check if button is pressed (active LOW) */
    if (nrf_gpio_pin_read(PAIRING_BUTTON_PIN) != 0) {
        return false;
    }
    
    /* Debounce */
    nrf_delay_ms(50);
    if (nrf_gpio_pin_read(PAIRING_BUTTON_PIN) != 0) {
        return false;
    }
    
    /* Wait for hold duration */
    uint32_t start = 0;
    while (start < PAIRING_BUTTON_HOLD_MS) {
        if (nrf_gpio_pin_read(PAIRING_BUTTON_PIN) != 0) {
            return false;  /* Released early */
        }
        nrf_delay_ms(10);
        start += 10;
        
        /* Blink LED while holding */
        if ((start / 250) % 2) {
            nrf_gpio_pin_clear(LED_STATUS_PIN);
        } else {
            nrf_gpio_pin_set(LED_STATUS_PIN);
        }
    }
    
    nrf_gpio_pin_clear(LED_STATUS_PIN);
    return true;
}

static void enter_pairing_mode(void)
{
    SEGGER_RTT_printf(0, "Entering pairing mode (timeout: %d sec)\n",
                      BLE_PAIRING_TIMEOUT_MS / 1000);
    g_pairing_mode_active = true;
    g_pairing_start_tick = xTaskGetTickCount();
    agsys_device_start_advertising(&m_device_ctx);
}

static void exit_pairing_mode(void)
{
    SEGGER_RTT_printf(0, "Exiting pairing mode\n");
    g_pairing_mode_active = false;
    agsys_device_stop_advertising(&m_device_ctx);
}

/* ==========================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
        SEGGER_RTT_printf(0, "Clock init failed: %d\n", err_code);
    }
    
    SEGGER_RTT_printf(0, "\n\n=== Valve Controller FreeRTOS ===\n");
    SEGGER_RTT_printf(0, "nRF52832 + S132 SoftDevice\n\n");
    
    /* Configure power fail pin */
    nrf_gpio_cfg_input(POWER_FAIL_PIN, NRF_GPIO_PIN_PULLUP);
    g_on_battery_power = (nrf_gpio_pin_read(POWER_FAIL_PIN) == 0);
    if (g_on_battery_power) {
        SEGGER_RTT_printf(0, "WARNING: Starting on battery power\n");
    }
    
    /* Configure pairing button */
    nrf_gpio_cfg_input(PAIRING_BUTTON_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* Configure LED for pairing button feedback */
    nrf_gpio_cfg_output(LED_STATUS_PIN);
    
    /* Check if pairing button held at boot (before BLE init) */
    bool start_pairing = check_pairing_button();
    if (start_pairing) {
        SEGGER_RTT_printf(0, "Pairing button held - will enter pairing mode\n");
    }
    
    softdevice_init();
    
    if (!create_shared_resources()) {
        SEGGER_RTT_printf(0, "Failed to create shared resources!\n");
        for (;;) { __WFE(); }
    }
    
    /* Start pairing mode after BLE is initialized */
    if (start_pairing) {
        enter_pairing_mode();
    }
    
    /* Create tasks */
    xTaskCreate(can_task, "CAN", TASK_STACK_CAN,
                NULL, TASK_PRIORITY_CAN, &m_can_task_handle);
    
    xTaskCreate(lora_task, "LoRa", TASK_STACK_LORA,
                NULL, TASK_PRIORITY_LORA, &m_lora_task_handle);
    
    xTaskCreate(schedule_task, "Sched", TASK_STACK_SCHEDULE,
                NULL, TASK_PRIORITY_SCHEDULE, &m_schedule_task_handle);
    
    xTaskCreate(led_task, "LED", TASK_STACK_LED,
                NULL, TASK_PRIORITY_LED, &m_led_task_handle);
    
    SEGGER_RTT_printf(0, "Tasks created\n");
    
    nrf_sdh_freertos_init(NULL, NULL);
    
    SEGGER_RTT_printf(0, "Starting FreeRTOS scheduler...\n");
    
    vTaskStartScheduler();
    
    for (;;) {
        __WFE();
    }
}

/* ==========================================================================
 * FREERTOS HOOKS
 * ========================================================================== */

void vApplicationMallocFailedHook(void)
{
    SEGGER_RTT_printf(0, "Malloc failed!\n");
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    SEGGER_RTT_printf(0, "Stack overflow in task: %s\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationIdleHook(void)
{
    /* Check power state in idle */
    power_fail_check();
    __WFE();
}

static StaticTask_t xIdleTaskTCB;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = xIdleStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = xTimerStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
