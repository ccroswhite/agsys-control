/**
 * @file main.c
 * @brief Valve Actuator Main Application (FreeRTOS)
 * 
 * Controls a single motorized ball valve via discrete H-bridge,
 * communicates with valve controller via CAN bus.
 * 
 * Tasks:
 *   - CAN Task (Priority 4): Processes CAN messages from interrupt
 *   - Valve Task (Priority 3): State machine for valve control
 *   - BLE Task (Priority 2): Handles BLE events (DFU)
 *   - LED Task (Priority 1): Status LED patterns
 */

#include "agsys_common.h"
#include "agsys_debug.h"
#include "agsys_spi.h"
#include "agsys_fram.h"
#include "agsys_ble.h"

#include "can_task.h"
#include "valve_task.h"
#include "led_task.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"
#include "nrf_pwr_mgmt.h"
#include "app_timer.h"

/* ==========================================================================
 * GLOBAL STATE
 * ========================================================================== */

static agsys_fram_ctx_t g_fram_ctx;
static agsys_ble_ctx_t g_ble_ctx;

static uint8_t g_device_address = 0;

/* Pairing mode (non-static for LED task access) */
bool g_pairing_mode = false;
static uint32_t g_pairing_start_time = 0;

/* Task handles */
static TaskHandle_t g_can_task_handle = NULL;
static TaskHandle_t g_valve_task_handle = NULL;
/* BLE task handle - reserved for future use */
__attribute__((unused)) static TaskHandle_t g_ble_task_handle = NULL;
static TaskHandle_t g_led_task_handle = NULL;

/* ==========================================================================
 * FORWARD DECLARATIONS
 * ========================================================================== */

static void init_gpio(void);
static void init_softdevice(void);
static uint8_t read_device_address(void);
static void ble_evt_handler(const agsys_ble_evt_t *evt);
static bool check_pairing_button(void);
static void enter_pairing_mode(void);
static void exit_pairing_mode(void);

/* ==========================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    /* Initialize logging */
    AGSYS_LOG_INIT();
    AGSYS_LOG_INFO("Valve Actuator Starting (FreeRTOS)...");

    /* Initialize power management */
    nrf_pwr_mgmt_init();

    /* Initialize GPIO */
    init_gpio();

    /* Read device address from DIP switches */
    g_device_address = read_device_address();
    AGSYS_LOG_INFO("Device address: %d", g_device_address);

    if (g_device_address == 0 || g_device_address > 64) {
        AGSYS_LOG_ERROR("Invalid address! Check DIP switches.");
    }

    /* Initialize SPI bus */
    agsys_err_t err = agsys_spi_init(AGSYS_SPI_SCK_PIN, 
                                      AGSYS_SPI_MOSI_PIN, 
                                      AGSYS_SPI_MISO_PIN);
    if (err != AGSYS_OK) {
        AGSYS_LOG_ERROR("SPI init failed: %d", err);
    }

    /* Initialize FRAM */
    err = agsys_fram_init(&g_fram_ctx, AGSYS_FRAM_CS_PIN);
    if (err != AGSYS_OK) {
        AGSYS_LOG_WARNING("FRAM init failed: %d", err);
    }

    /* Initialize SoftDevice and BLE */
    init_softdevice();

    agsys_ble_init_t ble_init = {
        .device_name = AGSYS_BLE_NAME_PREFIX,
        .device_type = AGSYS_DEVICE_TYPE_VALVE_ACTUATOR,
        .evt_handler = ble_evt_handler,
        .enable_dfu = true,
    };
    agsys_ble_init(&g_ble_ctx, &ble_init);

    /* Check if pairing button held at boot */
    if (check_pairing_button()) {
        AGSYS_LOG_INFO("Pairing button held at boot - entering pairing mode");
        enter_pairing_mode();
    }

    /* Create tasks */
    AGSYS_LOG_INFO("Creating tasks...");

    xTaskCreate(can_task,
                "CAN",
                AGSYS_TASK_STACK_CAN,
                (void *)(uintptr_t)g_device_address,
                AGSYS_TASK_PRIORITY_CAN,
                &g_can_task_handle);

    xTaskCreate(valve_task,
                "Valve",
                AGSYS_TASK_STACK_VALVE,
                NULL,
                AGSYS_TASK_PRIORITY_VALVE,
                &g_valve_task_handle);

    xTaskCreate(led_task,
                "LED",
                AGSYS_TASK_STACK_LED,
                NULL,
                AGSYS_TASK_PRIORITY_LED,
                &g_led_task_handle);

    /* Start SoftDevice FreeRTOS thread (handles BLE events) */
    nrf_sdh_freertos_init(NULL, NULL);

    AGSYS_LOG_INFO("Starting scheduler...");

    /* Start FreeRTOS scheduler - does not return */
    vTaskStartScheduler();

    /* Should never reach here */
    AGSYS_LOG_ERROR("Scheduler exited!");
    for (;;) {
        __WFE();
    }
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

static void init_gpio(void)
{
    /* LEDs - output, active high */
    nrf_gpio_cfg_output(AGSYS_LED_3V3_PIN);
    nrf_gpio_cfg_output(AGSYS_LED_24V_PIN);
    nrf_gpio_cfg_output(AGSYS_LED_STATUS_PIN);
    nrf_gpio_cfg_output(AGSYS_LED_VALVE_OPEN_PIN);

    nrf_gpio_pin_set(AGSYS_LED_3V3_PIN);    /* Power LED on */
    nrf_gpio_pin_set(AGSYS_LED_24V_PIN);    /* 24V LED on (assume present) */
    nrf_gpio_pin_clear(AGSYS_LED_STATUS_PIN);
    nrf_gpio_pin_clear(AGSYS_LED_VALVE_OPEN_PIN);

    /* DIP switches - input with pullup (active LOW) */
    nrf_gpio_cfg_input(AGSYS_DIP_1_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_DIP_2_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_DIP_3_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_DIP_4_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_DIP_5_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_DIP_6_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_DIP_10_PIN, NRF_GPIO_PIN_PULLUP);

    /* Limit switches - input with pullup (active LOW) */
    nrf_gpio_cfg_input(AGSYS_LIMIT_OPEN_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_LIMIT_CLOSED_PIN, NRF_GPIO_PIN_PULLUP);

    /* Pairing button - input (external pullup) */
    nrf_gpio_cfg_input(AGSYS_PAIRING_BUTTON_PIN, NRF_GPIO_PIN_NOPULL);

    /* CAN interrupt - input with pullup */
    nrf_gpio_cfg_input(AGSYS_CAN_INT_PIN, NRF_GPIO_PIN_PULLUP);

    AGSYS_LOG_DEBUG("GPIO initialized");
}

static void init_softdevice(void)
{
    ret_code_t err;

    err = nrf_sdh_enable_request();
    AGSYS_ASSERT(err == NRF_SUCCESS);

    /* Configure BLE stack */
    uint32_t ram_start = 0;
    err = nrf_sdh_ble_default_cfg_set(1, &ram_start);
    AGSYS_ASSERT(err == NRF_SUCCESS);

    err = nrf_sdh_ble_enable(&ram_start);
    AGSYS_ASSERT(err == NRF_SUCCESS);

    AGSYS_LOG_DEBUG("SoftDevice initialized");
}

static uint8_t read_device_address(void)
{
    uint8_t addr = 0;

    /* DIP switches are active LOW (ON = LOW = 0) */
    if (nrf_gpio_pin_read(AGSYS_DIP_1_PIN) == 0) addr |= 0x01;
    if (nrf_gpio_pin_read(AGSYS_DIP_2_PIN) == 0) addr |= 0x02;
    if (nrf_gpio_pin_read(AGSYS_DIP_3_PIN) == 0) addr |= 0x04;
    if (nrf_gpio_pin_read(AGSYS_DIP_4_PIN) == 0) addr |= 0x08;
    if (nrf_gpio_pin_read(AGSYS_DIP_5_PIN) == 0) addr |= 0x10;
    if (nrf_gpio_pin_read(AGSYS_DIP_6_PIN) == 0) addr |= 0x20;

    return addr;
}

static void ble_evt_handler(const agsys_ble_evt_t *evt)
{
    switch (evt->type) {
        case AGSYS_BLE_EVT_CONNECTED:
            AGSYS_LOG_INFO("BLE: Connected");
            break;

        case AGSYS_BLE_EVT_DISCONNECTED:
            AGSYS_LOG_INFO("BLE: Disconnected");
            /* Exit pairing mode on disconnect */
            if (g_pairing_mode) {
                exit_pairing_mode();
            }
            break;

        default:
            break;
    }
}

/* ==========================================================================
 * PAIRING MODE
 * ========================================================================== */

static bool check_pairing_button(void)
{
    /* Check if button is pressed (active LOW with external pullup) */
    if (nrf_gpio_pin_read(AGSYS_PAIRING_BUTTON_PIN) != 0) {
        return false;
    }
    
    /* Debounce */
    nrf_delay_ms(50);
    if (nrf_gpio_pin_read(AGSYS_PAIRING_BUTTON_PIN) != 0) {
        return false;
    }
    
    /* Wait for hold duration */
    uint32_t start = 0;
    while (start < PAIRING_BUTTON_HOLD_MS) {
        if (nrf_gpio_pin_read(AGSYS_PAIRING_BUTTON_PIN) != 0) {
            return false;  /* Released early */
        }
        nrf_delay_ms(10);
        start += 10;
        
        /* Blink LED while holding */
        if ((start / 250) % 2) {
            nrf_gpio_pin_clear(AGSYS_LED_STATUS_PIN);
        } else {
            nrf_gpio_pin_set(AGSYS_LED_STATUS_PIN);
        }
    }
    
    nrf_gpio_pin_set(AGSYS_LED_STATUS_PIN);
    return true;
}

static void enter_pairing_mode(void)
{
    if (g_pairing_mode) return;
    
    AGSYS_LOG_INFO("Entering pairing mode");
    g_pairing_mode = true;
    g_pairing_start_time = xTaskGetTickCount();
    
    /* Start BLE advertising */
    agsys_ble_advertising_start(&g_ble_ctx);
}

static void exit_pairing_mode(void)
{
    if (!g_pairing_mode) return;
    
    AGSYS_LOG_INFO("Exiting pairing mode");
    g_pairing_mode = false;
    
    /* Stop BLE advertising */
    agsys_ble_advertising_stop(&g_ble_ctx);
}

/* ==========================================================================
 * FREERTOS HOOKS
 * ========================================================================== */

void vApplicationMallocFailedHook(void)
{
    AGSYS_LOG_ERROR("Malloc failed!");
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    AGSYS_LOG_ERROR("Stack overflow in task: %s", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationIdleHook(void)
{
    /* Enter low power mode */
    __WFE();
}

/* Static allocation support for FreeRTOS */
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
