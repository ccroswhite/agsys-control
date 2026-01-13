/**
 * @file lora_task.c
 * @brief LoRa task implementation for Water Meter (Magmeter)
 * 
 * Handles RFM95C communication with property controller using AgSys protocol.
 * Uses the canonical protocol definition from agsys-api.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"
#include "SEGGER_RTT.h"

#include "lora_task.h"
#include "board_config.h"
#include "agsys_config.h"
#include "agsys_device.h"
#include "agsys_protocol.h"
#include "agsys_memory_layout.h"
#include "agsys_fram.h"
#include "agsys_spi.h"
#include "flow_calc.h"
#include "temp_sensor.h"

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

/* External calibration data for intervals */
extern flow_calibration_t g_calibration;

/* External temperature sensor */
extern temp_sensor_ctx_t g_temp_sensor;

/* ==========================================================================
 * RFM95C REGISTER DEFINITIONS
 * ========================================================================== */

#define REG_FIFO                0x00
#define REG_OP_MODE             0x01
#define REG_FRF_MSB             0x06
#define REG_FRF_MID             0x07
#define REG_FRF_LSB             0x08
#define REG_PA_CONFIG           0x09
#define REG_LNA                 0x0C
#define REG_FIFO_ADDR_PTR       0x0D
#define REG_FIFO_TX_BASE        0x0E
#define REG_FIFO_RX_BASE        0x0F
#define REG_FIFO_RX_CURRENT     0x10
#define REG_IRQ_FLAGS           0x12
#define REG_RX_NB_BYTES         0x13
#define REG_PKT_SNR             0x19
#define REG_PKT_RSSI            0x1A
#define REG_MODEM_CONFIG_1      0x1D
#define REG_MODEM_CONFIG_2      0x1E
#define REG_PREAMBLE_MSB        0x20
#define REG_PREAMBLE_LSB        0x21
#define REG_PAYLOAD_LENGTH      0x22
#define REG_MODEM_CONFIG_3      0x26
#define REG_SYNC_WORD           0x39
#define REG_DIO_MAPPING_1       0x40
#define REG_VERSION             0x42
#define REG_PA_DAC              0x4D

/* Operating modes */
#define MODE_SLEEP              0x00
#define MODE_STDBY              0x01
#define MODE_TX                 0x03
#define MODE_RX_CONTINUOUS      0x05
#define MODE_RX_SINGLE          0x06
#define MODE_LORA               0x80

/* IRQ flags */
#define IRQ_TX_DONE             0x08
#define IRQ_RX_DONE             0x40
#define IRQ_PAYLOAD_CRC_ERROR   0x20

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static TaskHandle_t m_task_handle = NULL;
static bool m_initialized = false;
static uint16_t m_sequence = 0;
static uint8_t m_device_uid[8];

/* Flow data from main.c */
extern volatile float g_flow_rate_lpm;
extern volatile float g_total_volume_l;
extern volatile uint8_t g_alarm_flags;

/* SPI handle for LoRa */
static agsys_spi_handle_t m_lora_spi_handle = AGSYS_SPI_INVALID_HANDLE;

/* ==========================================================================
 * SPI HELPERS (using shared DMA driver)
 * ========================================================================== */

static void spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    agsys_spi_xfer_t xfer = {
        .tx_buf = tx,
        .rx_buf = rx,
        .length = len,
    };
    agsys_spi_transfer(m_lora_spi_handle, &xfer);
}

/* ==========================================================================
 * RFM95C LOW-LEVEL FUNCTIONS
 * ========================================================================== */

static void rfm_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { reg | 0x80, value };
    spi_transfer(tx, NULL, 2);
}

static uint8_t rfm_read_reg(uint8_t reg)
{
    uint8_t tx[2] = { reg & 0x7F, 0x00 };
    uint8_t rx[2];
    spi_transfer(tx, rx, 2);
    return rx[1];
}

static void rfm_set_mode(uint8_t mode)
{
    rfm_write_reg(REG_OP_MODE, MODE_LORA | mode);
}

static void rfm_set_frequency(uint32_t freq)
{
    uint64_t frf = ((uint64_t)freq << 19) / 32000000;
    rfm_write_reg(REG_FRF_MSB, (frf >> 16) & 0xFF);
    rfm_write_reg(REG_FRF_MID, (frf >> 8) & 0xFF);
    rfm_write_reg(REG_FRF_LSB, frf & 0xFF);
}

static bool rfm_init(void)
{
    /* Register with SPI manager */
    agsys_spi_config_t spi_config = {
        .cs_pin = SPI_CS_LORA_PIN,
        .cs_active_low = true,
        .frequency = NRF_SPIM_FREQ_4M,
        .mode = 0,
    };
    
    if (agsys_spi_register(&spi_config, &m_lora_spi_handle) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "LoRa: Failed to register SPI\n");
        return false;
    }
    
    /* Reset */
    nrf_gpio_cfg_output(LORA_RESET_PIN);
    nrf_gpio_pin_clear(LORA_RESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(10));
    nrf_gpio_pin_set(LORA_RESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Check version */
    uint8_t version = rfm_read_reg(REG_VERSION);
    SEGGER_RTT_printf(0, "RFM95 version: 0x%02X\n", version);
    
    if (version != 0x12) {
        SEGGER_RTT_printf(0, "RFM95: Invalid version (expected 0x12)\n");
        return false;
    }
    
    /* Sleep mode for configuration */
    rfm_set_mode(MODE_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Set frequency */
    rfm_set_frequency(LORA_FREQUENCY);
    
    /* Configure modem: BW=125kHz, CR=4/5, explicit header */
    rfm_write_reg(REG_MODEM_CONFIG_1, 0x72);
    
    /* SF=7, CRC on */
    rfm_write_reg(REG_MODEM_CONFIG_2, (LORA_SPREADING_FACTOR << 4) | 0x04);
    
    /* LNA gain auto, low data rate optimize off */
    rfm_write_reg(REG_MODEM_CONFIG_3, 0x04);
    
    /* TX power +20dBm */
    rfm_write_reg(REG_PA_CONFIG, 0x8F);
    rfm_write_reg(REG_PA_DAC, 0x87);
    
    /* Preamble length 8 */
    rfm_write_reg(REG_PREAMBLE_MSB, 0x00);
    rfm_write_reg(REG_PREAMBLE_LSB, 0x08);
    
    /* Sync word */
    rfm_write_reg(REG_SYNC_WORD, 0x34);
    
    /* DIO0 = TxDone/RxDone */
    rfm_write_reg(REG_DIO_MAPPING_1, 0x00);
    
    /* Standby mode */
    rfm_set_mode(MODE_STDBY);
    
    SEGGER_RTT_printf(0, "RFM95: Initialized at %lu Hz, SF%d\n", 
                      LORA_FREQUENCY, LORA_SPREADING_FACTOR);
    
    return true;
}

static bool rfm_send(const uint8_t *data, uint8_t len)
{
    if (len > 255) return false;
    
    /* Standby mode */
    rfm_set_mode(MODE_STDBY);
    
    /* Set FIFO pointer to TX base */
    rfm_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    rfm_write_reg(REG_FIFO_TX_BASE, 0x00);
    
    /* Write data to FIFO */
    for (uint8_t i = 0; i < len; i++) {
        rfm_write_reg(REG_FIFO, data[i]);
    }
    
    /* Set payload length */
    rfm_write_reg(REG_PAYLOAD_LENGTH, len);
    
    /* Clear IRQ flags */
    rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
    
    /* Start TX */
    rfm_set_mode(MODE_TX);
    
    /* Wait for TX done (timeout 5 seconds) */
    uint32_t start = xTaskGetTickCount();
    while ((rfm_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE) == 0) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(5000)) {
            SEGGER_RTT_printf(0, "RFM95: TX timeout\n");
            rfm_set_mode(MODE_STDBY);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    /* Clear IRQ flags */
    rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
    
    /* Back to standby */
    rfm_set_mode(MODE_STDBY);
    
    return true;
}

static uint8_t rfm_receive(uint8_t *buffer, uint8_t max_len, int16_t *rssi, uint32_t timeout_ms)
{
    /* Set FIFO pointer to RX base */
    rfm_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    rfm_write_reg(REG_FIFO_RX_BASE, 0x00);
    
    /* Clear IRQ flags */
    rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
    
    /* Start RX */
    rfm_set_mode(MODE_RX_SINGLE);
    
    /* Wait for RX done or timeout */
    uint32_t start = xTaskGetTickCount();
    while ((rfm_read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE) == 0) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(timeout_ms)) {
            rfm_set_mode(MODE_STDBY);
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    /* Check CRC error */
    if (rfm_read_reg(REG_IRQ_FLAGS) & IRQ_PAYLOAD_CRC_ERROR) {
        rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
        rfm_set_mode(MODE_STDBY);
        return 0;
    }
    
    /* Get RSSI */
    if (rssi) {
        *rssi = rfm_read_reg(REG_PKT_RSSI) - 137;
    }
    
    /* Get payload length */
    uint8_t len = rfm_read_reg(REG_RX_NB_BYTES);
    if (len > max_len) len = max_len;
    
    /* Set FIFO pointer to current RX address */
    rfm_write_reg(REG_FIFO_ADDR_PTR, rfm_read_reg(REG_FIFO_RX_CURRENT));
    
    /* Read data from FIFO */
    for (uint8_t i = 0; i < len; i++) {
        buffer[i] = rfm_read_reg(REG_FIFO);
    }
    
    /* Clear IRQ flags */
    rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
    
    /* Back to standby */
    rfm_set_mode(MODE_STDBY);
    
    return len;
}

/* ==========================================================================
 * AGSYS PROTOCOL
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
    
    /* Build payload using canonical structure */
    agsys_meter_report_t *report = (agsys_meter_report_t *)(buffer + sizeof(agsys_header_t));
    
    report->timestamp = xTaskGetTickCount() / configTICK_RATE_HZ;  /* Uptime in seconds */
    report->total_pulses = 0;  /* Mag meter doesn't use pulses */
    report->total_liters = (uint32_t)total_volume_l;
    report->flow_rate_lpm = (uint16_t)(flow_rate_lpm * 10);  /* 0.1 L/min resolution */
    report->battery_mv = 0;  /* Mains powered, no battery */
    report->flags = alarm_flags;
    
    /* Firmware version and boot reason */
    report->fw_version[0] = FW_VERSION_MAJOR;
    report->fw_version[1] = FW_VERSION_MINOR;
    report->fw_version[2] = FW_VERSION_PATCH;
    report->boot_reason = m_boot_reason;
    
    uint8_t total_len = sizeof(agsys_header_t) + sizeof(agsys_meter_report_t);
    
    /* SPI mutex handled by agsys_spi driver */
    return rfm_send(buffer, total_len);
}

static void process_lora_message(const uint8_t *data, uint8_t len, int16_t rssi)
{
    if (len < sizeof(agsys_header_t)) return;
    
    agsys_header_t *hdr = (agsys_header_t *)data;
    
    /* Validate magic bytes */
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
                
                /* Check for OTA pending flag */
                if (ack->flags & AGSYS_ACK_FLAG_OTA_PENDING) {
                    SEGGER_RTT_printf(0, "LoRa: OTA update available, initiating OTA\n");
                    /* TODO: Initiate OTA request flow */
                }
                
                /* Clear boot reason after first successful report */
                if (m_boot_reason != AGSYS_BOOT_REASON_NORMAL) {
                    m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
                }
            } else {
                SEGGER_RTT_printf(0, "LoRa: ACK received\n");
            }
            break;
            
        case AGSYS_MSG_TIME_SYNC:
            if (payload_len >= 4) {
                uint32_t timestamp = (payload[0] << 24) | (payload[1] << 16) |
                                     (payload[2] << 8) | payload[3];
                SEGGER_RTT_printf(0, "Time sync: %u\n", timestamp);
                /* TODO: Set RTC */
            }
            break;
            
        case AGSYS_MSG_CONFIG_UPDATE:
            SEGGER_RTT_printf(0, "Config update received\n");
            /* TODO: Parse and apply config */
            break;
            
        case AGSYS_MSG_METER_RESET_TOTAL:
            SEGGER_RTT_printf(0, "Reset totalizer command received\n");
            /* TODO: Reset totalizer via callback */
            break;
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
    
    /* Initialize RFM95 */
    m_initialized = rfm_init();
    
    if (!m_initialized) {
        SEGGER_RTT_printf(0, "LoRa: Init failed, task exiting\n");
        vTaskDelete(NULL);
        return;
    }
    
    /* Get report interval from calibration data
     * LoRa interval = display_update_sec * lora_report_mult
     * Default: 15s * 4 = 60s
     */
    uint8_t display_sec = g_calibration.display_update_sec;
    uint8_t lora_mult = g_calibration.lora_report_mult;
    
    /* Use defaults if not configured (0 means use default) */
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
            
            /* Get current flow data */
            float flow_rate = g_flow_rate_lpm;
            float total_vol = g_total_volume_l;
            uint8_t alarms = g_alarm_flags;
            
            SEGGER_RTT_printf(0, "LoRa: Sending report (flow=%.1f L/min, total=%.1f L)\n",
                              flow_rate, total_vol);
            
            /* Turn on LoRa LED */
            nrf_gpio_pin_set(LED_LORA_PIN);
            
            bool tx_success = send_meter_report(flow_rate, total_vol, alarms);
            
            if (tx_success) {
                SEGGER_RTT_printf(0, "LoRa: TX success\n");
                
                /* Wait for ACK */
                uint8_t rx_buf[64];
                int16_t rssi;
                uint8_t rx_len = rfm_receive(rx_buf, sizeof(rx_buf), &rssi, 2000);
                
                if (rx_len > 0) {
                    process_lora_message(rx_buf, rx_len, rssi);
                }
            } else {
                SEGGER_RTT_printf(0, "LoRa: TX failed\n");
                
                /* Log to flash for later sync */
                uint32_t flow_mlpm = (uint32_t)(flow_rate * 1000);
                uint32_t total_ml = (uint32_t)(total_vol * 1000);
                agsys_device_log_meter(&m_device_ctx, flow_mlpm, total_ml, alarms);
            }
            
            /* Turn off LoRa LED */
            nrf_gpio_pin_clear(LED_LORA_PIN);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ==========================================================================
 * BOOT REASON AND OTA STATE
 * ========================================================================== */

static void load_boot_reason_from_fram(void)
{
    agsys_ota_fram_state_t ota_state;
    
    if (agsys_fram_read(&m_fram_ctx, AGSYS_FRAM_OTA_STATE_ADDR, 
                        (uint8_t *)&ota_state, sizeof(ota_state)) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "LoRa: Failed to read OTA state from FRAM\n");
        return;
    }
    
    /* Check if OTA state is valid */
    if (ota_state.magic != AGSYS_OTA_FRAM_MAGIC) {
        m_boot_reason = AGSYS_BOOT_REASON_NORMAL;
        return;
    }
    
    /* Determine boot reason based on OTA state */
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
        agsys_fram_write(&m_fram_ctx, AGSYS_FRAM_OTA_STATE_ADDR,
                         (uint8_t *)&ota_state, sizeof(ota_state));
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

void lora_task_init(void)
{
    /* Configure LED */
    nrf_gpio_cfg_output(LED_LORA_PIN);
    nrf_gpio_pin_clear(LED_LORA_PIN);
    
    /* Load boot reason from FRAM OTA state */
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
