/**
 * @file agsys_crypto.h
 * @brief AgSys LoRa Packet Encryption using AES-128-GCM
 * 
 * Provides authenticated encryption for LoRa packets with:
 * - AES-128-GCM for encryption + integrity
 * - Per-device keys derived from: SHA-256(SECRET_SALT || DEVICE_UID)[0:16]
 * - 4-byte nonce (from packet counter)
 * - 4-byte truncated auth tag
 * 
 * Encrypted packet format:
 *   [Nonce:4][Ciphertext:N][Tag:4] = N+8 bytes overhead
 * 
 * Security notes:
 * - Nonce MUST be unique per device (counter-based)
 * - Nonce counter MUST be persisted to NVRAM before each TX
 * - Key is unique per device, so nonce collisions between devices are safe
 */

#ifndef AGSYS_CRYPTO_H
#define AGSYS_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "agsys_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize crypto subsystem
 * 
 * Derives the device-specific key from SECRET_SALT and device UID.
 * Must be called before any encrypt/decrypt operations.
 * 
 * @param deviceUid 8-byte device unique identifier (from MCU FICR)
 */
void agsys_crypto_init(const uint8_t* deviceUid);

/**
 * @brief Set encryption key directly (for testing or property controller)
 * 
 * @param key 16-byte AES-128 key
 */
void agsys_crypto_setKey(const uint8_t* key);

/**
 * @brief Derive key for a specific device (used by property controller)
 * 
 * Computes: SHA-256(SECRET_SALT || deviceUid)[0:16]
 * 
 * @param deviceUid 8-byte device unique identifier
 * @param keyOut Output buffer for 16-byte derived key
 */
void agsys_crypto_deriveKey(const uint8_t* deviceUid, uint8_t* keyOut);

/**
 * @brief Encrypt a LoRa packet
 * 
 * @param plaintext     Input plaintext data (header + payload)
 * @param plaintextLen  Length of plaintext (max AGSYS_MAX_PLAINTEXT)
 * @param packetOut     Output buffer (must be at least plaintextLen + AGSYS_CRYPTO_OVERHEAD)
 * @param packetLen     Output: actual packet length
 * @return true on success, false on error
 * 
 * Output format: [Nonce:4][Ciphertext:N][Tag:4]
 * Nonce is auto-incremented on each call.
 */
bool agsys_crypto_encrypt(const uint8_t* plaintext, size_t plaintextLen,
                          uint8_t* packetOut, size_t* packetLen);

/**
 * @brief Decrypt a LoRa packet
 * 
 * @param packet        Input encrypted packet
 * @param packetLen     Length of packet
 * @param plaintextOut  Output buffer (must be at least packetLen - AGSYS_CRYPTO_OVERHEAD)
 * @param plaintextLen  Output: actual plaintext length
 * @return true on success (auth tag valid), false on error or auth failure
 */
bool agsys_crypto_decrypt(const uint8_t* packet, size_t packetLen,
                          uint8_t* plaintextOut, size_t* plaintextLen);

/**
 * @brief Decrypt a packet using a specific key (for property controller)
 * 
 * @param key           16-byte AES-128 key
 * @param packet        Input encrypted packet
 * @param packetLen     Length of packet
 * @param plaintextOut  Output buffer
 * @param plaintextLen  Output: actual plaintext length
 * @return true on success, false on error or auth failure
 */
bool agsys_crypto_decryptWithKey(const uint8_t* key,
                                  const uint8_t* packet, size_t packetLen,
                                  uint8_t* plaintextOut, size_t* plaintextLen);

/**
 * @brief Encrypt a packet using a specific key (for property controller)
 * 
 * @param key           16-byte AES-128 key
 * @param nonce         4-byte nonce value
 * @param plaintext     Input plaintext data
 * @param plaintextLen  Length of plaintext
 * @param packetOut     Output buffer
 * @param packetLen     Output: actual packet length
 * @return true on success, false on error
 */
bool agsys_crypto_encryptWithKey(const uint8_t* key, uint32_t nonce,
                                  const uint8_t* plaintext, size_t plaintextLen,
                                  uint8_t* packetOut, size_t* packetLen);

/**
 * @brief Get current nonce/packet counter value
 * @return Current 32-bit nonce value
 */
uint32_t agsys_crypto_getNonce(void);

/**
 * @brief Set nonce/packet counter (use with caution - for sync after reboot)
 * 
 * WARNING: Setting nonce to a previously used value will break security!
 * Only use to restore from NVRAM after reboot.
 * 
 * @param nonce New nonce value
 */
void agsys_crypto_setNonce(uint32_t nonce);

/**
 * @brief Increment and persist nonce (call before each TX)
 * 
 * This function should be called before each transmission to ensure
 * the nonce is persisted to NVRAM before the packet is sent.
 * 
 * @return New nonce value to use
 */
uint32_t agsys_crypto_nextNonce(void);

#ifdef __cplusplus
}
#endif

#endif // AGSYS_CRYPTO_H
