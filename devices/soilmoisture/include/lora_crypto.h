/**
 * @file lora_crypto.h
 * @brief LoRa packet encryption using AES-128-GCM
 * 
 * Provides authenticated encryption for LoRa packets with:
 * - AES-128-GCM for encryption + integrity
 * - 4-byte nonce (from packet counter)
 * - 4-byte truncated auth tag
 * - Device ID bound in associated data
 * 
 * Packet format:
 *   [Nonce:4][Ciphertext:N][Tag:4] = N+8 bytes overhead
 */

#ifndef LORA_CRYPTO_H
#define LORA_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// Crypto parameters
#define LORA_CRYPTO_KEY_SIZE     16   // AES-128
#define LORA_CRYPTO_NONCE_SIZE   4    // Truncated nonce (packet counter)
#define LORA_CRYPTO_TAG_SIZE     4    // Truncated auth tag
#define LORA_CRYPTO_OVERHEAD     (LORA_CRYPTO_NONCE_SIZE + LORA_CRYPTO_TAG_SIZE)  // 8 bytes

// Maximum payload sizes
#define LORA_MAX_PLAINTEXT       200  // Max plaintext payload
#define LORA_MAX_PACKET          (LORA_MAX_PLAINTEXT + LORA_CRYPTO_OVERHEAD)

/**
 * @brief Initialize LoRa crypto with device-specific key
 * 
 * Key is derived from: SHA-256(SECRET_SALT || FICR_DEVICE_ID)[0:16]
 * Must be called before any encrypt/decrypt operations.
 */
void lora_crypto_init(void);

/**
 * @brief Set encryption key directly (for testing or custom key management)
 * @param key 16-byte AES-128 key
 */
void lora_crypto_setKey(const uint8_t* key);

/**
 * @brief Encrypt a LoRa packet
 * 
 * @param plaintext     Input plaintext data
 * @param plaintext_len Length of plaintext (max LORA_MAX_PLAINTEXT)
 * @param packet_out    Output buffer (must be at least plaintext_len + LORA_CRYPTO_OVERHEAD)
 * @param packet_len    Output: actual packet length
 * @return true on success, false on error
 * 
 * Output format: [Nonce:4][Ciphertext:N][Tag:4]
 * Nonce is auto-incremented on each call.
 */
bool lora_crypto_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                         uint8_t* packet_out, size_t* packet_len);

/**
 * @brief Decrypt a LoRa packet
 * 
 * @param packet        Input encrypted packet
 * @param packet_len    Length of packet
 * @param plaintext_out Output buffer (must be at least packet_len - LORA_CRYPTO_OVERHEAD)
 * @param plaintext_len Output: actual plaintext length
 * @return true on success (auth tag valid), false on error or auth failure
 */
bool lora_crypto_decrypt(const uint8_t* packet, size_t packet_len,
                         uint8_t* plaintext_out, size_t* plaintext_len);

/**
 * @brief Get current nonce/packet counter value
 * @return Current 32-bit nonce value
 */
uint32_t lora_crypto_getNonce(void);

/**
 * @brief Set nonce/packet counter (use with caution - for sync after reboot)
 * @param nonce New nonce value
 */
void lora_crypto_setNonce(uint32_t nonce);

#endif // LORA_CRYPTO_H
