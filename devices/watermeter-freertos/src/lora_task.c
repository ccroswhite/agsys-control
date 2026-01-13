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

/* External SPI mutex */
extern SemaphoreHandle_t g_spi_mutex;

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

/* ==========================================================================
 * SPI HELPERS
 * ========================================================================== */

static bool spi_acquire(TickType_t timeout)
{
    if (g_spi_mutex == NULL) return true;
    return xSemaphoreTake(g_spi_mutex, timeout) == pdTRUE;
}

static void spi_release(void)
{
    if (g_spi_mutex != NULL) {
        xSemaphoreGive(g_spi_mutex);
    }
}

static void spi_transfer(uint8_t cs_pin, const uint8_t *tx, uint8_t *rx, size_t len)
{
    nrf_gpio_pin_clear(cs_pin);
    
    for (size_t i = 0; i < len; i++) {
        /* Bit-bang SPI - simplified for now */
        uint8_t tx_byte = tx ? tx[i] : 0x00;
        uint8_t rx_byte = 0;
        
        for (int bit = 7; bit >= 0; bit--) {
            /* Set MOSI */
            if (tx_byte & (1 << bit)) {
                nrf_gpio_pin_set(SPI2_MOSI_PIN);
            } else {
                nrf_gpio_pin_clear(SPI2_MOSI_PIN);
            }
            
            /* Clock high */
            nrf_gpio_pin_set(SPI2_SCK_PIN);
            
            /* Read MISO */
            if (nrf_gpio_pin_read(SPI2_MISO_PIN)) {
                rx_byte |= (1 << bit);
            }
            
            /* Clock low */
            nrf_gpio_pin_clear(SPI2_SCK_PIN);
        }
        
        if (rx) rx[i] = rx_byte;
    }
    
    nrf_gpio_pin_set(cs_pin);
}

/* ==========================================================================
 * RFM95C LOW-LEVEL FUNCTIONS
 * ========================================================================== */

static void rfm_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { reg | 0x80, value };
    spi_transfer(SPI_CS_LORA_PIN, tx, NULL, 2);
}

static uint8_t rfm_read_reg(uint8_t reg)
{
    uint8_t tx[2] = { reg & 0x7F, 0x00 };
    uint8_t rx[2];
    spi_transfer(SPI_CS_LORA_PIN, tx, rx, 2);
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
    /* Configure SPI pins */
    nrf_gpio_cfg_output(SPI2_SCK_PIN);
    nrf_gpio_cfg_output(SPI2_MOSI_PIN);
    nrf_gpio_cfg_input(SPI2_MISO_PIN, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_output(SPI_CS_LORA_PIN);
    nrf_gpio_pin_set(SPI_CS_LORA_PIN);
    
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
    report->total_pulses = 0;  /* TODO: Track pulse count */
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
    
    bool success = false;
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        success = rfm_send(buffer, total_len);
        spi_release();
    }
    
    return success;
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
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        m_initialized = rfm_init();
        spi_release();
    }
    
    if (!m_initialized) {
        SEGGER_RTT_printf(0, "LoRa: Init failed, task exiting\n");
        vTaskDelete(NULL);
        return;
    }
    
    uint32_t report_interval_ms = LORA_REPORT_INTERVAL_S * 1000;
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
                
                if (spi_acquire(pdMS_TO_TICKS(1000))) {
                    uint8_t rx_len = rfm_receive(rx_buf, sizeof(rx_buf), &rssi, 2000);
                    spi_release();
                    
                    if (rx_len > 0) {
                        process_lora_message(rx_buf, rx_len, rssi);
                    }
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
