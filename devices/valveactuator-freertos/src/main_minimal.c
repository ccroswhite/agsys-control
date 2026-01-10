/**
 * @file main_minimal.c
 * @brief Valve Actuator FreeRTOS Application (nRF52832)
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

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
#include "valve_task.h"
#include "led_task.h"
#include "agsys_device.h"
#include "agsys_approtect.h"

/* Task handles */
static TaskHandle_t m_can_task_handle = NULL;
static TaskHandle_t m_valve_task_handle = NULL;
static TaskHandle_t m_led_task_handle = NULL;

/* Device context (BLE, FRAM, auth) */
static agsys_device_ctx_t m_device_ctx;

/* Pairing mode (non-static for LED task access) */
bool g_pairing_mode = false;
TickType_t g_pairing_start_tick = 0;

/* ==========================================================================
 * DIP SWITCH READING
 * ========================================================================== */

static uint8_t read_device_address(void)
{
    /* Configure DIP switch pins as inputs with pullup */
    nrf_gpio_cfg_input(DIP_1_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(DIP_2_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(DIP_3_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(DIP_4_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(DIP_5_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(DIP_6_PIN, NRF_GPIO_PIN_PULLUP);
    
    nrf_delay_us(10);  /* Let pins settle */
    
    /* Read address (active low switches) */
    uint8_t addr = 0;
    if (!nrf_gpio_pin_read(DIP_1_PIN)) addr |= 0x01;
    if (!nrf_gpio_pin_read(DIP_2_PIN)) addr |= 0x02;
    if (!nrf_gpio_pin_read(DIP_3_PIN)) addr |= 0x04;
    if (!nrf_gpio_pin_read(DIP_4_PIN)) addr |= 0x08;
    if (!nrf_gpio_pin_read(DIP_5_PIN)) addr |= 0x10;
    if (!nrf_gpio_pin_read(DIP_6_PIN)) addr |= 0x20;
    
    return addr;
}

static void init_leds(void)
{
    nrf_gpio_cfg_output(LED_POWER_PIN);
    nrf_gpio_cfg_output(LED_24V_PIN);
    nrf_gpio_cfg_output(LED_STATUS_PIN);
    nrf_gpio_cfg_output(LED_VALVE_OPEN_PIN);
    
    /* Power LED on */
    nrf_gpio_pin_set(LED_POWER_PIN);
    
    /* Others off */
    nrf_gpio_pin_clear(LED_24V_PIN);
    nrf_gpio_pin_clear(LED_STATUS_PIN);
    nrf_gpio_pin_clear(LED_VALVE_OPEN_PIN);
}

/* ==========================================================================
 * PAIRING MODE
 * ========================================================================== */

static bool check_pairing_button(void)
{
    /* Configure pairing button */
    nrf_gpio_cfg_input(PAIRING_BUTTON_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_delay_us(10);
    
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
    g_pairing_mode = true;
    g_pairing_start_tick = xTaskGetTickCount();
    agsys_device_start_advertising(&m_device_ctx);
}

void exit_pairing_mode(void)
{
    SEGGER_RTT_printf(0, "Exiting pairing mode\n");
    g_pairing_mode = false;
    agsys_device_stop_advertising(&m_device_ctx);
}

/* ==========================================================================
 * SOFTDEVICE INIT
 * ========================================================================== */

static void softdevice_init(void)
{
    ret_code_t err_code;
    
    err_code = nrf_sdh_enable_request();
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "SoftDevice enable failed: %d\n", err_code);
    }
    
    /* Configure BLE stack */
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(1, &ram_start);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "BLE config failed: %d\n", err_code);
    }
    
    err_code = nrf_sdh_ble_enable(&ram_start);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "BLE enable failed: %d\n", err_code);
    }
    
    SEGGER_RTT_printf(0, "SoftDevice initialized, RAM start: 0x%08X\n", ram_start);
}

/* ==========================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    /* Initialize clock */
    ret_code_t err_code = nrf_drv_clock_init();
    if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
        SEGGER_RTT_printf(0, "Clock init failed: %d\n", err_code);
    }
    
    SEGGER_RTT_printf(0, "\n\n=== Valve Actuator FreeRTOS ===\n");
    
    /* Initialize LEDs */
    init_leds();
    
    /* Read device address from DIP switches */
    uint8_t device_address = read_device_address();
    SEGGER_RTT_printf(0, "Device address: %d\n", device_address);
    
    /* Check if pairing button held at boot (before BLE init) */
    bool start_pairing = check_pairing_button();
    if (start_pairing) {
        SEGGER_RTT_printf(0, "Pairing button held - will enter pairing mode\n");
    }
    
    /* Initialize SoftDevice */
    softdevice_init();
    
    /* Initialize device (FRAM, Flash, BLE auth, BLE service) */
    agsys_device_init_t dev_init = {
        .device_name = "AgActuator",
        .device_type = AGSYS_DEVICE_TYPE_VALVE_ACTUATOR,
        .fram_cs_pin = SPI_CS_FRAM_PIN,
        .flash_cs_pin = SPI_CS_FLASH_PIN,
        .evt_handler = NULL
    };
    if (!agsys_device_init(&m_device_ctx, &dev_init)) {
        SEGGER_RTT_printf(0, "WARNING: Device init failed\n");
    }
    
    /* Start pairing mode after BLE is initialized */
    if (start_pairing) {
        enter_pairing_mode();
    }
    
    /* Create tasks */
    xTaskCreate(can_task, "CAN", TASK_STACK_CAN, 
                (void *)(uintptr_t)device_address, TASK_PRIORITY_CAN, &m_can_task_handle);
    
    xTaskCreate(valve_task, "Valve", TASK_STACK_VALVE, 
                NULL, TASK_PRIORITY_VALVE, &m_valve_task_handle);
    
    xTaskCreate(led_task, "LED", TASK_STACK_LED, 
                NULL, TASK_PRIORITY_LED, &m_led_task_handle);
    
    SEGGER_RTT_printf(0, "Tasks created\n");
    
    /* Start SoftDevice FreeRTOS thread */
    nrf_sdh_freertos_init(NULL, NULL);
    
    SEGGER_RTT_printf(0, "Starting FreeRTOS scheduler...\n");
    
    /* Start scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
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
    __WFE();
}

/* Static allocation support */
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
