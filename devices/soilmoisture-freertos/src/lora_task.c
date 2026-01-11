/**
 * @file lora_task.c
 * @brief LoRa task implementation for Soil Moisture Sensor
 * 
 * Handles RFM95C communication with property controller using AgSys protocol.
 * Uses the common freertos-common library for protocol and encryption.
 * Implements channel hopping and exponential backoff for collision avoidance.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "SEGGER_RTT.h"

#include "lora_task.h"
#include "spi_driver.h"
#include "board_config.h"
#include "agsys_device.h"
#include "agsys_protocol.h"

#include <string.h>

/* External device context for logging */
extern agsys_device_ctx_t m_device_ctx;

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

static uint8_t get_random_channel(void)
{
    /* Use hardware RNG for channel selection */
    NRF_RNG->TASKS_START = 1;
    while (NRF_RNG->EVENTS_VALRDY == 0);
    uint8_t rnd = NRF_RNG->VALUE;
    NRF_RNG->EVENTS_VALRDY = 0;
    NRF_RNG->TASKS_STOP = 1;
    
    return rnd % LORA_NUM_CHANNELS;
}

static void rfm_hop_channel(void)
{
    uint8_t channel = get_random_channel();
    uint32_t freq = LORA_BASE_FREQ + (channel * LORA_CHANNEL_STEP);
    rfm_set_frequency(freq);
    SEGGER_RTT_printf(0, "LoRa: Channel %d (%lu Hz)\n", channel, freq);
}

static void rfm_init(void)
{
    /* Reset */
    nrf_gpio_cfg_output(LORA_RESET_PIN);
    nrf_gpio_pin_clear(LORA_RESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(10));
    nrf_gpio_pin_set(LORA_RESET_PIN);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Check version */
    uint8_t version = rfm_read_reg(REG_VERSION);
    SEGGER_RTT_printf(0, "RFM95 version: 0x%02X\n", version);
    
    /* Sleep mode for configuration */
    rfm_set_mode(MODE_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Set frequency */
    rfm_set_frequency(LORA_FREQUENCY);
    
    /* Configure modem: BW=125kHz, CR=4/5, explicit header */
    rfm_write_reg(REG_MODEM_CONFIG_1, 0x72);
    
    /* SF=10, CRC on */
    rfm_write_reg(REG_MODEM_CONFIG_2, (LORA_SPREADING_FACTOR << 4) | 0x04);
    
    /* LNA gain auto, low data rate optimize on for SF10 */
    rfm_write_reg(REG_MODEM_CONFIG_3, 0x04);
    
    /* TX power +20dBm */
    rfm_write_reg(REG_PA_CONFIG, 0x8F);
    rfm_write_reg(REG_PA_DAC, 0x87);
    
    /* Preamble length 8 */
    rfm_write_reg(REG_PREAMBLE_MSB, 0x00);
    rfm_write_reg(REG_PREAMBLE_LSB, 0x08);
    
    /* Sync word */
    rfm_write_reg(REG_SYNC_WORD, LORA_SYNC_WORD);
    
    /* FIFO pointers */
    rfm_write_reg(REG_FIFO_TX_BASE, 0x00);
    rfm_write_reg(REG_FIFO_RX_BASE, 0x00);
    
    /* DIO0 = RxDone/TxDone */
    rfm_write_reg(REG_DIO_MAPPING_1, 0x00);
    
    /* Standby mode */
    rfm_set_mode(MODE_STDBY);
    
    m_initialized = true;
    SEGGER_RTT_printf(0, "RFM95 initialized at %lu Hz, SF%d\n", 
                      LORA_FREQUENCY, LORA_SPREADING_FACTOR);
}

static bool rfm_send(const uint8_t *data, uint8_t len)
{
    if (len > 255) return false;
    
    rfm_set_mode(MODE_STDBY);
    
    /* Set FIFO pointer */
    rfm_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    
    /* Write data to FIFO */
    spi_cs_assert(SPI_CS_LORA_PIN);
    uint8_t cmd = REG_FIFO | 0x80;
    spi_transfer_raw(&cmd, NULL, 1);
    spi_transfer_raw(data, NULL, len);
    spi_cs_deassert(SPI_CS_LORA_PIN);
    
    /* Set payload length */
    rfm_write_reg(REG_PAYLOAD_LENGTH, len);
    
    /* Clear IRQ flags */
    rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
    
    /* Start TX */
    rfm_set_mode(MODE_TX);
    
    /* Wait for TX done (with timeout) */
    for (int i = 0; i < 500; i++) {  /* 5 second timeout for SF10 */
        if (rfm_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE) {
            rfm_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE);
            rfm_set_mode(MODE_STDBY);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    SEGGER_RTT_printf(0, "LoRa TX timeout\n");
    rfm_set_mode(MODE_STDBY);
    return false;
}

static int rfm_receive(uint8_t *data, uint8_t max_len, int16_t *rssi, uint32_t timeout_ms)
{
    rfm_set_mode(MODE_STDBY);
    rfm_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    rfm_write_reg(REG_IRQ_FLAGS, 0xFF);
    rfm_set_mode(MODE_RX_SINGLE);
    
    uint32_t start = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        uint8_t irq = rfm_read_reg(REG_IRQ_FLAGS);
        
        if (irq & IRQ_RX_DONE) {
            rfm_write_reg(REG_IRQ_FLAGS, IRQ_RX_DONE | IRQ_PAYLOAD_CRC_ERROR);
            
            if (irq & IRQ_PAYLOAD_CRC_ERROR) {
                SEGGER_RTT_printf(0, "LoRa CRC error\n");
                return -1;
            }
            
            uint8_t len = rfm_read_reg(REG_RX_NB_BYTES);
            if (len > max_len) len = max_len;
            
            rfm_write_reg(REG_FIFO_ADDR_PTR, rfm_read_reg(REG_FIFO_RX_CURRENT));
            
            spi_cs_assert(SPI_CS_LORA_PIN);
            uint8_t cmd = REG_FIFO & 0x7F;
            spi_transfer_raw(&cmd, NULL, 1);
            spi_transfer_raw(NULL, data, len);
            spi_cs_deassert(SPI_CS_LORA_PIN);
            
            if (rssi) {
                *rssi = rfm_read_reg(REG_PKT_RSSI) - 137;
            }
            
            rfm_set_mode(MODE_STDBY);
            return len;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    rfm_set_mode(MODE_STDBY);
    return 0;  /* Timeout */
}

/* ==========================================================================
 * AGSYS PROTOCOL - Uses common freertos-common types
 * ========================================================================== */

static uint8_t build_sensor_report(uint8_t *buffer, size_t max_len,
                                    const uint8_t *device_uid,
                                    uint32_t probe_freqs[4],
                                    uint8_t probe_moisture[4],
                                    uint16_t battery_mv,
                                    uint8_t flags)
{
    size_t total_len = sizeof(agsys_header_t) + sizeof(agsys_soil_report_t);
    if (max_len < total_len) return 0;
    
    /* Build header using common protocol format */
    agsys_header_t *hdr = (agsys_header_t *)buffer;
    hdr->magic[0] = AGSYS_MAGIC_BYTE1;
    hdr->magic[1] = AGSYS_MAGIC_BYTE2;
    hdr->version = AGSYS_PROTOCOL_VERSION;
    hdr->msg_type = AGSYS_MSG_SOIL_REPORT;
    hdr->device_type = AGSYS_DEVICE_TYPE_SOIL_MOISTURE;
    memcpy(hdr->device_uid, device_uid, AGSYS_DEVICE_UID_SIZE);
    hdr->sequence = m_sequence++;
    
    /* Build payload using common protocol format */
    agsys_soil_report_t *report = (agsys_soil_report_t *)(buffer + sizeof(agsys_header_t));
    report->timestamp = 0;  /* TODO: Add uptime tracking */
    report->probe_count = NUM_MOISTURE_PROBES;
    report->battery_mv = battery_mv;
    report->temperature = 0;  /* TODO: Add temperature sensor */
    report->pending_logs = (uint8_t)agsys_device_log_pending_count(&m_device_ctx);
    report->flags = flags;
    
    /* Fill probe readings */
    for (int i = 0; i < AGSYS_MAX_PROBES; i++) {
        if (i < NUM_MOISTURE_PROBES) {
            report->probes[i].probe_index = i;
            report->probes[i].frequency_hz = (uint16_t)(probe_freqs[i] / 100);  /* Scale to 16-bit */
            report->probes[i].moisture_percent = probe_moisture[i];
        } else {
            report->probes[i].probe_index = i;
            report->probes[i].frequency_hz = 0;
            report->probes[i].moisture_percent = 0;
        }
    }
    
    return (uint8_t)total_len;
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool lora_task_init(void)
{
    return true;
}

void lora_task(void *pvParameters)
{
    (void)pvParameters;
    m_task_handle = xTaskGetCurrentTaskHandle();
    
    SEGGER_RTT_printf(0, "LoRa task started\n");
    
    /* Initialize SPI */
    spi_init();
    
    /* Initialize RFM95 */
    if (spi_acquire(pdMS_TO_TICKS(1000))) {
        rfm_init();
        spi_release();
    }
    
    for (;;) {
        /* Wait for sensor data ready notification */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        SEGGER_RTT_printf(0, "LoRa: Preparing to transmit\n");
        
        /* TODO: Get actual sensor data from sensor task */
        /* For now, this is a placeholder - actual implementation would
         * receive data via queue or shared memory */
        
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
    
    /* Exponential backoff retry loop */
    uint32_t backoff_ms = 1000;
    
    for (int retry = 0; retry < LORA_MAX_RETRIES; retry++) {
        if (!spi_acquire(pdMS_TO_TICKS(1000))) {
            continue;
        }
        
        /* Hop to random channel */
        rfm_hop_channel();
        
        /* Add random jitter before TX */
        uint32_t jitter = (get_random_channel() * 31) % 500;  /* 0-500ms */
        vTaskDelay(pdMS_TO_TICKS(jitter));
        
        SEGGER_RTT_printf(0, "LoRa: TX attempt %d\n", retry + 1);
        
        if (rfm_send(buffer, len)) {
            /* Wait for ACK */
            uint8_t rx_buf[32];
            int16_t rssi;
            int rx_len = rfm_receive(rx_buf, sizeof(rx_buf), &rssi, LORA_ACK_TIMEOUT_MS);
            
            spi_release();
            
            if (rx_len > 0) {
                agsys_header_t *hdr = (agsys_header_t *)rx_buf;
                if (hdr->magic[0] == AGSYS_MAGIC_BYTE1 && hdr->magic[1] == AGSYS_MAGIC_BYTE2 &&
                    hdr->msg_type == AGSYS_MSG_ACK) {
                    SEGGER_RTT_printf(0, "LoRa: ACK received (RSSI=%d)\n", rssi);
                    
                    /* Sync any pending logs on successful TX */
                    uint32_t pending = agsys_device_log_pending_count(&m_device_ctx);
                    if (pending > 0) {
                        SEGGER_RTT_printf(0, "LoRa: %lu pending logs to sync\n", pending);
                        /* TODO: Send pending logs to property controller */
                    }
                    
                    return true;
                }
            }
            
            SEGGER_RTT_printf(0, "LoRa: No ACK, retry in %lu ms\n", backoff_ms);
        } else {
            spi_release();
        }
        
        /* Exponential backoff with jitter */
        uint32_t jitter_backoff = backoff_ms + (backoff_ms * (get_random_channel() % 50) / 100);
        vTaskDelay(pdMS_TO_TICKS(jitter_backoff));
        backoff_ms *= 2;
        if (backoff_ms > 60000) backoff_ms = 60000;
    }
    
    /* TX failed - log reading to flash for later sync */
    SEGGER_RTT_printf(0, "LoRa: TX failed after %d retries, logging to flash\n", LORA_MAX_RETRIES);
    
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
    
    if (spi_acquire(pdMS_TO_TICKS(100))) {
        rfm_set_mode(MODE_SLEEP);
        spi_release();
    }
    SEGGER_RTT_printf(0, "LoRa: Sleep\n");
}

void lora_wake(void)
{
    if (!m_initialized) return;
    
    if (spi_acquire(pdMS_TO_TICKS(100))) {
        rfm_set_mode(MODE_STDBY);
        spi_release();
    }
    SEGGER_RTT_printf(0, "LoRa: Wake\n");
}
