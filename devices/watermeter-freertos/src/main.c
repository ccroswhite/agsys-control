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
#include "agsys_ota.h"
#include "agsys_ble_ota.h"
#include "agsys_flash.h"
#include "agsys_flash_backup.h"
#include "board_config.h"
#include "lora_task.h"
#include "display.h"

/* ==========================================================================
 * SHARED RESOURCES
 * ========================================================================== */

/* SPI bus mutex - shared by ADC, Display, LoRa, FRAM */
SemaphoreHandle_t g_spi_mutex = NULL;

/* Device context (BLE, FRAM, Flash, auth) - non-static for logging access */
agsys_device_ctx_t m_device_ctx;

/* OTA contexts */
static agsys_flash_ctx_t m_flash_ctx;
static agsys_backup_ctx_t m_backup_ctx;
static agsys_ota_ctx_t m_ota_ctx;
static agsys_ble_ota_t m_ble_ota_ctx;
static bool m_ota_in_progress = false;
static char m_ota_version_str[16] = {0};

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

/* Global flow data for LoRa task access */
volatile float g_flow_rate_lpm = 0.0f;
volatile float g_total_volume_l = 0.0f;
volatile uint8_t g_alarm_flags = 0;

/* ==========================================================================
 * ALARM STATE (types defined in ui_types.h)
 * ========================================================================== */

typedef struct {
    AlarmType_t type;
    uint32_t start_time_sec;
    float flow_rate_lpm;
    float volume_l;
    bool acknowledged;
} alarm_state_t;

static alarm_state_t m_alarm_state = {0};

/* ==========================================================================
 * DISPLAY STATE (types defined in ui_types.h)
 * ========================================================================== */

static DisplayPowerState_t m_display_power = DISPLAY_ACTIVE;
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
static TaskHandle_t m_button_task_handle = NULL;

/* ==========================================================================
 * BUTTON EVENT QUEUE (types defined in ui_types.h)
 * ========================================================================== */

static QueueHandle_t m_button_queue = NULL;

/* ==========================================================================
 * FORWARD DECLARATIONS
 * ========================================================================== */

static void adc_task(void *pvParameters);
static void display_task(void *pvParameters);
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
    
    /* Update display icon */
    display_updateBleStatus(BLE_UI_STATE_ADVERTISING);
}

static void exit_pairing_mode(void)
{
    m_pairing_mode = false;
    SEGGER_RTT_printf(0, "Exiting pairing mode\n");
    
    /* Stop BLE advertising */
    agsys_device_stop_advertising(&m_device_ctx);
    
    /* Update display icon */
    display_updateBleStatus(BLE_UI_STATE_IDLE);
}

/* ==========================================================================
 * BLE EVENT HANDLER
 * ========================================================================== */

static void ble_event_handler(const agsys_ble_evt_t *evt)
{
    if (evt == NULL) return;
    
    switch (evt->type) {
        case AGSYS_BLE_EVT_CONNECTED:
            SEGGER_RTT_printf(0, "BLE: Connected\n");
            display_updateBleStatus(BLE_UI_STATE_CONNECTED);
            break;
            
        case AGSYS_BLE_EVT_DISCONNECTED:
            SEGGER_RTT_printf(0, "BLE: Disconnected\n");
            display_updateBleStatus(BLE_UI_STATE_DISCONNECTED);
            /* Return to idle after brief flash (handled by display tick) */
            break;
            
        case AGSYS_BLE_EVT_AUTHENTICATED:
            SEGGER_RTT_printf(0, "BLE: Authenticated\n");
            display_updateBleStatus(BLE_UI_STATE_AUTHENTICATED);
            break;
            
        case AGSYS_BLE_EVT_AUTH_FAILED:
            SEGGER_RTT_printf(0, "BLE: Auth failed\n");
            /* Stay in connected state, icon keeps flashing */
            break;
            
        case AGSYS_BLE_EVT_AUTH_TIMEOUT:
            SEGGER_RTT_printf(0, "BLE: Auth timeout\n");
            /* Connection will be dropped, disconnected event will follow */
            break;
            
        case AGSYS_BLE_EVT_CONFIG_CHANGED:
            SEGGER_RTT_printf(0, "BLE: Config changed\n");
            /* TODO: Handle config update from app */
            break;
            
        case AGSYS_BLE_EVT_COMMAND_RECEIVED:
            SEGGER_RTT_printf(0, "BLE: Command received (cmd=%d)\n", evt->command.cmd_id);
            /* TODO: Handle commands from app */
            break;
            
        default:
            break;
    }
}

/* ==========================================================================
 * OTA CALLBACKS AND HELPERS
 * ========================================================================== */

static const char *ota_status_to_string(agsys_ota_status_t status)
{
    switch (status) {
        case AGSYS_OTA_STATUS_IDLE:              return "Idle";
        case AGSYS_OTA_STATUS_BACKUP_IN_PROGRESS: return "Backing up...";
        case AGSYS_OTA_STATUS_RECEIVING:         return "Receiving...";
        case AGSYS_OTA_STATUS_VERIFYING:         return "Verifying...";
        case AGSYS_OTA_STATUS_APPLYING:          return "Applying...";
        case AGSYS_OTA_STATUS_PENDING_REBOOT:    return "Complete!";
        case AGSYS_OTA_STATUS_PENDING_CONFIRM:   return "Confirming...";
        case AGSYS_OTA_STATUS_ERROR:             return "Error";
        default:                                 return "Unknown";
    }
}

static const char *ota_error_to_string(agsys_ota_error_t error)
{
    switch (error) {
        case AGSYS_OTA_ERR_NONE:                return "No error";
        case AGSYS_OTA_ERR_ALREADY_IN_PROGRESS: return "Update already in progress";
        case AGSYS_OTA_ERR_BACKUP_FAILED:       return "Backup failed";
        case AGSYS_OTA_ERR_FLASH_ERASE:         return "Flash erase failed";
        case AGSYS_OTA_ERR_FLASH_WRITE:         return "Flash write failed";
        case AGSYS_OTA_ERR_INVALID_CHUNK:       return "Invalid data chunk";
        case AGSYS_OTA_ERR_CRC_MISMATCH:        return "CRC verification failed";
        case AGSYS_OTA_ERR_SIZE_MISMATCH:       return "Size mismatch";
        case AGSYS_OTA_ERR_SIGNATURE_INVALID:   return "Invalid signature";
        case AGSYS_OTA_ERR_INTERNAL_FLASH:      return "Internal flash error";
        case AGSYS_OTA_ERR_NOT_STARTED:         return "OTA not started";
        case AGSYS_OTA_ERR_TIMEOUT:             return "Timeout";
        default:                                return "Unknown error";
    }
}

static void ota_progress_callback(agsys_ota_status_t status, uint8_t progress, void *user_data)
{
    (void)user_data;
    
    SEGGER_RTT_printf(0, "OTA: %s (%d%%)\n", ota_status_to_string(status), progress);
    
    /* Update display */
    if (!m_ota_in_progress && status != AGSYS_OTA_STATUS_IDLE) {
        m_ota_in_progress = true;
        /* Get version from OTA context */
        snprintf(m_ota_version_str, sizeof(m_ota_version_str), "%d.%d.%d",
                 m_ota_ctx.expected_version[0],
                 m_ota_ctx.expected_version[1],
                 m_ota_ctx.expected_version[2]);
        display_showOTAProgress(progress, ota_status_to_string(status), m_ota_version_str);
    } else if (m_ota_in_progress) {
        display_updateOTAProgress(progress);
        display_updateOTAStatus(ota_status_to_string(status));
    }
}

static void ota_complete_callback(bool success, agsys_ota_error_t error, void *user_data)
{
    (void)user_data;
    
    if (success) {
        SEGGER_RTT_printf(0, "OTA: Complete, rebooting...\n");
        display_updateOTAStatus("Rebooting...");
        /* Reboot is handled by OTA module after ACK sent */
    } else {
        SEGGER_RTT_printf(0, "OTA: Failed - %s\n", ota_error_to_string(error));
        m_ota_in_progress = false;
        display_showOTAError(ota_error_to_string(error));
    }
}

static bool init_ota(void)
{
    /* Initialize external flash */
    if (!agsys_flash_init(&m_flash_ctx, SPI_CS_FLASH_PIN)) {
        SEGGER_RTT_printf(0, "OTA: Flash init failed\n");
        return false;
    }
    
    /* Initialize backup system */
    if (!agsys_backup_init(&m_backup_ctx, &m_flash_ctx)) {
        SEGGER_RTT_printf(0, "OTA: Backup init failed\n");
        return false;
    }
    
    /* Check for rollback from previous failed update */
    if (agsys_backup_check_rollback(&m_backup_ctx)) {
        SEGGER_RTT_printf(0, "OTA: Rollback occurred from failed update\n");
        uint8_t major, minor, patch;
        if (agsys_backup_get_failed_version(&m_backup_ctx, &major, &minor, &patch)) {
            SEGGER_RTT_printf(0, "OTA: Failed version was v%d.%d.%d\n", major, minor, patch);
        }
    }
    
    /* Initialize OTA module */
    if (!agsys_ota_init(&m_ota_ctx, &m_flash_ctx, &m_backup_ctx)) {
        SEGGER_RTT_printf(0, "OTA: OTA init failed\n");
        return false;
    }
    
    /* Set callbacks */
    agsys_ota_set_progress_callback(&m_ota_ctx, ota_progress_callback, NULL);
    agsys_ota_set_complete_callback(&m_ota_ctx, ota_complete_callback, NULL);
    
    /* Register tasks to suspend during OTA apply phase */
    agsys_ota_register_task(m_adc_task_handle);
    agsys_ota_register_task(m_display_task_handle);
    agsys_ota_register_task(m_button_task_handle);
    
    /* LoRa OTA: Messages are handled via lora_task calling ota_handle_lora_message() 
     * See ota_handle_lora_message() below for the handler function */
    SEGGER_RTT_printf(0, "OTA: LoRa OTA enabled (via lora_task)\n");
    
    /* Initialize BLE OTA service */
    uint32_t err_code = agsys_ble_ota_init(&m_ble_ota_ctx, &m_ota_ctx);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "OTA: BLE OTA init failed (err=%lu)\n", err_code);
        /* Continue - LoRa OTA can still work */
    } else {
        SEGGER_RTT_printf(0, "OTA: BLE OTA enabled\n");
    }
    
    /* Confirm firmware if pending from previous OTA */
    if (agsys_ota_is_confirm_pending(&m_ota_ctx)) {
        SEGGER_RTT_printf(0, "OTA: Confirming firmware after successful boot\n");
        agsys_ota_confirm(&m_ota_ctx);
    }
    
    SEGGER_RTT_printf(0, "OTA: Initialized\n");
    return true;
}

/* ==========================================================================
 * LORA OTA MESSAGE HANDLER (called from lora_task)
 * ========================================================================== */

/**
 * @brief Handle incoming LoRa OTA message
 * 
 * Called by lora_task when an OTA message (0x40-0x45) is received.
 * Returns response data to send back to controller.
 * 
 * @param msg_type Message type (0x40-0x45)
 * @param data Message payload
 * @param len Payload length
 * @param response Output buffer for response (at least 4 bytes)
 * @param response_len Output: response length
 * @return true if response should be sent
 */
bool ota_handle_lora_message(uint8_t msg_type, const uint8_t *data, size_t len,
                              uint8_t *response, size_t *response_len)
{
    if (data == NULL || response == NULL || response_len == NULL) {
        return false;
    }
    
    *response_len = 0;
    
    switch (msg_type) {
        case 0x40: {  /* OTA_START */
            if (len < 12) {
                SEGGER_RTT_printf(0, "OTA: Invalid START message\n");
                response[0] = 0x80;  /* ACK_ERROR */
                response[1] = 0;
                *response_len = 2;
                return true;
            }
            
            uint32_t fw_size = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
            uint32_t fw_crc = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
            uint8_t major = data[8];
            uint8_t minor = data[9];
            uint8_t patch = data[10];
            
            SEGGER_RTT_printf(0, "OTA: LoRa START - size=%lu, v%d.%d.%d\n", 
                              fw_size, major, minor, patch);
            
            agsys_ota_error_t err = agsys_ota_start(&m_ota_ctx, fw_size, fw_crc, 
                                                     major, minor, patch);
            if (err == AGSYS_OTA_ERR_NONE) {
                response[0] = 0x01;  /* ACK_READY */
                response[1] = 0;
                *response_len = 2;
            } else {
                response[0] = 0x80;  /* ACK_ERROR */
                response[1] = (uint8_t)err;
                *response_len = 2;
            }
            return true;
        }
        
        case 0x41: {  /* OTA_CHUNK */
            if (len < 4) {
                return false;
            }
            
            uint16_t chunk_idx = data[0] | (data[1] << 8);
            /* offset_check at data[2-3] can be used for verification if needed */
            const uint8_t *chunk_data = &data[4];
            size_t chunk_len = len - 4;
            
            /* Calculate actual offset from chunk index */
            uint32_t offset = (uint32_t)chunk_idx * 200;  /* 200 byte chunks for LoRa */
            
            agsys_ota_error_t err = agsys_ota_write_chunk(&m_ota_ctx, offset, 
                                                          chunk_data, chunk_len);
            
            response[0] = (err == AGSYS_OTA_ERR_NONE) ? 0x02 : 0x80;  /* ACK_CHUNK_OK or ERROR */
            response[1] = agsys_ota_get_progress(&m_ota_ctx);
            response[2] = chunk_idx & 0xFF;
            response[3] = (chunk_idx >> 8) & 0xFF;
            *response_len = 4;
            return true;
        }
        
        case 0x42: {  /* OTA_FINISH */
            SEGGER_RTT_printf(0, "OTA: LoRa FINISH\n");
            
            agsys_ota_error_t err = agsys_ota_finish(&m_ota_ctx);
            if (err == AGSYS_OTA_ERR_NONE) {
                response[0] = 0x04;  /* ACK_REBOOTING */
                response[1] = 100;
                *response_len = 2;
                /* Reboot will happen after ACK is sent (handled by complete callback) */
            } else {
                response[0] = 0x80;  /* ACK_ERROR */
                response[1] = (uint8_t)err;
                *response_len = 2;
            }
            return true;
        }
        
        case 0x43: {  /* OTA_ABORT */
            SEGGER_RTT_printf(0, "OTA: LoRa ABORT\n");
            agsys_ota_abort(&m_ota_ctx);
            m_ota_in_progress = false;
            display_showMain();
            
            response[0] = 0x00;  /* ACK_OK */
            *response_len = 1;
            return true;
        }
        
        default:
            return false;
    }
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
        .evt_handler = ble_event_handler
    };
    if (!agsys_device_init(&m_device_ctx, &dev_init)) {
        SEGGER_RTT_printf(0, "WARNING: Device init failed\n");
    }
    
    /* Create button event queue */
    m_button_queue = xQueueCreate(10, sizeof(ButtonEvent_t));
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
        
        /* Update global flow data for LoRa task */
        g_flow_rate_lpm = m_flow_state.flow_rate_lpm;
        g_total_volume_l = m_flow_state.total_volume_l;
        g_alarm_flags = m_flow_state.reverse_flow ? 0x01 : 0x00;
        
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
        ButtonEvent_t btn_event;
        while (xQueueReceive(m_button_queue, &btn_event, 0) == pdTRUE) {
            /* Reset activity timer on any button press */
            m_last_activity_tick = xTaskGetTickCount();
            m_display_power = DISPLAY_ACTIVE;
            
            /* TODO: Handle button event in current screen */
            SEGGER_RTT_printf(0, "Button event: %d\n", btn_event);
        }
        
        /* Update display power state */
        TickType_t idle_time = xTaskGetTickCount() - m_last_activity_tick;
        if (m_alarm_state.type == ALARM_CLEARED) {
            if (idle_time > pdMS_TO_TICKS(AGSYS_DISPLAY_DIM_TIMEOUT_SEC * 1000 + 
                                          AGSYS_DISPLAY_SLEEP_TIMEOUT_SEC * 1000)) {
                m_display_power = DISPLAY_SLEEP;
            } else if (idle_time > pdMS_TO_TICKS(AGSYS_DISPLAY_DIM_TIMEOUT_SEC * 1000)) {
                m_display_power = DISPLAY_DIM;
            }
        }
        
        /* TODO: Call lv_timer_handler() for LVGL */
        /* TODO: Update main screen with flow data */
        
        /* Update BLE icon flash animation */
        display_tickBleIcon();
        
        /* Check OTA error screen timeout (60s auto-dismiss) */
        display_tickOTAError();
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20));  /* 50 Hz refresh */
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
        ButtonEvent_t short_event;
        ButtonEvent_t long_event;
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
                
                ButtonEvent_t event;
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
    
    /* LoRa task is started via lora_task module */
    lora_task_init();
    lora_task_start();
    
    xTaskCreate(button_task, "Button", AGSYS_TASK_STACK_BUTTON,
                NULL, AGSYS_TASK_PRIORITY_HIGH, &m_button_task_handle);
    
    /* Initialize OTA (LoRa + BLE) after tasks are created */
    if (!init_ota()) {
        SEGGER_RTT_printf(0, "WARNING: OTA init failed, updates disabled\n");
    }
    
    SEGGER_RTT_printf(0, "Starting FreeRTOS scheduler...\n");
    
    /* Start scheduler (does not return) */
    vTaskStartScheduler();
    
    /* Should never reach here */
    for (;;) { __WFE(); }
}

