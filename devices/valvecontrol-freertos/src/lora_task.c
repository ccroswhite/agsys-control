/**
 * @file lora_task.c
 * @brief LoRa task implementation for Valve Controller
 * 
 * Handles RFM95C communication with property controller using AgSys protocol.
 * Uses the shared agsys_lora driver from freertos-common.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include "SEGGER_RTT.h"

#include "lora_task.h"
#include "can_task.h"
#include "agsys_lora.h"
#include "board_config.h"
#include "agsys_device.h"
#include "agsys_protocol.h"
#include "agsys_memory_layout.h"
#include "agsys_fram.h"

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

/* Boot reason - set during startup */
static uint8_t m_boot_reason = AGSYS_BOOT_REASON_NORMAL;

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define LORA_FREQUENCY              915000000
#ifndef LORA_SPREADING_FACTOR
#define LORA_SPREADING_FACTOR       10
#endif
#define LORA_BANDWIDTH              125000
#define LORA_CODING_RATE            5
#define LORA_TX_POWER               20
#define STATUS_REPORT_INTERVAL_MS   60000
#define SCHEDULE_PULL_INTERVAL_MS   300000

/* AgSys message types - legacy aliases */
#define AGSYS_MSG_SCHEDULE_UPDATE   AGSYS_MSG_VALVE_SCHEDULE
#define AGSYS_MSG_SCHEDULE_REQUEST  AGSYS_MSG_VALVE_SCHEDULE_REQ
#define AGSYS_MSG_VALVE_DISCOVER    0x60
#define AGSYS_MSG_VALVE_DISCOVERY_RESP 0x61

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static TaskHandle_t m_task_handle = NULL;
static uint8_t m_device_uid[8];
static uint8_t m_sequence = 0;

/* Shared LoRa driver context */
static agsys_lora_ctx_t m_lora_ctx;

/* External power state from main.c */
extern volatile bool g_on_battery_power;

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
    hdr->device_type = AGSYS_DEVICE_TYPE_VALVE_CONTROLLER;
    memcpy(hdr->device_uid, m_device_uid, 8);
    hdr->sequence = m_sequence++;
}

/* ==========================================================================
 * MESSAGE PROCESSING
 * ========================================================================== */

static void process_valve_command(const uint8_t *payload, uint8_t len)
{
    if (len < 11) return;  /* 8 byte UID + 1 cmd + 2 command_id */
    
    const uint8_t *actuator_uid = payload;
    uint8_t command = payload[8];
    uint16_t command_id = (payload[9] << 8) | payload[10];
    
    SEGGER_RTT_printf(0, "Valve cmd: UID=%02X%02X... cmd=%d id=%d\n", 
                      actuator_uid[0], actuator_uid[1], command, command_id);
    
    bool success = false;
    uint8_t result_state = 0xFF;
    
    switch (command) {
        case 0x01:  /* Open */
            success = can_open_valve_by_uid(actuator_uid);
            break;
        case 0x02:  /* Close */
            success = can_close_valve_by_uid(actuator_uid);
            break;
        case 0x03:  /* Stop */
            success = can_stop_valve_by_uid(actuator_uid);
            break;
        case 0x04:  /* Emergency close all */
            can_emergency_close_all();
            success = true;
            break;
    }
    
    if (success) {
        result_state = can_get_valve_state_by_uid(actuator_uid);
    }
    
    lora_send_valve_ack_by_uid(actuator_uid, command_id, result_state, success, 0);
}

static void process_discovery_command(void)
{
    SEGGER_RTT_printf(0, "Discovery command received\n");
    can_discover_all();
    vTaskDelay(pdMS_TO_TICKS(500));
    lora_send_discovery_response();
}

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
        case AGSYS_MSG_VALVE_COMMAND:
            process_valve_command(payload, payload_len);
            break;
            
        case AGSYS_MSG_VALVE_DISCOVER:
            process_discovery_command();
            break;
            
        case AGSYS_MSG_TIME_SYNC:
            if (payload_len >= 4) {
                uint32_t timestamp = (payload[0] << 24) | (payload[1] << 16) |
                                     (payload[2] << 8) | payload[3];
                SEGGER_RTT_printf(0, "Time sync: %u\n", timestamp);
            }
            break;
            
        case AGSYS_MSG_SCHEDULE_UPDATE:
            SEGGER_RTT_printf(0, "Schedule update received\n");
            break;
    }
}

/* ==========================================================================
 * RX CALLBACK
 * ========================================================================== */

static void lora_rx_callback(const uint8_t *data, size_t len, int16_t rssi, int8_t snr)
{
    process_lora_message(data, len, rssi);
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
 * PUBLIC API
 * ========================================================================== */

bool lora_task_init(void)
{
    get_device_uid();
    load_boot_reason_from_fram();
    return true;
}

void lora_task(void *pvParameters)
{
    (void)pvParameters;
    m_task_handle = xTaskGetCurrentTaskHandle();
    
    SEGGER_RTT_printf(0, "LoRa task started\n");
    
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
                                       AGSYS_SPI_BUS_0,
                                       &lora_config);
    if (err != AGSYS_OK) {
        SEGGER_RTT_printf(0, "LoRa: Init failed (err=%d)\n", err);
        vTaskDelete(NULL);
        return;
    }
    
    SEGGER_RTT_printf(0, "LoRa: Initialized using shared agsys_lora driver\n");
    
    /* Start continuous receive with callback */
    agsys_lora_receive_start(&m_lora_ctx, lora_rx_callback);
    
    uint8_t rx_buf[128];
    int16_t rssi;
    int8_t snr;
    TickType_t last_status_report = 0;
    TickType_t last_schedule_pull = 0;
    
    for (;;) {
        /* Poll for received data (callback handles processing) */
        int len = agsys_lora_receive(&m_lora_ctx, rx_buf, sizeof(rx_buf), 
                                      &rssi, &snr, 1000);
        if (len > 0) {
            process_lora_message(rx_buf, len, rssi);
        }
        
        TickType_t now = xTaskGetTickCount();
        
        /* Periodic status report (only on mains power) */
        if (!g_on_battery_power && 
            (now - last_status_report) >= pdMS_TO_TICKS(STATUS_REPORT_INTERVAL_MS)) {
            lora_send_status_report();
            last_status_report = now;
        }
        
        /* Periodic schedule pull (only on mains power) */
        if (!g_on_battery_power && 
            (now - last_schedule_pull) >= pdMS_TO_TICKS(SCHEDULE_PULL_INTERVAL_MS)) {
            lora_request_schedule();
            last_schedule_pull = now;
        }
    }
}

void lora_send_status_report(void)
{
    uint8_t buffer[128];
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_VALVE_STATUS);
    
    uint8_t *payload = buffer + sizeof(agsys_header_t);
    
    payload[0] = FW_VERSION_MAJOR;
    payload[1] = FW_VERSION_MINOR;
    payload[2] = FW_VERSION_PATCH;
    payload[3] = m_boot_reason;
    
    uint8_t count = 0;
    uint8_t *actuator_data = payload + 5;
    
    for (uint8_t addr = ACTUATOR_ADDR_MIN; addr <= ACTUATOR_ADDR_MAX && count < 20; addr++) {
        if (can_is_actuator_online(addr)) {
            const actuator_status_t *act = can_get_actuator(addr);
            actuator_data[count * 4 + 0] = addr;
            actuator_data[count * 4 + 1] = act->status_flags;
            actuator_data[count * 4 + 2] = (act->current_ma >> 8) & 0xFF;
            actuator_data[count * 4 + 3] = act->current_ma & 0xFF;
            count++;
        }
    }
    
    payload[4] = count;
    uint8_t total_len = sizeof(agsys_header_t) + 5 + (count * 4);
    
    if (agsys_lora_transmit(&m_lora_ctx, buffer, total_len) == AGSYS_OK) {
        SEGGER_RTT_printf(0, "Status report sent: %d actuators\n", count);
        
        if (m_boot_reason != AGSYS_BOOT_REASON_NORMAL) {
            m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
        }
    }
}

void lora_request_schedule(void)
{
    uint8_t buffer[sizeof(agsys_header_t)];
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_SCHEDULE_REQUEST);
    
    if (agsys_lora_transmit(&m_lora_ctx, buffer, sizeof(buffer)) == AGSYS_OK) {
        SEGGER_RTT_printf(0, "Schedule request sent\n");
    }
}

void lora_send_valve_ack_by_uid(const uint8_t actuator_uid[8], uint16_t command_id,
                                 uint8_t result_state, bool success, uint8_t error_code)
{
    uint8_t buffer[sizeof(agsys_header_t) + 13];
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_VALVE_ACK);
    
    uint8_t *payload = buffer + sizeof(agsys_header_t);
    memcpy(payload, actuator_uid, 8);
    payload[8] = (command_id >> 8) & 0xFF;
    payload[9] = command_id & 0xFF;
    payload[10] = result_state;
    payload[11] = success ? 1 : 0;
    payload[12] = error_code;
    
    bool tx_success = (agsys_lora_transmit(&m_lora_ctx, buffer, sizeof(buffer)) == AGSYS_OK);
    
    if (tx_success) {
        SEGGER_RTT_printf(0, "Valve ACK sent for UID %02X%02X...\n", 
                          actuator_uid[0], actuator_uid[1]);
    }
    
    /* Log valve event to flash */
    agsys_device_log_valve(&m_device_ctx, actuator_uid[0], result_state, 
                           success ? 100 : 0);
    
    if (!tx_success) {
        SEGGER_RTT_printf(0, "Valve ACK TX failed, logged to flash\n");
    }
}

void lora_send_discovery_response(void)
{
    uint8_t buffer[200];
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_VALVE_DISCOVERY_RESP);
    
    uint8_t *payload = buffer + sizeof(agsys_header_t);
    uint8_t count = 0;
    
    for (uint8_t addr = ACTUATOR_ADDR_MIN; addr <= ACTUATOR_ADDR_MAX && count < 15; addr++) {
        const actuator_status_t *act = can_get_actuator(addr);
        if (act && act->online && act->uid_known) {
            uint8_t *entry = payload + 1 + (count * 10);
            entry[0] = addr;
            memcpy(entry + 1, act->uid, 8);
            entry[9] = act->status_flags;
            count++;
        }
    }
    
    payload[0] = count;
    uint8_t total_len = sizeof(agsys_header_t) + 1 + (count * 10);
    
    if (agsys_lora_transmit(&m_lora_ctx, buffer, total_len) == AGSYS_OK) {
        SEGGER_RTT_printf(0, "Discovery response sent: %d actuators\n", count);
    }
}
