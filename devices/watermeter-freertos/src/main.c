/**
 * @file main.c
 * @brief Water Meter (Mag Meter) FreeRTOS Application (nRF52840)
 * 
 * Electromagnetic flow meter with:
 * - ADS131M02 24-bit ADC for electrode signal
 * - ST7789 2.8" TFT display with LVGL
 * - 5-button navigation (UP, DOWN, LEFT, RIGHT, SELECT)
 * - LoRa reporting to property controller
 * - BLE for configuration and pairing
 * - FRAM for settings and calibration storage
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "queue.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"

#include "SEGGER_RTT.h"

#include "agsys_config.h"
#include "agsys_device.h"
#include "agsys_protocol.h"
#include "agsys_approtect.h"
#include "board_config.h"

/* ==========================================================================
 * SHARED RESOURCES
 * ========================================================================== */

/* SPI bus mutex - shared by ADC, Display, LoRa, FRAM */
SemaphoreHandle_t g_spi_mutex = NULL;

/* Device context (BLE, FRAM, Flash, auth) - non-static for logging access */
agsys_device_ctx_t m_device_ctx;

/* ==========================================================================
 * FLOW MEASUREMENT STATE
 * ========================================================================== */

typedef struct {
    float flow_rate_lpm;        /* Current flow rate (L/min) */
    float total_volume_l;       /* Total volume (liters) */
    float trend_volume_l;       /* Volume change in trend period */
    float avg_volume_l;         /* Average volume in avg period */
    float velocity_mps;         /* Flow velocity (m/s) */
    bool reverse_flow;          /* Reverse flow detected */
    uint8_t tier;               /* Meter tier (pipe size) */
} flow_state_t;

static flow_state_t m_flow_state = {0};

/* ==========================================================================
 * ALARM STATE
 * ========================================================================== */

typedef enum {
    ALARM_NONE = 0,
    ALARM_LEAK,
    ALARM_REVERSE_FLOW,
    ALARM_TAMPER,
    ALARM_HIGH_FLOW
} alarm_type_t;

typedef struct {
    alarm_type_t type;
    uint32_t start_time_sec;
    float flow_rate_lpm;
    float volume_l;
    bool acknowledged;
} alarm_state_t;

static alarm_state_t m_alarm_state = {0};

/* ==========================================================================
 * DISPLAY STATE
 * ========================================================================== */

typedef enum {
    DISPLAY_ACTIVE = 0,
    DISPLAY_DIM,
    DISPLAY_SLEEP
} display_power_t;

static display_power_t m_display_power = DISPLAY_ACTIVE;
static uint32_t m_last_activity_tick = 0;

/* ==========================================================================
 * PAIRING MODE
 * ========================================================================== */

#define BLE_PAIRING_TIMEOUT_MS      120000  /* 2 minutes */

static bool m_pairing_mode = false;
static TickType_t m_pairing_start_tick = 0;

/* ==========================================================================
 * TASK HANDLES
 * ========================================================================== */

static TaskHandle_t m_adc_task_handle = NULL;
static TaskHandle_t m_display_task_handle = NULL;
static TaskHandle_t m_lora_task_handle = NULL;
static TaskHandle_t m_button_task_handle = NULL;

/* ==========================================================================
 * BUTTON EVENT QUEUE
 * ========================================================================== */

typedef enum {
    BTN_NONE = 0,
    BTN_UP_SHORT,
    BTN_UP_LONG,
    BTN_DOWN_SHORT,
    BTN_DOWN_LONG,
    BTN_LEFT_SHORT,
    BTN_LEFT_LONG,
    BTN_RIGHT_SHORT,
    BTN_RIGHT_LONG,
    BTN_SELECT_SHORT,
    BTN_SELECT_LONG
} button_event_t;

static QueueHandle_t m_button_queue = NULL;

/* ==========================================================================
 * FORWARD DECLARATIONS
 * ========================================================================== */

static void adc_task(void *pvParameters);
static void display_task(void *pvParameters);
static void lora_task(void *pvParameters);
static void button_task(void *pvParameters);

static void softdevice_init(void);
static bool create_shared_resources(void);
static bool check_pairing_button(void);
static void enter_pairing_mode(void);
static void exit_pairing_mode(void);

/* ==========================================================================
 * UTILITY FUNCTIONS
 * ========================================================================== */

static bool check_pairing_button(void)
{
    nrf_gpio_cfg_input(AGSYS_BTN_SELECT_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_delay_ms(10);
    
    /* Check if SELECT button held for 2 seconds at boot */
    uint32_t hold_start = 0;
    while (nrf_gpio_pin_read(AGSYS_BTN_SELECT_PIN) == 0) {
        if (hold_start == 0) {
            hold_start = 1;
        }
        nrf_delay_ms(100);
        hold_start++;
        if (hold_start >= 20) {  /* 2 seconds */
            return true;
        }
    }
    return false;
}

static void enter_pairing_mode(void)
{
    m_pairing_mode = true;
    m_pairing_start_tick = xTaskGetTickCount();
    SEGGER_RTT_printf(0, "Entering pairing mode\n");
    
    /* Start BLE advertising */
    agsys_device_start_advertising(&m_device_ctx);
}

static void exit_pairing_mode(void)
{
    m_pairing_mode = false;
    SEGGER_RTT_printf(0, "Exiting pairing mode\n");
    
    /* Stop BLE advertising */
    agsys_device_stop_advertising(&m_device_ctx);
}


/* ==========================================================================
 * SOFTDEVICE INITIALIZATION
 * ========================================================================== */

static void softdevice_init(void)
{
    ret_code_t err_code;
    
    err_code = nrf_sdh_enable_request();
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "SoftDevice enable failed: %d\n", err_code);
        return;
    }
    
    uint32_t ram_start = 0x20000000;
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

/* ==========================================================================
 * SHARED RESOURCES
 * ========================================================================== */

static bool create_shared_resources(void)
{
    g_spi_mutex = xSemaphoreCreateMutex();
    if (g_spi_mutex == NULL) {
        SEGGER_RTT_printf(0, "Failed to create SPI mutex\n");
        return false;
    }
    
    /* Initialize device (FRAM, Flash, BLE auth, BLE service) */
    agsys_device_init_t dev_init = {
        .device_name = "AgMeter",
        .device_type = AGSYS_DEVICE_TYPE_WATER_METER,
        .fram_cs_pin = AGSYS_FRAM_CS_PIN,
        .flash_cs_pin = SPI_CS_FLASH_PIN,
        .evt_handler = NULL
    };
    if (!agsys_device_init(&m_device_ctx, &dev_init)) {
        SEGGER_RTT_printf(0, "WARNING: Device init failed\n");
    }
    
    /* Create button event queue */
    m_button_queue = xQueueCreate(10, sizeof(button_event_t));
    if (m_button_queue == NULL) {
        SEGGER_RTT_printf(0, "Failed to create button queue\n");
        return false;
    }
    
    return true;
}

/* ==========================================================================
 * ADC TASK - Signal acquisition and flow calculation
 * ========================================================================== */

static void adc_task(void *pvParameters)
{
    (void)pvParameters;
    
    SEGGER_RTT_printf(0, "ADC task started\n");
    
    /* TODO: Initialize ADS131M02 ADC */
    /* TODO: Initialize coil driver with hardware timers */
    
    TickType_t last_wake = xTaskGetTickCount();
    
    for (;;) {
        /* Sample at 1kHz, process synchronous detection */
        /* TODO: Read ADC, apply synchronous detection */
        /* TODO: Calculate flow rate from electrode signal */
        /* TODO: Update m_flow_state */
        
        /* For now, simulate flow data */
        m_flow_state.flow_rate_lpm = 0.0f;
        m_flow_state.reverse_flow = false;
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));
    }
}

/* ==========================================================================
 * DISPLAY TASK - LVGL UI management
 * ========================================================================== */

static void display_task(void *pvParameters)
{
    (void)pvParameters;
    
    SEGGER_RTT_printf(0, "Display task started\n");
    
    /* TODO: Initialize ST7789 display */
    /* TODO: Initialize LVGL */
    /* TODO: Create UI screens */
    
    TickType_t last_wake = xTaskGetTickCount();
    
    for (;;) {
        /* Check pairing mode timeout */
        if (m_pairing_mode) {
            TickType_t elapsed = xTaskGetTickCount() - m_pairing_start_tick;
            if (elapsed >= pdMS_TO_TICKS(BLE_PAIRING_TIMEOUT_MS)) {
                exit_pairing_mode();
            }
        }
        
        /* Process button events from queue */
        button_event_t btn_event;
        while (xQueueReceive(m_button_queue, &btn_event, 0) == pdTRUE) {
            /* Reset activity timer on any button press */
            m_last_activity_tick = xTaskGetTickCount();
            m_display_power = DISPLAY_ACTIVE;
            
            /* TODO: Handle button event in current screen */
            SEGGER_RTT_printf(0, "Button event: %d\n", btn_event);
        }
        
        /* Update display power state */
        TickType_t idle_time = xTaskGetTickCount() - m_last_activity_tick;
        if (m_alarm_state.type == ALARM_NONE) {
            if (idle_time > pdMS_TO_TICKS(AGSYS_DISPLAY_DIM_TIMEOUT_SEC * 1000 + 
                                          AGSYS_DISPLAY_SLEEP_TIMEOUT_SEC * 1000)) {
                m_display_power = DISPLAY_SLEEP;
            } else if (idle_time > pdMS_TO_TICKS(AGSYS_DISPLAY_DIM_TIMEOUT_SEC * 1000)) {
                m_display_power = DISPLAY_DIM;
            }
        }
        
        /* TODO: Call lv_timer_handler() for LVGL */
        /* TODO: Update main screen with flow data */
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20));  /* 50 Hz refresh */
    }
}

/* ==========================================================================
 * LORA TASK - Communication with property controller
 * ========================================================================== */

static void lora_task(void *pvParameters)
{
    (void)pvParameters;
    
    SEGGER_RTT_printf(0, "LoRa task started\n");
    
    /* TODO: Initialize RFM95C LoRa module */
    
    uint32_t report_interval_ms = 60000;  /* Default 60 seconds */
    TickType_t last_report = xTaskGetTickCount();
    
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        
        /* Send periodic reports */
        if ((now - last_report) >= pdMS_TO_TICKS(report_interval_ms)) {
            last_report = now;
            
            /* TODO: Build and send meter report packet */
            /* TODO: Include flow rate, total volume, alarms */
            
            SEGGER_RTT_printf(0, "LoRa: Sending report (flow=%.1f L/min, total=%.1f L)\n",
                              m_flow_state.flow_rate_lpm, m_flow_state.total_volume_l);
            
            /* TODO: Implement actual LoRa TX with retry */
            bool tx_success = false;  /* Placeholder - will be set by actual TX code */
            
            if (!tx_success) {
                /* Log meter reading to flash for later sync */
                uint32_t flow_mlpm = (uint32_t)(m_flow_state.flow_rate_lpm * 1000);
                uint32_t total_ml = (uint32_t)(m_flow_state.total_volume_l * 1000);
                uint8_t alarm_flags = m_flow_state.reverse_flow ? 0x01 : 0x00;
                
                if (agsys_device_log_meter(&m_device_ctx, flow_mlpm, total_ml, alarm_flags)) {
                    SEGGER_RTT_printf(0, "LoRa: Reading logged to flash (%lu pending)\n",
                                      agsys_device_log_pending_count(&m_device_ctx));
                }
            } else {
                /* Check for pending logs to sync */
                uint32_t pending = agsys_device_log_pending_count(&m_device_ctx);
                if (pending > 0) {
                    SEGGER_RTT_printf(0, "LoRa: %lu pending logs to sync\n", pending);
                    /* TODO: Send pending logs to property controller */
                }
            }
        }
        
        /* TODO: Check for incoming commands */
        /* TODO: Handle CONFIG_UPDATE, METER_RESET_TOTAL, etc. */
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ==========================================================================
 * BUTTON TASK - Debounce and event detection
 * ========================================================================== */

static void button_task(void *pvParameters)
{
    (void)pvParameters;
    
    SEGGER_RTT_printf(0, "Button task started\n");
    
    /* Configure button pins */
    nrf_gpio_cfg_input(AGSYS_BTN_UP_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_BTN_DOWN_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_BTN_LEFT_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_BTN_RIGHT_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(AGSYS_BTN_SELECT_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* Button state tracking */
    typedef struct {
        uint8_t pin;
        button_event_t short_event;
        button_event_t long_event;
        bool pressed;
        uint32_t press_start;
    } button_t;
    
    button_t buttons[] = {
        {AGSYS_BTN_UP_PIN, BTN_UP_SHORT, BTN_UP_LONG, false, 0},
        {AGSYS_BTN_DOWN_PIN, BTN_DOWN_SHORT, BTN_DOWN_LONG, false, 0},
        {AGSYS_BTN_LEFT_PIN, BTN_LEFT_SHORT, BTN_LEFT_LONG, false, 0},
        {AGSYS_BTN_RIGHT_PIN, BTN_RIGHT_SHORT, BTN_RIGHT_LONG, false, 0},
        {AGSYS_BTN_SELECT_PIN, BTN_SELECT_SHORT, BTN_SELECT_LONG, false, 0},
    };
    const int num_buttons = sizeof(buttons) / sizeof(buttons[0]);
    
    for (;;) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        for (int i = 0; i < num_buttons; i++) {
            bool is_pressed = (nrf_gpio_pin_read(buttons[i].pin) == 0);
            
            if (is_pressed && !buttons[i].pressed) {
                /* Button just pressed */
                buttons[i].pressed = true;
                buttons[i].press_start = now;
            } else if (!is_pressed && buttons[i].pressed) {
                /* Button released */
                buttons[i].pressed = false;
                uint32_t duration = now - buttons[i].press_start;
                
                button_event_t event;
                if (duration >= AGSYS_BTN_LONG_PRESS_MS) {
                    event = buttons[i].long_event;
                } else if (duration >= AGSYS_BTN_DEBOUNCE_MS) {
                    event = buttons[i].short_event;
                } else {
                    continue;  /* Too short, ignore */
                }
                
                xQueueSend(m_button_queue, &event, 0);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  /* 100 Hz polling */
    }
}


/* ==========================================================================
 * FREERTOS HOOKS
 * ========================================================================== */

/* Static memory for idle task (required when configSUPPORT_STATIC_ALLOCATION=1) */
static StaticTask_t m_idle_task_tcb;
static StackType_t m_idle_task_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &m_idle_task_tcb;
    *ppxIdleTaskStackBuffer = m_idle_task_stack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* Static memory for timer task */
static StaticTask_t m_timer_task_tcb;
static StackType_t m_timer_task_stack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &m_timer_task_tcb;
    *ppxTimerTaskStackBuffer = m_timer_task_stack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationMallocFailedHook(void)
{
    SEGGER_RTT_printf(0, "FATAL: Malloc failed!\n");
    for (;;) { __WFE(); }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    SEGGER_RTT_printf(0, "FATAL: Stack overflow in %s!\n", pcTaskName);
    for (;;) { __WFE(); }
}

void vApplicationIdleHook(void)
{
    /* Enter low power mode when idle */
    __WFE();
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
    
    SEGGER_RTT_printf(0, "\n\n=== Water Meter (Mag Meter) FreeRTOS ===\n");
    SEGGER_RTT_printf(0, "nRF52840 + S140 SoftDevice\n\n");
    
    /* Check for pairing button held at boot (SELECT button) */
    bool start_pairing = check_pairing_button();
    if (start_pairing) {
        SEGGER_RTT_printf(0, "SELECT button held - will enter pairing mode (timeout: %d sec)\n",
                          BLE_PAIRING_TIMEOUT_MS / 1000);
    }
    
    softdevice_init();
    
    if (!create_shared_resources()) {
        SEGGER_RTT_printf(0, "Failed to create shared resources!\n");
        for (;;) { __WFE(); }
    }
    
    /* Start pairing mode if button was held */
    if (start_pairing) {
        enter_pairing_mode();
    }
    
    /* Create tasks */
    xTaskCreate(adc_task, "ADC", AGSYS_TASK_STACK_ADC,
                NULL, AGSYS_TASK_PRIORITY_REALTIME, &m_adc_task_handle);
    
    xTaskCreate(display_task, "Display", AGSYS_TASK_STACK_DISPLAY,
                NULL, AGSYS_TASK_PRIORITY_NORMAL, &m_display_task_handle);
    
    xTaskCreate(lora_task, "LoRa", AGSYS_TASK_STACK_LORA,
                NULL, AGSYS_TASK_PRIORITY_HIGH, &m_lora_task_handle);
    
    xTaskCreate(button_task, "Button", AGSYS_TASK_STACK_BUTTON,
                NULL, AGSYS_TASK_PRIORITY_HIGH, &m_button_task_handle);
    
    SEGGER_RTT_printf(0, "Starting FreeRTOS scheduler...\n");
    
    /* Start scheduler (does not return) */
    vTaskStartScheduler();
    
    /* Should never reach here */
    for (;;) { __WFE(); }
}

