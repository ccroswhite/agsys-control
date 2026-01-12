/**
 * @file agsys_protocol.h
 * @brief AgSys LoRa protocol - FreeRTOS wrapper
 * 
 * This file includes the canonical protocol definition and provides
 * any FreeRTOS-specific additions.
 * 
 * The canonical definition is:
 *   freertos-common/include/agsys_lora_protocol.h
 * 
 * This file is copied from agsys-api/gen/c/lora/v1/agsys_lora_protocol.h
 * When the protocol changes, update the source in agsys-api and copy here.
 * 
 * DO NOT define protocol constants or structures here.
 * All definitions should be in the canonical header.
 */

#ifndef AGSYS_PROTOCOL_H
#define AGSYS_PROTOCOL_H

/* Include the canonical protocol definition (local copy) */
#include "agsys_lora_protocol.h"

/* ==========================================================================
 * FREERTOS-SPECIFIC TYPE ALIASES
 * 
 * These provide backward compatibility with existing FreeRTOS code
 * that uses slightly different naming conventions.
 * ========================================================================== */

/* Header alias */
typedef agsys_header_t agsys_msg_header_t;

/* Legacy message type aliases */
#define AGSYS_MSG_SENSOR_DATA           AGSYS_MSG_SOIL_REPORT
#define AGSYS_MSG_METER_DATA            AGSYS_MSG_METER_REPORT

/* ==========================================================================
 * PROTOCOL ENCODING / DECODING FUNCTIONS
 * ========================================================================== */

#include "agsys_common.h"

/**
 * @brief Encode a message for transmission
 * 
 * @param header        Message header
 * @param payload       Payload data
 * @param payload_len   Payload length
 * @param crypto_ctx    Crypto context for encryption
 * @param out_buf       Output buffer
 * @param out_len       Output: actual encoded length
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_protocol_encode(const agsys_header_t *header,
                                   const void *payload,
                                   size_t payload_len,
                                   const void *crypto_ctx,
                                   uint8_t *out_buf,
                                   size_t *out_len);

/**
 * @brief Decode a received message
 * 
 * @param in_buf        Input buffer
 * @param in_len        Input length
 * @param crypto_ctx    Crypto context for decryption
 * @param header        Output: decoded header
 * @param payload       Output: decrypted payload
 * @param payload_len   Output: payload length
 * @return AGSYS_OK on success, AGSYS_ERR_CRYPTO if auth fails
 */
agsys_err_t agsys_protocol_decode(const uint8_t *in_buf,
                                   size_t in_len,
                                   const void *crypto_ctx,
                                   agsys_header_t *header,
                                   void *payload,
                                   size_t *payload_len);

#endif /* AGSYS_PROTOCOL_H */
