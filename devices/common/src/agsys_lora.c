/**
 * @file agsys_lora.c
 * @brief LoRa radio driver for RFM95/SX1276
 * 
 * Provides FreeRTOS-aware LoRa communication with mutex-protected SPI access
 * and automatic logging on TX failure.
 */

#include "agsys_lora.h"
#include "agsys_device.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* External device context for logging */
extern agsys_device_ctx_t m_device_ctx;

/* ==========================================================================
 * RFM95/SX1276 REGISTER DEFINITIONS
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
 * LOW-LEVEL SPI HELPERS
 * ========================================================================== */

static void lora_write_reg(agsys_lora_ctx_t *ctx, uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { reg | 0x80, value };
    agsys_spi_xfer_t xfer = { .tx_buf = tx, .rx_buf = NULL, .length = 2 };
    agsys_spi_transfer(ctx->spi_handle, &xfer);
}

static uint8_t lora_read_reg(agsys_lora_ctx_t *ctx, uint8_t reg)
{
    uint8_t tx[2] = { reg & 0x7F, 0x00 };
    uint8_t rx[2] = {0};
    agsys_spi_xfer_t xfer = { .tx_buf = tx, .rx_buf = rx, .length = 2 };
    agsys_spi_transfer(ctx->spi_handle, &xfer);
    return rx[1];
}

static void lora_set_mode(agsys_lora_ctx_t *ctx, uint8_t mode)
{
    lora_write_reg(ctx, REG_OP_MODE, MODE_LORA | mode);
}

static void lora_set_frequency(agsys_lora_ctx_t *ctx, uint32_t freq)
{
    uint64_t frf = ((uint64_t)freq << 19) / 32000000;
    lora_write_reg(ctx, REG_FRF_MSB, (frf >> 16) & 0xFF);
    lora_write_reg(ctx, REG_FRF_MID, (frf >> 8) & 0xFF);
    lora_write_reg(ctx, REG_FRF_LSB, frf & 0xFF);
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

agsys_err_t agsys_lora_init(agsys_lora_ctx_t *ctx,
                             uint8_t cs_pin,
                             uint8_t rst_pin,
                             uint8_t dio0_pin,
                             agsys_spi_bus_t bus,
                             const agsys_lora_config_t *config)
{
    if (ctx == NULL || config == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    memset(ctx, 0, sizeof(agsys_lora_ctx_t));
    ctx->rst_pin = rst_pin;
    ctx->dio0_pin = dio0_pin;
    memcpy(&ctx->config, config, sizeof(agsys_lora_config_t));
    
    /* Register with SPI manager on specified bus */
    agsys_spi_config_t spi_config = {
        .cs_pin = cs_pin,
        .cs_active_low = true,
        .frequency = NRF_SPIM_FREQ_4M,
        .mode = 0,
        .bus = bus,
    };
    
    agsys_err_t err = agsys_spi_register(&spi_config, &ctx->spi_handle);
    if (err != AGSYS_OK) {
        return err;
    }
    
    /* Hardware reset */
    nrf_gpio_cfg_output(rst_pin);
    nrf_gpio_pin_clear(rst_pin);
    vTaskDelay(pdMS_TO_TICKS(10));
    nrf_gpio_pin_set(rst_pin);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Verify chip version */
    uint8_t version = lora_read_reg(ctx, REG_VERSION);
    if (version != 0x12) {
        SEGGER_RTT_printf(0, "LoRa: Invalid version 0x%02X (expected 0x12)\n", version);
        return AGSYS_ERR_SPI;
    }
    
    /* Enter sleep mode for configuration */
    lora_set_mode(ctx, MODE_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    /* Set frequency */
    lora_set_frequency(ctx, config->frequency);
    
    /* Configure modem */
    uint8_t bw_reg = 0x70;  /* 125kHz default */
    if (config->bandwidth >= 250000) bw_reg = 0x80;
    if (config->bandwidth >= 500000) bw_reg = 0x90;
    
    uint8_t cr_reg = ((config->coding_rate - 4) & 0x07) << 1;
    lora_write_reg(ctx, REG_MODEM_CONFIG_1, bw_reg | cr_reg | 0x00);  /* Explicit header */
    
    /* SF and CRC */
    uint8_t sf_reg = (config->spreading_factor << 4) | (config->crc_enabled ? 0x04 : 0x00);
    lora_write_reg(ctx, REG_MODEM_CONFIG_2, sf_reg);
    
    /* LNA gain auto, low data rate optimize for SF >= 10 */
    uint8_t ldr = (config->spreading_factor >= 10) ? 0x08 : 0x00;
    lora_write_reg(ctx, REG_MODEM_CONFIG_3, 0x04 | ldr);
    
    /* TX power */
    if (config->tx_power >= 20) {
        lora_write_reg(ctx, REG_PA_CONFIG, 0x8F);
        lora_write_reg(ctx, REG_PA_DAC, 0x87);
    } else {
        uint8_t pa = (config->tx_power - 2) & 0x0F;
        lora_write_reg(ctx, REG_PA_CONFIG, 0x80 | pa);
        lora_write_reg(ctx, REG_PA_DAC, 0x84);
    }
    
    /* Preamble length 8 */
    lora_write_reg(ctx, REG_PREAMBLE_MSB, 0x00);
    lora_write_reg(ctx, REG_PREAMBLE_LSB, 0x08);
    
    /* Sync word (AgSys private) */
    lora_write_reg(ctx, REG_SYNC_WORD, 0x34);
    
    /* FIFO pointers */
    lora_write_reg(ctx, REG_FIFO_TX_BASE, 0x00);
    lora_write_reg(ctx, REG_FIFO_RX_BASE, 0x00);
    
    /* DIO0 = RxDone/TxDone */
    lora_write_reg(ctx, REG_DIO_MAPPING_1, 0x00);
    
    /* Standby mode */
    lora_set_mode(ctx, MODE_STDBY);
    
    ctx->initialized = true;
    SEGGER_RTT_printf(0, "LoRa: Initialized at %lu Hz, SF%d\n", 
                      config->frequency, config->spreading_factor);
    
    return AGSYS_OK;
}

void agsys_lora_deinit(agsys_lora_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    
    lora_set_mode(ctx, MODE_SLEEP);
    agsys_spi_unregister(ctx->spi_handle);
    ctx->initialized = false;
}

/* ==========================================================================
 * TRANSMIT / RECEIVE
 * ========================================================================== */

agsys_err_t agsys_lora_transmit(agsys_lora_ctx_t *ctx,
                                 const uint8_t *data,
                                 size_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL || len == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    if (len > AGSYS_LORA_MAX_PACKET_SIZE) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    lora_set_mode(ctx, MODE_STDBY);
    
    /* Set FIFO pointer */
    lora_write_reg(ctx, REG_FIFO_ADDR_PTR, 0x00);
    
    /* Write data to FIFO */
    uint8_t cmd = REG_FIFO | 0x80;
    agsys_spi_xfer_t xfers[2] = {
        { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 },
        { .tx_buf = data, .rx_buf = NULL, .length = len }
    };
    agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2);
    
    /* Set payload length */
    lora_write_reg(ctx, REG_PAYLOAD_LENGTH, len);
    
    /* Clear IRQ flags */
    lora_write_reg(ctx, REG_IRQ_FLAGS, 0xFF);
    
    /* Start TX */
    lora_set_mode(ctx, MODE_TX);
    
    /* Wait for TX done (timeout based on SF) */
    uint32_t timeout_ms = 5000;  /* 5 seconds max for SF12 */
    TickType_t start = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        if (lora_read_reg(ctx, REG_IRQ_FLAGS) & IRQ_TX_DONE) {
            lora_write_reg(ctx, REG_IRQ_FLAGS, IRQ_TX_DONE);
            lora_set_mode(ctx, MODE_STDBY);
            return AGSYS_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    SEGGER_RTT_printf(0, "LoRa: TX timeout\n");
    lora_set_mode(ctx, MODE_STDBY);
    return AGSYS_ERR_TIMEOUT;
}

agsys_err_t agsys_lora_receive_start(agsys_lora_ctx_t *ctx,
                                      agsys_lora_rx_callback_t callback)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    ctx->rx_callback = callback;
    
    lora_set_mode(ctx, MODE_STDBY);
    lora_write_reg(ctx, REG_FIFO_ADDR_PTR, 0x00);
    lora_write_reg(ctx, REG_IRQ_FLAGS, 0xFF);
    lora_set_mode(ctx, MODE_RX_CONTINUOUS);
    
    return AGSYS_OK;
}

agsys_err_t agsys_lora_receive_stop(agsys_lora_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    lora_set_mode(ctx, MODE_STDBY);
    ctx->rx_callback = NULL;
    
    return AGSYS_OK;
}

int agsys_lora_receive(agsys_lora_ctx_t *ctx,
                        uint8_t *data,
                        size_t max_len,
                        int16_t *rssi,
                        int8_t *snr,
                        uint32_t timeout_ms)
{
    if (ctx == NULL || !ctx->initialized || data == NULL || max_len == 0) {
        return -1;
    }
    
    lora_set_mode(ctx, MODE_STDBY);
    lora_write_reg(ctx, REG_FIFO_ADDR_PTR, 0x00);
    lora_write_reg(ctx, REG_IRQ_FLAGS, 0xFF);
    lora_set_mode(ctx, MODE_RX_SINGLE);
    
    TickType_t start = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        uint8_t irq = lora_read_reg(ctx, REG_IRQ_FLAGS);
        
        if (irq & IRQ_RX_DONE) {
            lora_write_reg(ctx, REG_IRQ_FLAGS, IRQ_RX_DONE | IRQ_PAYLOAD_CRC_ERROR);
            
            if (irq & IRQ_PAYLOAD_CRC_ERROR) {
                SEGGER_RTT_printf(0, "LoRa: RX CRC error\n");
                lora_set_mode(ctx, MODE_STDBY);
                return -1;
            }
            
            uint8_t len = lora_read_reg(ctx, REG_RX_NB_BYTES);
            if (len > max_len) len = max_len;
            
            lora_write_reg(ctx, REG_FIFO_ADDR_PTR, lora_read_reg(ctx, REG_FIFO_RX_CURRENT));
            
            /* Read FIFO */
            uint8_t cmd = REG_FIFO & 0x7F;
            agsys_spi_xfer_t xfers[2] = {
                { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 },
                { .tx_buf = NULL, .rx_buf = data, .length = len }
            };
            agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2);
            
            if (rssi) {
                *rssi = lora_read_reg(ctx, REG_PKT_RSSI) - 137;
            }
            if (snr) {
                *snr = (int8_t)lora_read_reg(ctx, REG_PKT_SNR) / 4;
            }
            
            lora_set_mode(ctx, MODE_STDBY);
            return len;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    lora_set_mode(ctx, MODE_STDBY);
    return 0;  /* Timeout */
}

agsys_err_t agsys_lora_sleep(agsys_lora_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    lora_set_mode(ctx, MODE_SLEEP);
    return AGSYS_OK;
}

/* ==========================================================================
 * UTILITIES
 * ========================================================================== */

int16_t agsys_lora_get_rssi(agsys_lora_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return -255;
    }
    return lora_read_reg(ctx, REG_PKT_RSSI) - 137;
}

int8_t agsys_lora_get_snr(agsys_lora_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return -128;
    }
    return (int8_t)lora_read_reg(ctx, REG_PKT_SNR) / 4;
}

/* ==========================================================================
 * HIGH-LEVEL TX WITH RETRY AND LOGGING
 * ========================================================================== */

static uint8_t get_random_byte(void)
{
    NRF_RNG->TASKS_START = 1;
    while (NRF_RNG->EVENTS_VALRDY == 0);
    uint8_t rnd = NRF_RNG->VALUE;
    NRF_RNG->EVENTS_VALRDY = 0;
    NRF_RNG->TASKS_STOP = 1;
    return rnd;
}

agsys_lora_tx_result_t agsys_lora_tx_with_retry(agsys_lora_ctx_t *ctx,
                                                  const uint8_t *data,
                                                  size_t len,
                                                  uint8_t max_retries,
                                                  uint32_t ack_timeout_ms)
{
    if (ctx == NULL || !ctx->initialized || data == NULL || len == 0) {
        return AGSYS_LORA_TX_FAILED;
    }
    
    uint32_t backoff_ms = 1000;
    
    for (uint8_t retry = 0; retry <= max_retries; retry++) {
        /* Add random jitter before TX */
        uint32_t jitter = (get_random_byte() * 2);  /* 0-510ms */
        vTaskDelay(pdMS_TO_TICKS(jitter));
        
        SEGGER_RTT_printf(0, "LoRa: TX attempt %d/%d\n", retry + 1, max_retries + 1);
        
        agsys_err_t err = agsys_lora_transmit(ctx, data, len);
        if (err != AGSYS_OK) {
            SEGGER_RTT_printf(0, "LoRa: TX failed (err=%d)\n", err);
            continue;
        }
        
        /* If no ACK expected, we're done */
        if (ack_timeout_ms == 0) {
            return AGSYS_LORA_TX_SUCCESS;
        }
        
        /* Wait for ACK */
        lora_set_mode(ctx, MODE_RX_SINGLE);
        TickType_t start = xTaskGetTickCount();
        
        while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(ack_timeout_ms)) {
            uint8_t irq = lora_read_reg(ctx, REG_IRQ_FLAGS);
            
            if (irq & IRQ_RX_DONE) {
                lora_write_reg(ctx, REG_IRQ_FLAGS, IRQ_RX_DONE | IRQ_PAYLOAD_CRC_ERROR);
                
                if (irq & IRQ_PAYLOAD_CRC_ERROR) {
                    SEGGER_RTT_printf(0, "LoRa: ACK CRC error\n");
                    break;
                }
                
                /* ACK received - check pending logs */
                uint32_t pending = agsys_lora_check_pending_logs();
                if (pending > 0) {
                    SEGGER_RTT_printf(0, "LoRa: %lu pending logs to sync\n", pending);
                }
                
                lora_set_mode(ctx, MODE_STDBY);
                return AGSYS_LORA_TX_SUCCESS;
            }
            
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        lora_set_mode(ctx, MODE_STDBY);
        SEGGER_RTT_printf(0, "LoRa: No ACK, retry in %lu ms\n", backoff_ms);
        
        /* Exponential backoff with jitter */
        uint32_t jitter_backoff = backoff_ms + (backoff_ms * (get_random_byte() % 50) / 100);
        vTaskDelay(pdMS_TO_TICKS(jitter_backoff));
        backoff_ms *= 2;
        if (backoff_ms > 60000) backoff_ms = 60000;
    }
    
    /* All retries failed - data will be logged by caller based on device type */
    SEGGER_RTT_printf(0, "LoRa: TX failed after %d retries\n", max_retries + 1);
    return AGSYS_LORA_TX_NO_ACK;
}

uint32_t agsys_lora_check_pending_logs(void)
{
    return agsys_device_log_pending_count(&m_device_ctx);
}

bool agsys_lora_mark_log_synced(void)
{
    return agsys_device_log_mark_synced(&m_device_ctx);
}
