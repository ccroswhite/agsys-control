/**
 * @file agsys_lora_ota.h
 * @brief LoRa OTA Handler for Firmware Updates
 * 
 * Handles OTA firmware updates received over LoRa from the property controller.
 * Works with the agsys_ota module for actual update processing.
 * 
 * Protocol Messages:
 *   - OTA_START (0x40): Start OTA session
 *   - OTA_CHUNK (0x41): Firmware data chunk
 *   - OTA_FINISH (0x42): Finish and apply update
 *   - OTA_ABORT (0x43): Abort update
 *   - OTA_ACK (0x44): Acknowledgment from device
 */

#ifndef AGSYS_LORA_OTA_H
#define AGSYS_LORA_OTA_H

#include <stdint.h>
#include <stdbool.h>
#include "agsys_ota.h"
#include "agsys_lora.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * MESSAGE TYPES
 * ========================================================================== */

#define AGSYS_LORA_OTA_MSG_START    0x40
#define AGSYS_LORA_OTA_MSG_CHUNK    0x41
#define AGSYS_LORA_OTA_MSG_FINISH   0x42
#define AGSYS_LORA_OTA_MSG_ABORT    0x43
#define AGSYS_LORA_OTA_MSG_ACK      0x44
#define AGSYS_LORA_OTA_MSG_STATUS   0x45

/* ==========================================================================
 * ACK STATUS CODES
 * ========================================================================== */

typedef enum {
    AGSYS_LORA_OTA_ACK_OK           = 0x00,
    AGSYS_LORA_OTA_ACK_READY        = 0x01,
    AGSYS_LORA_OTA_ACK_CHUNK_OK     = 0x02,
    AGSYS_LORA_OTA_ACK_COMPLETE     = 0x03,
    AGSYS_LORA_OTA_ACK_REBOOTING    = 0x04,
    AGSYS_LORA_OTA_ACK_ERROR        = 0x80,
} agsys_lora_ota_ack_t;

/* ==========================================================================
 * MESSAGE STRUCTURES
 * ========================================================================== */

/**
 * OTA_START payload (from controller):
 *   [0-3]  fw_size (uint32_t, little-endian)
 *   [4-7]  fw_crc (uint32_t, little-endian)
 *   [8]    major version
 *   [9]    minor version
 *   [10]   patch version
 *   [11]   chunk_size (typically 200 bytes for LoRa)
 *   [12-13] total_chunks (uint16_t)
 */

/**
 * OTA_CHUNK payload (from controller):
 *   [0-1]  chunk_index (uint16_t, little-endian)
 *   [2-3]  offset (uint16_t, chunk_index * chunk_size, for verification)
 *   [4+]   data (up to chunk_size bytes)
 */

/**
 * OTA_ACK payload (to controller):
 *   [0]    ack_status
 *   [1]    progress (0-100)
 *   [2-3]  last_chunk_received (uint16_t) or error_code
 */

/* ==========================================================================
 * CONTEXT
 * ========================================================================== */

typedef struct {
    agsys_ota_ctx_t     *ota_ctx;
    agsys_lora_ctx_t    *lora_ctx;
    
    /* Session state */
    bool                session_active;
    uint32_t            expected_size;
    uint16_t            chunk_size;
    uint16_t            total_chunks;
    uint16_t            last_chunk_received;
    uint32_t            session_start_tick;
    
    /* Timeout */
    uint32_t            chunk_timeout_ms;
    uint32_t            last_chunk_tick;
    
    bool                initialized;
} agsys_lora_ota_ctx_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize LoRa OTA handler
 * @param ctx LoRa OTA context
 * @param ota_ctx OTA context (must be initialized)
 * @param lora_ctx LoRa context (must be initialized)
 * @return true on success
 */
bool agsys_lora_ota_init(agsys_lora_ota_ctx_t *ctx,
                          agsys_ota_ctx_t *ota_ctx,
                          agsys_lora_ctx_t *lora_ctx);

/**
 * @brief Handle incoming LoRa OTA message
 * 
 * Call this when an OTA message is received over LoRa.
 * 
 * @param ctx LoRa OTA context
 * @param msg_type Message type (0x40-0x45)
 * @param data Message payload
 * @param len Payload length
 * @return true if message was handled
 */
bool agsys_lora_ota_handle_message(agsys_lora_ota_ctx_t *ctx,
                                    uint8_t msg_type,
                                    const uint8_t *data,
                                    size_t len);

/**
 * @brief Check for OTA timeout
 * 
 * Call periodically to check if OTA session has timed out.
 * 
 * @param ctx LoRa OTA context
 * @return true if timeout occurred and session was aborted
 */
bool agsys_lora_ota_check_timeout(agsys_lora_ota_ctx_t *ctx);

/**
 * @brief Check if OTA session is active
 * @param ctx LoRa OTA context
 * @return true if OTA is in progress
 */
bool agsys_lora_ota_is_active(agsys_lora_ota_ctx_t *ctx);

/**
 * @brief Get current progress
 * @param ctx LoRa OTA context
 * @return Progress 0-100
 */
uint8_t agsys_lora_ota_get_progress(agsys_lora_ota_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_LORA_OTA_H */
