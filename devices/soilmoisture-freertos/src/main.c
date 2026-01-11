/**
 * @file main.c
 * @brief Soil Moisture Sensor FreeRTOS Application (nRF52832)
 * 
 * Battery-powered sensor with ultra-low power operation:
 * - Wake every 2 hours from deep sleep
 * - Read 4 moisture probes (oscillator frequency measurement)
 * - Read battery voltage
 * - Transmit via LoRa to property controller
 * - Return to deep sleep
 * 
 * BLE pairing mode activated by holding button at boot.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"

#include "SEGGER_RTT.h"

#include "board_config.h"
#include "spi_driver.h"
#include "lora_task.h"
#include "freq_counter.h"
#include "sleep_manager.h"
#include "agsys_device.h"
#include "agsys_protocol.h"
#include "agsys_approtect.h"

/* ==========================================================================
 * SHARED RESOURCES
 * ========================================================================== */

/* SPI bus mutex - shared by LoRa, FRAM, Flash */
SemaphoreHandle_t g_spi_mutex = NULL;

/* Device context (BLE, FRAM, Flash, auth) - non-static for logging access */
agsys_device_ctx_t m_device_ctx;

/* Sensor readings */
typedef struct {
    uint32_t frequency;
    uint8_t moisture_percent;
    bool valid;
} probe_reading_t;

static probe_reading_t m_probes[MAX_PROBES];
static uint16_t m_battery_mv = 0;

/* Power state */
static bool m_low_battery = false;
static bool m_critical_battery = false;

/* Pairing mode */
static bool m_pairing_mode = false;
static TickType_t m_pairing_start_tick = 0;

/* Task handles */
static TaskHandle_t m_sensor_task_handle = NULL;
static TaskHandle_t m_lora_task_handle = NULL;
static TaskHandle_t m_led_task_handle = NULL;

/* Sleep timer */
static TimerHandle_t m_sleep_timer = NULL;

/* ==========================================================================
 * FORWARD DECLARATIONS
 * ========================================================================== */

static void sensor_task(void *pvParameters);
static void led_task(void *pvParameters);
static void sleep_timer_callback(TimerHandle_t xTimer);
static void enter_deep_sleep(void);
static bool check_pairing_button(void);

/* ==========================================================================
 * LED TASK
 * ========================================================================== */

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    SEGGER_RTT_printf(0, "LED task started\n");
    
    nrf_gpio_cfg_output(LED_STATUS_PIN);
    nrf_gpio_pin_set(LED_STATUS_PIN);  /* LED off (active LOW) */
    
    for (;;) {
        if (m_pairing_mode) {
            /* Check for pairing timeout */
            TickType_t now = xTaskGetTickCount();
            if ((now - m_pairing_start_tick) >= pdMS_TO_TICKS(BLE_PAIRING_TIMEOUT_MS)) {
                SEGGER_RTT_printf(0, "Pairing timeout - exiting pairing mode\n");
                m_pairing_mode = false;
                agsys_device_stop_advertising(&m_device_ctx);
            } else {
                /* Fast blink in pairing mode */
                nrf_gpio_pin_clear(LED_STATUS_PIN);
                vTaskDelay(pdMS_TO_TICKS(100));
                nrf_gpio_pin_set(LED_STATUS_PIN);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        } else if (m_critical_battery) {
            /* SOS pattern for critical battery */
            for (int i = 0; i < 3; i++) {
                nrf_gpio_pin_clear(LED_STATUS_PIN);
                vTaskDelay(pdMS_TO_TICKS(100));
                nrf_gpio_pin_set(LED_STATUS_PIN);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            /* Normal: LED off, blink briefly on activity */
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

/* ==========================================================================
 * SENSOR TASK
 * Reads moisture probes and battery voltage
 * ========================================================================== */

static void sensor_task(void *pvParameters)
{
    (void)pvParameters;
    SEGGER_RTT_printf(0, "Sensor task started\n");
    
    /* Initialize frequency counter */
    if (!freq_counter_init()) {
        SEGGER_RTT_printf(0, "Sensor: Failed to init freq counter!\n");
    }
    
    for (;;) {
        /* Wait for notification to take reading */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        SEGGER_RTT_printf(0, "Sensor: Starting measurement\n");
        
        /* Power on probes and wait for stabilization */
        freq_counter_power_on();
        vTaskDelay(pdMS_TO_TICKS(PROBE_STABILIZE_MS));
        
        /* Read each probe */
        for (int i = 0; i < NUM_MOISTURE_PROBES; i++) {
            m_probes[i].frequency = freq_counter_measure(i, PROBE_MEASUREMENT_MS);
            m_probes[i].valid = freq_counter_is_valid(m_probes[i].frequency);
            
            /* Convert frequency to moisture percent using calibration
             * TODO: Load calibration from FRAM
             * For now, use simple linear mapping:
             * f_dry ~= 500kHz (0%), f_wet ~= 100kHz (100%)
             */
            if (m_probes[i].valid && m_probes[i].frequency > 0) {
                uint32_t f_dry = 500000;
                uint32_t f_wet = 100000;
                if (m_probes[i].frequency >= f_dry) {
                    m_probes[i].moisture_percent = 0;
                } else if (m_probes[i].frequency <= f_wet) {
                    m_probes[i].moisture_percent = 100;
                } else {
                    m_probes[i].moisture_percent = 
                        (uint8_t)(100 * (f_dry - m_probes[i].frequency) / (f_dry - f_wet));
                }
            } else {
                m_probes[i].moisture_percent = 0;
            }
            
            SEGGER_RTT_printf(0, "Probe %d: freq=%lu Hz, moisture=%d%%%s\n",
                              i, m_probes[i].frequency, m_probes[i].moisture_percent,
                              m_probes[i].valid ? "" : " (INVALID)");
        }
        
        /* Power off probes */
        freq_counter_power_off();
        
        /* Read battery voltage */
        /* TODO: Implement SAADC reading */
        m_battery_mv = 3700;  /* Placeholder */
        
        m_low_battery = (m_battery_mv < BATTERY_LOW_MV);
        m_critical_battery = (m_battery_mv < BATTERY_CRITICAL_MV);
        
        SEGGER_RTT_printf(0, "Battery: %d mV%s\n", m_battery_mv,
                          m_critical_battery ? " (CRITICAL)" :
                          m_low_battery ? " (LOW)" : "");
        
        /* Notify LoRa task that data is ready */
        if (m_lora_task_handle != NULL) {
            xTaskNotifyGive(m_lora_task_handle);
        }
    }
}

/* ==========================================================================
 * SLEEP MANAGEMENT
 * ========================================================================== */

static void sleep_timer_callback(TimerHandle_t xTimer)
{
    (void)xTimer;
    SEGGER_RTT_printf(0, "Entering deep sleep...\n");
    enter_deep_sleep();
}

static void enter_deep_sleep(void)
{
    /* Prepare peripherals for sleep */
    sleep_manager_prepare_sleep();
    
    /* Turn off probe power */
    freq_counter_power_off();
    
    /* Calculate sleep duration */
    uint32_t sleep_ms = SLEEP_INTERVAL_MS;
    if (m_critical_battery) {
        sleep_ms *= 4;  /* Extended sleep when critical */
        SEGGER_RTT_printf(0, "Critical battery - extended sleep\n");
    }
    
    /* Enter deep sleep */
    uint32_t actual_sleep = sleep_manager_sleep(sleep_ms);
    
    /* Check if woken by button */
    if (sleep_manager_woken_by_button()) {
        SEGGER_RTT_printf(0, "Button wake detected\n");
        
        /* Check if button held for pairing mode */
        if (check_pairing_button()) {
            SEGGER_RTT_printf(0, "Entering pairing mode (timeout: %d sec)\n",
                              BLE_PAIRING_TIMEOUT_MS / 1000);
            m_pairing_mode = true;
            m_pairing_start_tick = xTaskGetTickCount();
        } else if (actual_sleep < sleep_ms - 1000) {
            /* Button released early, go back to sleep */
            uint32_t remaining = sleep_ms - actual_sleep;
            SEGGER_RTT_printf(0, "Going back to sleep for %lu ms\n", remaining);
            sleep_manager_sleep(remaining);
        }
    }
    
    /* Restore peripherals */
    sleep_manager_restore_wake();
    sleep_manager_clear_wake_flags();
    
    /* Wake - trigger sensor reading */
    if (m_sensor_task_handle != NULL && !m_pairing_mode) {
        xTaskNotifyGive(m_sensor_task_handle);
    }
}

/* ==========================================================================
 * PAIRING BUTTON
 * ========================================================================== */

static bool check_pairing_button(void)
{
    nrf_gpio_cfg_input(PAIRING_BUTTON_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* Check if button pressed */
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
    
    nrf_gpio_pin_set(LED_STATUS_PIN);
    return true;
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
        .device_name = "AgSoil",
        .device_type = AGSYS_DEVICE_TYPE_SOIL_MOISTURE,
        .fram_cs_pin = SPI_CS_FRAM_PIN,
        .flash_cs_pin = SPI_CS_FLASH_PIN,
        .evt_handler = NULL
    };
    if (!agsys_device_init(&m_device_ctx, &dev_init)) {
        SEGGER_RTT_printf(0, "WARNING: Device init failed\n");
    }
    
    /* Create sleep timer (one-shot, 1 second delay before sleep) */
    m_sleep_timer = xTimerCreate("Sleep", pdMS_TO_TICKS(1000), pdFALSE,
                                  NULL, sleep_timer_callback);
    if (m_sleep_timer == NULL) {
        SEGGER_RTT_printf(0, "Failed to create sleep timer\n");
        return false;
    }
    
    return true;
}

/* ==========================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    /* Enable DC-DC converter for lower power */
    NRF_POWER->DCDCEN = 1;
    
    ret_code_t err_code = nrf_drv_clock_init();
    if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
        SEGGER_RTT_printf(0, "Clock init failed: %d\n", err_code);
    }
    
    SEGGER_RTT_printf(0, "\n\n=== Soil Moisture Sensor FreeRTOS ===\n");
    SEGGER_RTT_printf(0, "nRF52832 + S132 SoftDevice\n\n");
    
    /* Check for pairing button held at boot */
    nrf_gpio_cfg_output(LED_STATUS_PIN);
    bool start_pairing = check_pairing_button();
    if (start_pairing) {
        SEGGER_RTT_printf(0, "Pairing button held - will enter pairing mode (timeout: %d sec)\n",
                          BLE_PAIRING_TIMEOUT_MS / 1000);
    }
    
    softdevice_init();
    
    if (!create_shared_resources()) {
        SEGGER_RTT_printf(0, "Failed to create shared resources!\n");
        for (;;) { __WFE(); }
    }
    
    /* Start pairing mode after BLE is initialized */
    if (start_pairing) {
        m_pairing_mode = true;
        m_pairing_start_tick = xTaskGetTickCount();
        agsys_device_start_advertising(&m_device_ctx);
    }
    
    /* Create tasks */
    xTaskCreate(sensor_task, "Sensor", TASK_STACK_SENSOR,
                NULL, TASK_PRIORITY_SENSOR, &m_sensor_task_handle);
    
    xTaskCreate(lora_task, "LoRa", TASK_STACK_LORA,
                NULL, TASK_PRIORITY_LORA, &m_lora_task_handle);
    
    xTaskCreate(led_task, "LED", TASK_STACK_LED,
                NULL, TASK_PRIORITY_LED, &m_led_task_handle);
    
    SEGGER_RTT_printf(0, "Tasks created\n");
    
    nrf_sdh_freertos_init(NULL, NULL);
    
    /* Trigger initial sensor reading */
    if (m_sensor_task_handle != NULL) {
        xTaskNotifyGive(m_sensor_task_handle);
    }
    
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

/* Static allocation callbacks */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
    *ppxIdleTaskStackBuffer = xIdleStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
    *ppxTimerTaskStackBuffer = xTimerStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
