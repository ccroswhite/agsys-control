/**
 * @file agsys_lora.h
 * @brief LoRa radio driver for RFM95/SX1276
 * 
 * Provides FreeRTOS-aware LoRa communication with mutex-protected SPI access.
 */

#ifndef AGSYS_LORA_H
#define AGSYS_LORA_H

#include "agsys_common.h"
#include "agsys_spi.h"

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

#ifndef AGSYS_LORA_MAX_PACKET_SIZE
#define AGSYS_LORA_MAX_PACKET_SIZE  255
#endif

/* ==========================================================================
 * TYPES
 * ========================================================================== */

/**
 * @brief LoRa radio configuration
 */
typedef struct {
    uint32_t    frequency;          /* Frequency in Hz (e.g., 915000000) */
    uint8_t     spreading_factor;   /* 6-12 */
    uint32_t    bandwidth;          /* Hz (7800-500000) */
    uint8_t     coding_rate;        /* 5-8 (4/5 to 4/8) */
    int8_t      tx_power;           /* dBm (2-20) */
    bool        crc_enabled;        /* Enable CRC */
} agsys_lora_config_t;

/**
 * @brief LoRa receive callback
 */
typedef void (*agsys_lora_rx_callback_t)(const uint8_t *data, size_t len, int16_t rssi, int8_t snr);

/**
 * @brief LoRa context
 */
typedef struct {
    agsys_spi_handle_t  spi_handle;
    uint8_t             rst_pin;
    uint8_t             dio0_pin;
    agsys_lora_config_t config;
    agsys_lora_rx_callback_t rx_callback;
    TaskHandle_t        irq_task;
    bool                initialized;
} agsys_lora_ctx_t;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize the LoRa radio
 * 
 * @param ctx       LoRa context
 * @param cs_pin    SPI chip select pin
 * @param rst_pin   Reset pin
 * @param dio0_pin  DIO0 interrupt pin
 * @param config    Radio configuration
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_lora_init(agsys_lora_ctx_t *ctx,
                             uint8_t cs_pin,
                             uint8_t rst_pin,
                             uint8_t dio0_pin,
                             const agsys_lora_config_t *config);

/**
 * @brief Deinitialize the LoRa radio
 * 
 * @param ctx       LoRa context
 */
void agsys_lora_deinit(agsys_lora_ctx_t *ctx);

/* ==========================================================================
 * TRANSMIT / RECEIVE
 * ========================================================================== */

/**
 * @brief Transmit a packet (blocking)
 * 
 * @param ctx       LoRa context
 * @param data      Data to transmit
 * @param len       Data length
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_lora_transmit(agsys_lora_ctx_t *ctx,
                                 const uint8_t *data,
                                 size_t len);

/**
 * @brief Set receive callback and enter RX mode
 * 
 * @param ctx       LoRa context
 * @param callback  Callback for received packets
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_lora_receive_start(agsys_lora_ctx_t *ctx,
                                      agsys_lora_rx_callback_t callback);

/**
 * @brief Stop receiving and enter standby
 * 
 * @param ctx       LoRa context
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_lora_receive_stop(agsys_lora_ctx_t *ctx);

/**
 * @brief Enter sleep mode (lowest power)
 * 
 * @param ctx       LoRa context
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_lora_sleep(agsys_lora_ctx_t *ctx);

/* ==========================================================================
 * UTILITIES
 * ========================================================================== */

/**
 * @brief Get last packet RSSI
 * 
 * @param ctx       LoRa context
 * @return RSSI in dBm
 */
int16_t agsys_lora_get_rssi(agsys_lora_ctx_t *ctx);

/**
 * @brief Get last packet SNR
 * 
 * @param ctx       LoRa context
 * @return SNR in dB
 */
int8_t agsys_lora_get_snr(agsys_lora_ctx_t *ctx);

/* ==========================================================================
 * HIGH-LEVEL TX WITH RETRY AND LOGGING
 * ========================================================================== */

/**
 * @brief TX result codes
 */
typedef enum {
    AGSYS_LORA_TX_SUCCESS = 0,      /**< TX successful, ACK received */
    AGSYS_LORA_TX_NO_ACK,           /**< TX sent but no ACK received */
    AGSYS_LORA_TX_FAILED,           /**< TX failed (hardware error) */
    AGSYS_LORA_TX_LOGGED,           /**< TX failed, data logged to flash */
} agsys_lora_tx_result_t;

/**
 * @brief Transmit with retry and automatic logging on failure
 * 
 * Attempts to send data with exponential backoff. If all retries fail,
 * automatically logs the data to flash for later sync.
 * 
 * @param ctx           LoRa context
 * @param data          Data to transmit
 * @param len           Data length
 * @param max_retries   Maximum retry attempts (0 = no retry)
 * @param ack_timeout_ms Timeout waiting for ACK (0 = no ACK expected)
 * @return TX result code
 */
agsys_lora_tx_result_t agsys_lora_tx_with_retry(agsys_lora_ctx_t *ctx,
                                                  const uint8_t *data,
                                                  size_t len,
                                                  uint8_t max_retries,
                                                  uint32_t ack_timeout_ms);

/**
 * @brief Check and sync pending logs after successful TX
 * 
 * Call this after a successful TX to check if there are pending logs
 * that need to be synced to the property controller.
 * 
 * @return Number of pending log entries
 */
uint32_t agsys_lora_check_pending_logs(void);

/**
 * @brief Mark a log entry as synced
 * 
 * Call this after successfully sending a pending log entry.
 * 
 * @return true if entry marked, false if no pending entries
 */
bool agsys_lora_mark_log_synced(void);

#endif /* AGSYS_LORA_H */
