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
#include "nrf_drv_saadc.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"

#include "SEGGER_RTT.h"

#include <math.h>

#include "agsys_config.h"
#include "agsys_spi.h"
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
#include "lvgl/lvgl.h"
#include "flow_calc.h"
#include "coil_driver.h"
#include "temp_sensor.h"
#include "lvgl_port.h"

/* ==========================================================================
 * SHARED RESOURCES
 * ========================================================================== */

/* SPI bus mutex - shared by ADC, Display, LoRa, FRAM */
SemaphoreHandle_t g_spi_mutex = NULL;

/* Device context (BLE, FRAM, Flash, auth) - non-static for logging access */
agsys_device_ctx_t m_device_ctx;

/* Global FRAM context pointer for flow_calc module */
agsys_fram_ctx_t *g_fram_ctx = NULL;

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

static ads131m0x_ctx_t m_adc_ctx;
static flow_calc_ctx_t m_flow_ctx;
coil_driver_ctx_t m_coil_ctx;  /* Non-static for flow_calc access */
temp_sensor_ctx_t g_temp_sensor;  /* Non-static for LoRa task access */

/* Global flow data for LoRa task access (full float precision) */
volatile float g_flow_rate_lpm = 0.0f;
volatile float g_total_volume_l = 0.0f;
volatile float g_signal_uv = 0.0f;
volatile float g_temperature_c = 25.0f;
volatile uint8_t g_signal_quality = 0;
volatile uint8_t g_alarm_flags = 0;

/* Calibration state - set true if device needs calibration */
volatile bool g_needs_calibration = false;

/* Global pointer to calibration data for LoRa task access */
flow_calibration_t *g_calibration_ptr = NULL;

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

/**
 * @brief Read TIER_ID voltage from ADC
 * 
 * P1.01 = AIN7 on nRF52840
 * Voltage divider on power board sets:
 *   MM-S: 0.825V (R4=1M, R5=3M)
 *   MM-M: 1.65V  (R4=1M, R5=1M)
 *   MM-L: 2.475V (R4=1M, R5=0.5M)
 * 
 * @return Voltage in millivolts
 */
static uint32_t read_tier_id_adc(void)
{
    /* P1.01 = AIN7 on nRF52840 */
    #define TIER_ID_AIN     NRF_SAADC_INPUT_AIN7
    
    /* Initialize SAADC if not already done */
    ret_code_t err = nrf_drv_saadc_init(NULL, NULL);
    if (err != NRF_SUCCESS && err != NRF_ERROR_INVALID_STATE) {
        SEGGER_RTT_printf(0, "TIER: SAADC init failed (err=%d)\n", err);
        return 0;
    }
    
    /* Configure channel for TIER_ID
     * Using VDD/4 reference with 1/4 gain gives full VDD range
     * Resolution: VDD / 4096 per LSB (12-bit)
     */
    nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(TIER_ID_AIN);
    channel_config.gain = NRF_SAADC_GAIN1_4;
    channel_config.reference = NRF_SAADC_REFERENCE_VDD4;
    channel_config.acq_time = NRF_SAADC_ACQTIME_40US;  /* Long acquisition for high-impedance divider */
    
    /* Use channel 1 (channel 0 may be used by temp sensor) */
    err = nrf_drv_saadc_channel_init(1, &channel_config);
    if (err != NRF_SUCCESS && err != NRF_ERROR_INVALID_STATE) {
        SEGGER_RTT_printf(0, "TIER: Channel init failed (err=%d)\n", err);
        return 0;
    }
    
    /* Take multiple samples and average */
    int32_t sum = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < 8; i++) {
        nrf_saadc_value_t sample;
        err = nrf_drv_saadc_sample_convert(1, &sample);
        if (err == NRF_SUCCESS && sample > 0) {
            sum += sample;
            valid_samples++;
        }
        nrf_delay_us(100);
    }
    
    /* Uninit channel to free it */
    nrf_drv_saadc_channel_uninit(1);
    
    if (valid_samples == 0) {
        SEGGER_RTT_printf(0, "TIER: No valid samples\n");
        return 0;
    }
    
    int32_t avg_raw = sum / valid_samples;
    
    /* Convert to millivolts
     * With VDD/4 reference and 1/4 gain:
     * V_in = (raw / 4096) * VDD
     * Assuming VDD = 3.3V = 3300mV
     */
    uint32_t voltage_mv = (uint32_t)((avg_raw * 3300) / 4096);
    
    SEGGER_RTT_printf(0, "TIER: ADC raw=%ld, voltage=%lu mV\n", avg_raw, voltage_mv);
    
    return voltage_mv;
}

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
 * DISPLAY CALIBRATION CALLBACKS
 * ========================================================================== */

bool display_cal_zero_callback(void)
{
    SEGGER_RTT_printf(0, "UI: Zero calibration requested\n");
    if (flow_calc_zero_calibrate(&m_flow_ctx)) {
        flow_calc_save_calibration(&m_flow_ctx);
        SEGGER_RTT_printf(0, "UI: Zero cal success\n");
        return true;
    }
    SEGGER_RTT_printf(0, "UI: Zero cal failed\n");
    return false;
}

bool display_cal_span_callback(float known_flow_lpm)
{
    SEGGER_RTT_printf(0, "UI: Span calibration requested (ref=%.1f L/min)\n", known_flow_lpm);
    if (flow_calc_span_calibrate(&m_flow_ctx, known_flow_lpm)) {
        flow_calc_save_calibration(&m_flow_ctx);
        SEGGER_RTT_printf(0, "UI: Span cal success\n");
        return true;
    }
    SEGGER_RTT_printf(0, "UI: Span cal failed\n");
    return false;
}

void display_cal_pipe_size_callback(uint8_t pipe_size)
{
    SEGGER_RTT_printf(0, "UI: Pipe size set to %d\n", pipe_size);
    flow_calc_set_defaults(&m_flow_ctx, (flow_pipe_size_t)pipe_size);
    flow_calc_save_calibration(&m_flow_ctx);
}

void display_cal_get_data(float *zero_uv, float *span, float *diameter_m, uint8_t *pipe_size)
{
    if (zero_uv) *zero_uv = m_flow_ctx.calibration.zero_offset_uv;
    if (span) *span = m_flow_ctx.calibration.span_uv_per_mps;
    if (diameter_m) *diameter_m = m_flow_ctx.calibration.pipe_diameter_m;
    if (pipe_size) *pipe_size = m_flow_ctx.calibration.pipe_size;
}

void display_cal_get_duty_cycle(uint16_t *on_ms, uint16_t *off_ms)
{
    if (on_ms) *on_ms = m_flow_ctx.calibration.coil_on_time_ms;
    if (off_ms) *off_ms = m_flow_ctx.calibration.coil_off_time_ms;
}

void display_cal_set_duty_cycle(uint16_t on_ms, uint16_t off_ms)
{
    SEGGER_RTT_printf(0, "UI: Duty cycle set to %ums/%ums\n", on_ms, off_ms);
    
    /* Apply to coil driver */
    coil_driver_set_duty_cycle(&m_coil_ctx, on_ms, off_ms);
    
    /* Save to calibration (use clamped values from driver) */
    m_flow_ctx.calibration.coil_on_time_ms = m_coil_ctx.on_time_ms;
    m_flow_ctx.calibration.coil_off_time_ms = m_coil_ctx.off_time_ms;
    flow_calc_save_calibration(&m_flow_ctx);
}

/* ==========================================================================
 * BLE COMMAND IDS (Water Meter specific)
 * ========================================================================== */

#define BLE_CMD_ZERO_CAL            0x10  /* Trigger zero calibration */
#define BLE_CMD_SPAN_CAL            0x11  /* Span cal: params = float32 known_flow_lpm */
#define BLE_CMD_SET_PIPE_SIZE       0x12  /* Set pipe size: params = uint8 pipe_size_enum */
#define BLE_CMD_RESET_TOTAL         0x13  /* Reset totalizer */
#define BLE_CMD_GET_CAL_DATA        0x14  /* Request calibration data */
#define BLE_CMD_SAVE_CAL            0x15  /* Save calibration to FRAM */
#define BLE_CMD_AUTO_ZERO_ENABLE    0x16  /* Enable/disable auto-zero: params = uint8 enable */
#define BLE_CMD_SET_DUTY_CYCLE      0x17  /* Set duty cycle: params = uint16 on_ms, uint16 off_ms */
#define BLE_CMD_GET_DUTY_CYCLE      0x18  /* Get current duty cycle */

/* BLE response codes */
#define BLE_RSP_OK                  0x00
#define BLE_RSP_ERR_NOT_READY       0x01
#define BLE_RSP_ERR_INVALID_PARAM   0x02
#define BLE_RSP_ERR_CAL_FAILED      0x03
#define BLE_RSP_ERR_NOT_AUTH        0x04

/* ==========================================================================
 * BLE COMMAND HANDLER
 * ========================================================================== */

static void handle_ble_command(uint8_t cmd_id, const uint8_t *params, uint16_t params_len)
{
    uint8_t response[32];
    uint16_t rsp_len = 2;  /* cmd_id + status */
    response[0] = cmd_id;
    response[1] = BLE_RSP_OK;
    
    switch (cmd_id) {
        case BLE_CMD_ZERO_CAL:
            SEGGER_RTT_printf(0, "BLE: Zero calibration requested\n");
            if (flow_calc_zero_calibrate(&m_flow_ctx)) {
                flow_calc_save_calibration(&m_flow_ctx);
                SEGGER_RTT_printf(0, "BLE: Zero cal success\n");
            } else {
                response[1] = BLE_RSP_ERR_CAL_FAILED;
                SEGGER_RTT_printf(0, "BLE: Zero cal failed\n");
            }
            break;
            
        case BLE_CMD_SPAN_CAL:
            if (params_len >= 4) {
                float known_flow_lpm;
                memcpy(&known_flow_lpm, params, sizeof(float));
                SEGGER_RTT_printf(0, "BLE: Span cal requested (ref=%.1f L/min)\n", known_flow_lpm);
                
                if (flow_calc_span_calibrate(&m_flow_ctx, known_flow_lpm)) {
                    flow_calc_save_calibration(&m_flow_ctx);
                    SEGGER_RTT_printf(0, "BLE: Span cal success\n");
                } else {
                    response[1] = BLE_RSP_ERR_CAL_FAILED;
                    SEGGER_RTT_printf(0, "BLE: Span cal failed\n");
                }
            } else {
                response[1] = BLE_RSP_ERR_INVALID_PARAM;
            }
            break;
            
        case BLE_CMD_SET_PIPE_SIZE:
            if (params_len >= 1 && params[0] < PIPE_SIZE_COUNT) {
                flow_pipe_size_t pipe_size = (flow_pipe_size_t)params[0];
                flow_calc_set_defaults(&m_flow_ctx, pipe_size);
                flow_calc_save_calibration(&m_flow_ctx);
                SEGGER_RTT_printf(0, "BLE: Pipe size set to %d\n", pipe_size);
            } else {
                response[1] = BLE_RSP_ERR_INVALID_PARAM;
            }
            break;
            
        case BLE_CMD_RESET_TOTAL:
            flow_calc_reset_total(&m_flow_ctx);
            SEGGER_RTT_printf(0, "BLE: Totalizer reset\n");
            break;
            
        case BLE_CMD_GET_CAL_DATA:
            /* Return calibration data in response */
            response[2] = m_flow_ctx.calibration.pipe_size;
            memcpy(&response[3], &m_flow_ctx.calibration.zero_offset_uv, sizeof(float));
            memcpy(&response[7], &m_flow_ctx.calibration.span_uv_per_mps, sizeof(float));
            memcpy(&response[11], &m_flow_ctx.calibration.pipe_diameter_m, sizeof(float));
            rsp_len = 15;
            SEGGER_RTT_printf(0, "BLE: Cal data requested\n");
            break;
            
        case BLE_CMD_SAVE_CAL:
            if (flow_calc_save_calibration(&m_flow_ctx)) {
                SEGGER_RTT_printf(0, "BLE: Cal saved\n");
            } else {
                response[1] = BLE_RSP_ERR_CAL_FAILED;
            }
            break;
            
        case BLE_CMD_AUTO_ZERO_ENABLE:
            if (params_len >= 1) {
                flow_calc_set_auto_zero(&m_flow_ctx, params[0] != 0);
                m_flow_ctx.calibration.auto_zero_enabled = params[0] != 0;
                flow_calc_save_calibration(&m_flow_ctx);
            } else {
                response[1] = BLE_RSP_ERR_INVALID_PARAM;
            }
            break;
            
        case BLE_CMD_SET_DUTY_CYCLE:
            if (params_len >= 4) {
                uint16_t on_ms, off_ms;
                memcpy(&on_ms, params, sizeof(uint16_t));
                memcpy(&off_ms, params + 2, sizeof(uint16_t));
                
                /* Apply to coil driver */
                coil_driver_set_duty_cycle(&m_coil_ctx, on_ms, off_ms);
                
                /* Save to calibration */
                m_flow_ctx.calibration.coil_on_time_ms = m_coil_ctx.on_time_ms;
                m_flow_ctx.calibration.coil_off_time_ms = m_coil_ctx.off_time_ms;
                flow_calc_save_calibration(&m_flow_ctx);
                
                SEGGER_RTT_printf(0, "BLE: Duty cycle set to %ums/%ums\n", on_ms, off_ms);
            } else {
                response[1] = BLE_RSP_ERR_INVALID_PARAM;
            }
            break;
            
        case BLE_CMD_GET_DUTY_CYCLE:
            /* Return duty cycle in response */
            memcpy(&response[2], &m_flow_ctx.calibration.coil_on_time_ms, sizeof(uint16_t));
            memcpy(&response[4], &m_flow_ctx.calibration.coil_off_time_ms, sizeof(uint16_t));
            response[6] = m_flow_ctx.calibration.auto_zero_enabled;
            rsp_len = 7;
            SEGGER_RTT_printf(0, "BLE: Duty cycle requested\n");
            break;
            
        default:
            response[1] = BLE_RSP_ERR_INVALID_PARAM;
            SEGGER_RTT_printf(0, "BLE: Unknown command %d\n", cmd_id);
            break;
    }
    
    /* Send response */
    agsys_ble_send_response(&m_device_ctx.ble_ctx, response, rsp_len);
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
            handle_ble_command(evt->command.cmd_id, evt->command.params, evt->command.params_len);
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
    } else {
        /* Set global FRAM pointer for flow_calc module */
        g_fram_ctx = &m_device_ctx.fram_ctx;
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
 * ADC DRDY CALLBACK - Called when new sample ready
 * ========================================================================== */

static void adc_drdy_callback(ads131m0x_sample_t *sample, void *user_data)
{
    (void)user_data;
    
    /* Get current coil state from hardware driver */
    bool coil_on = coil_driver_get_state(&m_coil_ctx);
    
    /* Process sample through flow calculator */
    flow_calc_process_sample(&m_flow_ctx, sample, coil_on);
}

/* ==========================================================================
 * ADC TASK - Signal acquisition and flow calculation
 * ========================================================================== */

static void adc_task(void *pvParameters)
{
    (void)pvParameters;
    
    SEGGER_RTT_printf(0, "ADC task started\n");
    
    /* Initialize SPI buses with DMA support
     * Water meter uses 4 SPI buses:
     * Bus 0 (SPIM0): ADC (ADS131M02)
     * Bus 1 (SPIM1): Display (ST7789)  
     * Bus 2 (SPIM2): LoRa (RFM95C)
     * Bus 3 (SPIM3): Memory (FRAM + Flash)
     */
    agsys_spi_bus_config_t adc_bus = {
        .sck_pin = SPI0_SCK_PIN,
        .mosi_pin = SPI0_MOSI_PIN,
        .miso_pin = SPI0_MISO_PIN,
        .spim_instance = 0,
    };
    if (agsys_spi_bus_init(AGSYS_SPI_BUS_0, &adc_bus) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "SPI: Bus 0 (ADC) init failed!\n");
        vTaskSuspend(NULL);
    }
    
    agsys_spi_bus_config_t display_bus = {
        .sck_pin = SPI1_SCK_PIN,
        .mosi_pin = SPI1_MOSI_PIN,
        .miso_pin = SPI1_MISO_PIN,
        .spim_instance = 1,
    };
    if (agsys_spi_bus_init(AGSYS_SPI_BUS_1, &display_bus) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "SPI: Bus 1 (Display) init failed!\n");
        vTaskSuspend(NULL);
    }
    
    agsys_spi_bus_config_t lora_bus = {
        .sck_pin = SPI2_SCK_PIN,
        .mosi_pin = SPI2_MOSI_PIN,
        .miso_pin = SPI2_MISO_PIN,
        .spim_instance = 2,
    };
    if (agsys_spi_bus_init(AGSYS_SPI_BUS_2, &lora_bus) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "SPI: Bus 2 (LoRa) init failed!\n");
        vTaskSuspend(NULL);
    }
    
    SEGGER_RTT_printf(0, "SPI: 3 buses initialized with DMA\n");
    
    /* Initialize ADS131M02 ADC using HAL wrapper */
    if (!ads131m0x_hal_init(&m_adc_ctx,
                            AGSYS_ADC_CS_PIN,
                            AGSYS_ADC_DRDY_PIN,
                            AGSYS_ADC_SYNC_PIN,
                            ADS131M0X_OSR_256,      /* 16 kSPS (OSR=256 with 8.192MHz clock) */
                            ADS131M0X_GAIN_32X,     /* Electrode signal */
                            ADS131M0X_GAIN_1X)) {   /* Coil current sense */
        SEGGER_RTT_printf(0, "ADC: Init failed!\n");
        vTaskSuspend(NULL);
    }
    
    /* Initialize flow calculator */
    if (!flow_calc_init(&m_flow_ctx, &m_adc_ctx)) {
        SEGGER_RTT_printf(0, "FLOW: Init failed!\n");
        vTaskSuspend(NULL);
    }
    
    /* =======================================================================
     * AUTO-DETECTION SEQUENCE
     * 
     * 1. Detect tier from TIER_ID pin (resistor divider on coil board)
     * 2. Load calibration from FRAM
     * 3. If no calibration: apply tier defaults, measure coil resistance
     * 4. If tier changed: update tier-specific parameters
     * ======================================================================= */
    
    /* Step 1: Detect tier from TIER_ID ADC pin */
    uint32_t tier_id_mv = read_tier_id_adc();
    flow_tier_t detected_tier = flow_calc_detect_tier(tier_id_mv);
    
    const char *tier_names[] = {"MM-S", "MM-M", "MM-L", "UNKNOWN"};
    SEGGER_RTT_printf(0, "TIER: Detected %s (voltage=%lu mV)\n", 
                      tier_names[detected_tier < 4 ? detected_tier : 3], tier_id_mv);
    
    /* Step 2: Try to load calibration from FRAM */
    bool cal_loaded = flow_calc_load_calibration(&m_flow_ctx);
    bool needs_calibration = false;
    
    /* Export calibration pointer for LoRa task */
    g_calibration_ptr = &m_flow_ctx.calibration;
    
    if (!cal_loaded) {
        /* No valid calibration - first boot or corrupted */
        SEGGER_RTT_printf(0, "BOOT: No calibration found - applying defaults\n");
        flow_calc_set_defaults(&m_flow_ctx, PIPE_SIZE_2_INCH);
        flow_calc_apply_tier_defaults(&m_flow_ctx, detected_tier);
        needs_calibration = true;
    } else {
        /* Check if tier changed (different coil board installed) */
        flow_tier_t stored_tier = (flow_tier_t)m_flow_ctx.calibration.tier;
        if (detected_tier != FLOW_TIER_UNKNOWN && detected_tier != stored_tier) {
            SEGGER_RTT_printf(0, "BOOT: Tier changed from %d to %d - updating parameters\n",
                              stored_tier, detected_tier);
            flow_calc_apply_tier_defaults(&m_flow_ctx, detected_tier);
            needs_calibration = true;
        }
        
        /* Check if device has ever been calibrated */
        if (!flow_calc_is_calibrated(&m_flow_ctx)) {
            SEGGER_RTT_printf(0, "BOOT: Device has defaults but never calibrated\n");
            needs_calibration = true;
        }
    }
    
    /* Initialize hardware coil driver (TIMER2 + PPI + GPIOTE) */
    if (!coil_driver_init(&m_coil_ctx, AGSYS_COIL_GATE_PIN)) {
        SEGGER_RTT_printf(0, "COIL: Init failed!\n");
        vTaskSuspend(NULL);
    }
    
    /* Step 3: If uncalibrated, measure coil resistance */
    if (needs_calibration) {
        SEGGER_RTT_printf(0, "BOOT: Measuring coil resistance...\n");
        uint16_t measured_r = flow_calc_measure_coil_resistance(&m_flow_ctx);
        if (measured_r > 0) {
            SEGGER_RTT_printf(0, "BOOT: Coil resistance = %u mΩ\n", measured_r);
        } else {
            SEGGER_RTT_printf(0, "BOOT: Coil measurement failed - using defaults\n");
        }
        
        /* Save updated calibration with tier and measured resistance */
        flow_calc_save_calibration(&m_flow_ctx);
        
        /* Set flag to show calibration needed on display */
        g_needs_calibration = true;
    }
    
    /* Apply duty cycle from calibration data */
    coil_driver_set_duty_cycle(&m_coil_ctx, 
                                m_flow_ctx.calibration.coil_on_time_ms,
                                m_flow_ctx.calibration.coil_off_time_ms);
    
    /* Apply PWM current control parameters from calibration */
    coil_driver_set_electrical_params(&m_coil_ctx,
                                       m_flow_ctx.calibration.supply_voltage_mv * 10,  /* Stored as /10 */
                                       m_flow_ctx.calibration.coil_resistance_mo);
    coil_driver_set_target_current(&m_coil_ctx, 
                                    m_flow_ctx.calibration.target_current_ma);
    
    /* Initialize temperature sensors */
    if (!temp_sensor_init(&g_temp_sensor)) {
        SEGGER_RTT_printf(0, "TEMP: Init failed (non-fatal)\n");
    } else {
        SEGGER_RTT_printf(0, "TEMP: Board NTC=%s, Coil TMP102=%s, Electrode TMP102=%s\n",
                          g_temp_sensor.ntc_valid ? "OK" : "FAIL",
                          g_temp_sensor.tmp102_coil_present ? "OK" : "N/A",
                          g_temp_sensor.tmp102_electrode_present ? "OK" : "N/A");
    }
    
    /* Set up DRDY callback for interrupt-driven sampling */
    ads131m0x_hal_set_drdy_callback(&m_adc_ctx, adc_drdy_callback, NULL);
    
    /* Prepare ADC with calibration before starting measurements */
    SEGGER_RTT_printf(0, "ADC: Preparing with calibration...\n");
    if (!flow_calc_adc_prepare(&m_flow_ctx)) {
        SEGGER_RTT_printf(0, "ADC: Preparation failed - continuing with defaults\n");
    }
    
    /* Start flow measurement */
    flow_calc_start(&m_flow_ctx);
    
    /* Apply auto-zero setting from calibration */
    flow_calc_set_auto_zero(&m_flow_ctx, m_flow_ctx.calibration.auto_zero_enabled);
    
    /* Start coil excitation with soft-start to limit inrush current */
    coil_driver_soft_start(&m_coil_ctx);
    
    SEGGER_RTT_printf(0, "ADC: Running at 16kSPS, coil at 2kHz (hardware timer)\n");
    
    TickType_t last_wake = xTaskGetTickCount();
    flow_state_t flow_state;
    uint32_t temp_read_counter = 0;
    
    for (;;) {
        /* Process coil duty cycle state machine */
        bool is_measuring = coil_driver_tick(&m_coil_ctx);
        
        /* Get current flow state */
        flow_calc_get_state(&m_flow_ctx, &flow_state);
        
        /* Update global flow data for LoRa task (full float precision) */
        g_flow_rate_lpm = flow_state.flow_rate_lpm;
        g_total_volume_l = flow_state.total_volume_l;
        g_signal_uv = flow_state.signal_uv;
        g_temperature_c = flow_state.temperature_c;
        g_signal_quality = flow_state.signal_quality;
        
        /* Set alarm flags */
        g_alarm_flags = 0;
        if (flow_state.reverse_flow) g_alarm_flags |= 0x01;
        if (flow_state.signal_low)   g_alarm_flags |= 0x02;
        if (flow_state.signal_high)  g_alarm_flags |= 0x04;
        if (flow_state.coil_fault)   g_alarm_flags |= 0x08;
        if (!is_measuring)           g_alarm_flags |= 0x10;  /* Coil sleeping */
        
        /* Check for auto-zero opportunity (only when measuring and flow stops) */
        if (is_measuring) {
            flow_calc_auto_zero_check(&m_flow_ctx);
        }
        
        /* Read temperature sensors every 10 seconds (100 * 100ms) */
        temp_read_counter++;
        if (temp_read_counter >= 100) {
            temp_read_counter = 0;
            temp_sensor_read_all(&g_temp_sensor);
            
            if (g_temp_sensor.ntc_valid && !isnan(g_temp_sensor.board_temp_c)) {
                SEGGER_RTT_printf(0, "TEMP: Board=%.1f°C", g_temp_sensor.board_temp_c);
                if (g_temp_sensor.tmp102_coil_present && !isnan(g_temp_sensor.coil_temp_c)) {
                    SEGGER_RTT_printf(0, ", Coil=%.1f°C", g_temp_sensor.coil_temp_c);
                }
                if (g_temp_sensor.tmp102_electrode_present && !isnan(g_temp_sensor.electrode_temp_c)) {
                    SEGGER_RTT_printf(0, ", Electrode=%.1f°C", g_temp_sensor.electrode_temp_c);
                }
                SEGGER_RTT_printf(0, "\n");
                
                /* Update flow state temperature for calibration tracking (use board temp for ADC) */
                m_flow_ctx.state.temperature_c = g_temp_sensor.board_temp_c;
                
                /* Check if ADC recalibration is needed due to temperature drift */
                if (flow_calc_adc_needs_calibration(&m_flow_ctx, g_temp_sensor.board_temp_c)) {
                    SEGGER_RTT_printf(0, "ADC: Temperature drift detected - performing quick recalibration\n");
                    
                    /* Stop flow measurement briefly for recalibration */
                    flow_calc_stop(&m_flow_ctx);
                    
                    /* Perform quick offset recalibration */
                    flow_calc_adc_quick_offset_cal(&m_flow_ctx);
                    
                    /* Resume flow measurement */
                    flow_calc_start(&m_flow_ctx);
                }
            }
        }
        
        /* Update display with flow data (every 100ms) */
        /* display_updateFlow(flow_state.flow_rate_lpm, flow_state.total_volume_l); */
        
        /* Task runs at 10Hz for state updates (ADC runs via interrupt) */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

/* ==========================================================================
 * DISPLAY TASK - LVGL UI management
 * ========================================================================== */

static void display_task(void *pvParameters)
{
    (void)pvParameters;
    
    SEGGER_RTT_printf(0, "Display task started\n");
    
    /* Initialize LVGL port (ST7789 + LVGL) */
    if (!lvgl_port_init()) {
        SEGGER_RTT_printf(0, "Display: LVGL port init failed!\n");
        vTaskSuspend(NULL);
    }
    
    /* Register button input device */
    lvgl_port_register_buttons();
    
    /* Create initial UI screen */
    /* TODO: Create main flow display screen */
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    
    /* Create flow rate label */
    lv_obj_t *lbl_flow = lv_label_create(screen);
    lv_label_set_text(lbl_flow, "0.00 LPM");
    lv_obj_set_style_text_font(lbl_flow, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_flow, lv_color_hex(0x00FF00), 0);
    lv_obj_align(lbl_flow, LV_ALIGN_CENTER, 0, -30);
    
    /* Create total volume label */
    lv_obj_t *lbl_total = lv_label_create(screen);
    lv_label_set_text(lbl_total, "Total: 0.00 L");
    lv_obj_set_style_text_font(lbl_total, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_total, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_total, LV_ALIGN_CENTER, 0, 20);
    
    /* Create status label */
    lv_obj_t *lbl_status = lv_label_create(screen);
    lv_label_set_text(lbl_status, "Measuring...");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t last_tick = xTaskGetTickCount();
    
    for (;;) {
        /* Update LVGL tick */
        TickType_t now = xTaskGetTickCount();
        lvgl_port_tick((now - last_tick) * portTICK_PERIOD_MS);
        last_tick = now;
        
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
            
            /* Wake display if sleeping */
            if (lvgl_port_is_sleeping()) {
                lvgl_port_wake();
            }
            
            SEGGER_RTT_printf(0, "Button event: %d\n", btn_event);
        }
        
        /* Update display power state */
        TickType_t idle_time = xTaskGetTickCount() - m_last_activity_tick;
        if (m_alarm_state.type == ALARM_CLEARED) {
            if (idle_time > pdMS_TO_TICKS(AGSYS_DISPLAY_DIM_TIMEOUT_SEC * 1000 + 
                                          AGSYS_DISPLAY_SLEEP_TIMEOUT_SEC * 1000)) {
                m_display_power = DISPLAY_SLEEP;
                if (!lvgl_port_is_sleeping()) {
                    lvgl_port_sleep();
                }
            } else if (idle_time > pdMS_TO_TICKS(AGSYS_DISPLAY_DIM_TIMEOUT_SEC * 1000)) {
                m_display_power = DISPLAY_DIM;
                lvgl_port_set_brightness(30);
            } else {
                lvgl_port_set_brightness(100);
            }
        }
        
        /* Update flow display labels */
        static char flow_str[32];
        static char total_str[32];
        snprintf(flow_str, sizeof(flow_str), "%.2f LPM", g_flow_rate_lpm);
        snprintf(total_str, sizeof(total_str), "Total: %.2f L", g_total_volume_l);
        lv_label_set_text(lbl_flow, flow_str);
        lv_label_set_text(lbl_total, total_str);
        
        /* Update status based on alarm flags */
        if (g_alarm_flags & 0x10) {
            lv_label_set_text(lbl_status, "Coil sleeping...");
        } else if (g_alarm_flags & 0x08) {
            lv_label_set_text(lbl_status, "COIL FAULT!");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF0000), 0);
        } else {
            lv_label_set_text(lbl_status, "Measuring...");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x888888), 0);
        }
        
        /* Run LVGL task handler */
        lvgl_port_task_handler();
        
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

