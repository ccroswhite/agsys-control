/**
 * @file lora_task.c
 * @brief LoRa task implementation for Soil Moisture Sensor
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
extern agsys_fram_ctx_t m_fram_ctx;

/* Boot reason - set during startup */
static uint8_t m_boot_reason = AGSYS_BOOT_REASON_NORMAL;

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define LORA_FREQUENCY          915000000
#define LORA_SPREADING_FACTOR   10
#define LORA_BANDWIDTH          125000
#define LORA_CODING_RATE        5
#define LORA_TX_POWER           20
#define LORA_MAX_RETRIES        3
#ifndef LORA_ACK_TIMEOUT_MS
#define LORA_ACK_TIMEOUT_MS     2000
#endif

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static TaskHandle_t m_task_handle = NULL;
static bool m_initialized = false;
static uint16_t m_sequence = 0;

/* Shared LoRa driver context */
static agsys_lora_ctx_t m_lora_ctx;

/* ==========================================================================
 * PACKET BUILDING
 * ========================================================================== */

static void build_header(agsys_header_t *hdr, uint8_t msg_type)
{
    hdr->magic[0] = AGSYS_MAGIC_BYTE1;
    hdr->magic[1] = AGSYS_MAGIC_BYTE2;
    hdr->version = AGSYS_PROTOCOL_VERSION;
    hdr->msg_type = msg_type;
    hdr->device_type = AGSYS_DEVICE_TYPE_SOIL_MOISTURE;
    hdr->sequence = m_sequence++;
    
    /* Copy device UID */
    agsys_device_get_uid(hdr->device_uid);
}

static uint8_t build_sensor_report(uint8_t *buffer, size_t buf_size,
                                    const uint8_t *device_uid,
                                    uint32_t probe_freqs[4],
                                    uint8_t probe_moisture[4],
                                    uint16_t battery_mv,
                                    uint8_t flags)
{
    if (buf_size < sizeof(agsys_header_t) + sizeof(agsys_soil_report_t)) {
        return 0;
    }
    
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    build_header(hdr, AGSYS_MSG_SOIL_REPORT);
    
    agsys_soil_report_t *report = (agsys_soil_report_t *)(buffer + sizeof(agsys_header_t));
    
    /* Timestamp */
    report->timestamp = xTaskGetTickCount() / configTICK_RATE_HZ;
    
    /* Probe data using canonical structure */
    report->probe_count = NUM_MOISTURE_PROBES;
    for (int i = 0; i < NUM_MOISTURE_PROBES && i < AGSYS_MAX_PROBES; i++) {
        report->probes[i].probe_index = i;
        report->probes[i].frequency_hz = (uint16_t)(probe_freqs[i] & 0xFFFF);
        report->probes[i].moisture_percent = probe_moisture[i];
    }
    
    report->battery_mv = battery_mv;
    report->temperature = 0;  /* TODO: Add temperature sensor */
    report->pending_logs = (uint8_t)agsys_device_log_pending_count(&m_device_ctx);
    report->flags = flags;
    
    /* Add boot reason to flags if not normal */
    if (m_boot_reason == AGSYS_BOOT_REASON_OTA_SUCCESS) {
        report->flags |= AGSYS_SENSOR_FLAG_FIRST_BOOT;
    }
    
    return sizeof(agsys_header_t) + sizeof(agsys_soil_report_t);
}

/* ==========================================================================
 * BOOT REASON HANDLING
 * ========================================================================== */

static void load_boot_reason_from_fram(void)
{
    agsys_ota_fram_state_t ota_state;
    
    if (agsys_fram_read(&m_fram_ctx, AGSYS_FRAM_OTA_STATE_ADDR,
                        (uint8_t *)&ota_state, sizeof(ota_state)) != AGSYS_OK) {
        m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
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
            m_boot_reason = AGSYS_BOOT_REASON_OTA_ROLLBACK;
            SEGGER_RTT_printf(0, "LoRa: Boot after OTA rollback (error=%d)\n",
                              ota_state.error_code);
            break;
            
        case AGSYS_OTA_STATE_FAILED:
            m_boot_reason = AGSYS_BOOT_REASON_OTA_ROLLBACK;
            SEGGER_RTT_printf(0, "LoRa: Boot after OTA failure (error=%d)\n",
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
        agsys_fram_write(&m_fram_ctx, AGSYS_FRAM_OTA_STATE_ADDR,
                         (uint8_t *)&ota_state, sizeof(ota_state));
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool lora_task_init(void)
{
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
    
    m_initialized = true;
    SEGGER_RTT_printf(0, "LoRa: Initialized using shared agsys_lora driver\n");
    
    for (;;) {
        /* Wait for sensor data ready notification */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        SEGGER_RTT_printf(0, "LoRa: Preparing to transmit\n");
        
        /* TODO: Get actual sensor data from sensor task */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool lora_send_sensor_report(const uint8_t *device_uid,
                              uint32_t probe_freqs[4],
                              uint8_t probe_moisture[4],
                              uint16_t battery_mv,
                              uint8_t flags)
{
    uint8_t buffer[64];
    
    uint8_t len = build_sensor_report(buffer, sizeof(buffer),
                                       device_uid, probe_freqs,
                                       probe_moisture, battery_mv, flags);
    if (len == 0) {
        SEGGER_RTT_printf(0, "LoRa: Failed to build packet\n");
        return false;
    }
    
    /* Use shared driver's TX with retry */
    agsys_lora_tx_result_t result = agsys_lora_tx_with_retry(&m_lora_ctx,
                                                               buffer, len,
                                                               LORA_MAX_RETRIES,
                                                               LORA_ACK_TIMEOUT_MS);
    
    if (result == AGSYS_LORA_TX_SUCCESS) {
        /* Clear boot reason after first successful report */
        if (m_boot_reason != AGSYS_BOOT_REASON_NORMAL) {
            m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
        }
        return true;
    }
    
    /* TX failed - log reading to flash for later sync */
    SEGGER_RTT_printf(0, "LoRa: TX failed, logging to flash\n");
    
    uint16_t readings[4];
    for (int i = 0; i < NUM_MOISTURE_PROBES; i++) {
        readings[i] = probe_moisture[i];
    }
    
    if (agsys_device_log_sensor(&m_device_ctx, readings, NUM_MOISTURE_PROBES, battery_mv)) {
        SEGGER_RTT_printf(0, "LoRa: Reading logged to flash (%lu pending)\n",
                          agsys_device_log_pending_count(&m_device_ctx));
    } else {
        SEGGER_RTT_printf(0, "LoRa: Failed to log reading to flash\n");
    }
    
    return false;
}

void lora_sleep(void)
{
    if (!m_initialized) return;
    
    agsys_lora_sleep(&m_lora_ctx);
    SEGGER_RTT_printf(0, "LoRa: Sleep\n");
}

void lora_wake(void)
{
    if (!m_initialized) return;
    
    /* Wake by entering standby - agsys_lora doesn't have explicit wake */
    agsys_lora_receive_stop(&m_lora_ctx);
    SEGGER_RTT_printf(0, "LoRa: Wake\n");
}
