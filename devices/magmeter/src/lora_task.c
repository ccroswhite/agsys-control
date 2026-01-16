/**
 * @file lora_task.c
 * @brief LoRa task implementation for Water Meter
 * 
 * Handles RFM95C communication with property controller using AgSys protocol.
 * Uses the shared agsys_lora driver from freertos-common.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "SEGGER_RTT.h"

#include "lora_task.h"
#include "agsys_lora.h"
#include "board_config.h"
#include "agsys_device.h"
#include "agsys_protocol.h"
#include "agsys_memory_layout.h"
#include "agsys_fram.h"
#include "flow_calc.h"

#include <string.h>

/* Firmware version - should match build */
#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR    1
#endif
#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR    0
#endif
#ifndef FW_VERSION_PATCH
#define FW_VERSION_PATCH    0
#endif

/* External device context for logging */
extern agsys_device_ctx_t m_device_ctx;

/* External flow data from main.c */
extern volatile float g_flow_rate_lpm;
extern volatile float g_total_volume_l;
extern volatile uint8_t g_alarm_flags;
extern flow_calibration_t *g_calibration_ptr;

/* Boot reason - set during startup */
static uint8_t m_boot_reason = AGSYS_BOOT_REASON_NORMAL;

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define LORA_FREQUENCY          915000000
#ifndef LORA_SPREADING_FACTOR
#define LORA_SPREADING_FACTOR   10
#endif
#define LORA_BANDWIDTH          125000
#define LORA_CODING_RATE        5
#define LORA_TX_POWER           20

/* Task configuration */
#ifndef TASK_STACK_LORA
#define TASK_STACK_LORA         512
#endif
#ifndef TASK_PRIORITY_LORA
#define TASK_PRIORITY_LORA      2
#endif

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static TaskHandle_t m_task_handle = NULL;
static bool m_initialized = false;
static uint8_t m_device_uid[8];
static uint8_t m_sequence = 0;

/* Shared LoRa driver context */
static agsys_lora_ctx_t m_lora_ctx;

/* ==========================================================================
 * PACKET BUILDING
 * ========================================================================== */

static void get_device_uid(void)
{
    uint32_t uid0 = NRF_FICR->DEVICEID[0];
    uint32_t uid1 = NRF_FICR->DEVICEID[1];
    
    m_device_uid[0] = (uid0 >> 0) & 0xFF;
    m_device_uid[1] = (uid0 >> 8) & 0xFF;
    m_device_uid[2] = (uid0 >> 16) & 0xFF;
    m_device_uid[3] = (uid0 >> 24) & 0xFF;
    m_device_uid[4] = (uid1 >> 0) & 0xFF;
    m_device_uid[5] = (uid1 >> 8) & 0xFF;
    m_device_uid[6] = (uid1 >> 16) & 0xFF;
    m_device_uid[7] = (uid1 >> 24) & 0xFF;
}

static void build_header(agsys_header_t *hdr, uint8_t msg_type)
{
    hdr->magic[0] = AGSYS_MAGIC_BYTE1;
    hdr->magic[1] = AGSYS_MAGIC_BYTE2;
    hdr->version = AGSYS_PROTOCOL_VERSION;
    hdr->msg_type = msg_type;
    hdr->device_type = AGSYS_DEVICE_TYPE_WATER_METER;
    memcpy(hdr->device_uid, m_device_uid, 8);
    hdr->sequence = m_sequence++;
}

static bool send_meter_report(float flow_rate_lpm, float total_volume_l, uint8_t alarm_flags)
{
    uint8_t buffer[64];
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_METER_REPORT);
    
    agsys_meter_report_t *report = (agsys_meter_report_t *)(buffer + sizeof(agsys_header_t));
    
    report->timestamp = xTaskGetTickCount() / configTICK_RATE_HZ;
    report->total_pulses = 0;
    report->total_liters = (uint32_t)total_volume_l;
    report->flow_rate_lpm = (uint16_t)(flow_rate_lpm * 10);
    report->battery_mv = 0;
    report->flags = alarm_flags;
    
    /* Note: fw_version and boot_reason not in canonical agsys_meter_report_t */
    
    uint8_t total_len = sizeof(agsys_header_t) + sizeof(agsys_meter_report_t);
    
    return (agsys_lora_transmit(&m_lora_ctx, buffer, total_len) == AGSYS_OK);
}

/* ==========================================================================
 * MESSAGE PROCESSING
 * ========================================================================== */

static void process_lora_message(const uint8_t *data, uint8_t len, int16_t rssi)
{
    if (len < sizeof(agsys_header_t)) return;
    
    agsys_header_t *hdr = (agsys_header_t *)data;
    
    if (hdr->magic[0] != AGSYS_MAGIC_BYTE1 || hdr->magic[1] != AGSYS_MAGIC_BYTE2) {
        SEGGER_RTT_printf(0, "LoRa RX: Invalid magic bytes\n");
        return;
    }
    
    const uint8_t *payload = data + sizeof(agsys_header_t);
    uint8_t payload_len = len - sizeof(agsys_header_t);
    
    SEGGER_RTT_printf(0, "LoRa RX: type=0x%02X len=%d rssi=%d\n", 
                      hdr->msg_type, len, rssi);
    
    switch (hdr->msg_type) {
        case AGSYS_MSG_ACK:
            if (payload_len >= sizeof(agsys_ack_t)) {
                agsys_ack_t *ack = (agsys_ack_t *)payload;
                SEGGER_RTT_printf(0, "LoRa: ACK received (flags=0x%02X)\n", ack->flags);
                
                /* Check for config available flag */
                if (ack->flags & AGSYS_ACK_FLAG_CONFIG_AVAILABLE) {
                    SEGGER_RTT_printf(0, "LoRa: Config update available\n");
                }
                
                if (m_boot_reason != AGSYS_BOOT_REASON_NORMAL) {
                    m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
                }
            }
            break;
            
        case AGSYS_MSG_TIME_SYNC:
            if (payload_len >= 4) {
                uint32_t timestamp = (payload[0] << 24) | (payload[1] << 16) |
                                     (payload[2] << 8) | payload[3];
                SEGGER_RTT_printf(0, "Time sync: %u\n", timestamp);
            }
            break;
            
        case AGSYS_MSG_CONFIG_UPDATE:
            SEGGER_RTT_printf(0, "Config update received\n");
            break;
            
        case AGSYS_MSG_METER_RESET_TOTAL:
            SEGGER_RTT_printf(0, "Reset totalizer command received\n");
            break;
    }
}

/* ==========================================================================
 * BOOT REASON HANDLING
 * ========================================================================== */

static void load_boot_reason_from_fram(void)
{
    agsys_ota_fram_state_t ota_state;
    
    if (agsys_fram_read(&m_device_ctx.fram_ctx, AGSYS_FRAM_OTA_STATE_ADDR, 
                        (uint8_t *)&ota_state, sizeof(ota_state)) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "LoRa: Failed to read OTA state from FRAM\n");
        return;
    }
    
    if (ota_state.magic != AGSYS_OTA_FRAM_MAGIC) {
        m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
        return;
    }
    
    switch (ota_state.state) {
        case AGSYS_OTA_STATE_SUCCESS:
            m_boot_reason = AGSYS_BOOT_REASON_OTA_SUCCESS;
            SEGGER_RTT_printf(0, "LoRa: Boot after successful OTA to v%d.%d.%d\n",
                              ota_state.target_version[0],
                              ota_state.target_version[1],
                              ota_state.target_version[2]);
            break;
            
        case AGSYS_OTA_STATE_ROLLED_BACK:
        case AGSYS_OTA_STATE_FAILED:
            m_boot_reason = AGSYS_BOOT_REASON_OTA_ROLLBACK;
            SEGGER_RTT_printf(0, "LoRa: Boot after OTA rollback (error=%d)\n",
                              ota_state.error_code);
            break;
            
        default:
            m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
            break;
    }
    
    /* Clear OTA state after reading */
    if (ota_state.state == AGSYS_OTA_STATE_SUCCESS || 
        ota_state.state == AGSYS_OTA_STATE_ROLLED_BACK ||
        ota_state.state == AGSYS_OTA_STATE_FAILED) {
        ota_state.state = AGSYS_OTA_STATE_NONE;
        ota_state.magic = 0;
        agsys_fram_write(&m_device_ctx.fram_ctx, AGSYS_FRAM_OTA_STATE_ADDR,
                         (uint8_t *)&ota_state, sizeof(ota_state));
    }
}

/* ==========================================================================
 * LORA TASK
 * ========================================================================== */

static void lora_task_func(void *pvParameters)
{
    (void)pvParameters;
    
    SEGGER_RTT_printf(0, "LoRa task started\n");
    
    /* Get device UID */
    get_device_uid();
    SEGGER_RTT_printf(0, "Device UID: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                      m_device_uid[0], m_device_uid[1], m_device_uid[2], m_device_uid[3],
                      m_device_uid[4], m_device_uid[5], m_device_uid[6], m_device_uid[7]);
    
    /* Initialize LoRa using shared driver */
    agsys_lora_config_t lora_config = {
        .frequency = LORA_FREQUENCY,
        .spreading_factor = LORA_SPREADING_FACTOR,
        .bandwidth = LORA_BANDWIDTH,
        .coding_rate = LORA_CODING_RATE,
        .tx_power = LORA_TX_POWER,
        .crc_enabled = true,
    };
    
    agsys_err_t err = agsys_lora_init(&m_lora_ctx,
                                       SPI_CS_LORA_PIN,
                                       LORA_RESET_PIN,
                                       LORA_DIO0_PIN,
                                       AGSYS_SPI_BUS_2,
                                       &lora_config);
    if (err != AGSYS_OK) {
        SEGGER_RTT_printf(0, "LoRa: Init failed (err=%d)\n", err);
        vTaskDelete(NULL);
        return;
    }
    
    m_initialized = true;
    SEGGER_RTT_printf(0, "LoRa: Initialized using shared agsys_lora driver\n");
    
    /* Get report interval from calibration data */
    uint8_t display_sec = 0;
    uint8_t lora_mult = 0;
    
    if (g_calibration_ptr != NULL) {
        display_sec = g_calibration_ptr->display_update_sec;
        lora_mult = g_calibration_ptr->lora_report_mult;
    }
    
    if (display_sec == 0) display_sec = AGSYS_DISPLAY_UPDATE_SEC_DEFAULT;
    if (lora_mult == 0) lora_mult = AGSYS_LORA_REPORT_MULT_DEFAULT;
    
    uint32_t report_interval_ms = (uint32_t)display_sec * lora_mult * 1000;
    SEGGER_RTT_printf(0, "LoRa: Report interval = %lu ms (%d s * %d)\n", 
                      report_interval_ms, display_sec, lora_mult);
    
    TickType_t last_report = xTaskGetTickCount();
    
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        
        /* Send periodic reports */
        if ((now - last_report) >= pdMS_TO_TICKS(report_interval_ms)) {
            last_report = now;
            
            float flow_rate = g_flow_rate_lpm;
            float total_vol = g_total_volume_l;
            uint8_t alarms = g_alarm_flags;
            
            SEGGER_RTT_printf(0, "LoRa: Sending report (flow=%.1f L/min, total=%.1f L)\n",
                              flow_rate, total_vol);
            
            nrf_gpio_pin_set(LED_LORA_PIN);
            
            bool tx_success = send_meter_report(flow_rate, total_vol, alarms);
            
            if (tx_success) {
                SEGGER_RTT_printf(0, "LoRa: TX success\n");
                
                /* Wait for ACK using shared driver */
                uint8_t rx_buf[64];
                int16_t rssi;
                int8_t snr;
                int rx_len = agsys_lora_receive(&m_lora_ctx, rx_buf, sizeof(rx_buf), 
                                                 &rssi, &snr, 2000);
                
                if (rx_len > 0) {
                    process_lora_message(rx_buf, rx_len, rssi);
                }
            } else {
                SEGGER_RTT_printf(0, "LoRa: TX failed\n");
                
                uint32_t flow_mlpm = (uint32_t)(flow_rate * 1000);
                uint32_t total_ml = (uint32_t)(total_vol * 1000);
                agsys_device_log_meter(&m_device_ctx, flow_mlpm, total_ml, alarms);
            }
            
            nrf_gpio_pin_clear(LED_LORA_PIN);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

void lora_task_init(void)
{
    nrf_gpio_cfg_output(LED_LORA_PIN);
    nrf_gpio_pin_clear(LED_LORA_PIN);
    
    load_boot_reason_from_fram();
}

void lora_task_start(void)
{
    xTaskCreate(lora_task_func, "LoRa", TASK_STACK_LORA, NULL, 
                TASK_PRIORITY_LORA, &m_task_handle);
}

bool lora_is_initialized(void)
{
    return m_initialized;
}
