/**
 * @file firmware_crypto.h
 * @brief Firmware encryption for secure backup storage
 * 
 * Encrypts firmware backups stored in external W25Q16 flash using AES-256-CTR.
 * The encryption key is derived from:
 *   1. FICR device ID (64-bit, unique per chip)
 *   2. Hardcoded secret salt (compile-time constant)
 * 
 * This ensures:
 *   - Firmware backup is encrypted at rest
 *   - Key is unique per device (can't copy backup between devices)
 *   - Attacker needs both physical access AND the secret salt
 */

#ifndef FIRMWARE_CRYPTO_H
#define FIRMWARE_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encryption block size (AES)
 */
#define FW_CRYPTO_BLOCK_SIZE    16

/**
 * @brief Key size (AES-256)
 */
#define FW_CRYPTO_KEY_SIZE      32

/**
 * @brief IV/nonce size for CTR mode
 */
#define FW_CRYPTO_IV_SIZE       16

/**
 * @brief Initialize the firmware crypto module
 * 
 * Derives the device-specific encryption key from FICR + secret salt.
 * Must be called before any encrypt/decrypt operations.
 */
void fw_crypto_init(void);

/**
 * @brief Encrypt a buffer in place
 * 
 * Uses AES-256-CTR mode. The IV is derived from the block offset,
 * allowing random access encryption/decryption.
 * 
 * @param data      Buffer to encrypt (modified in place)
 * @param length    Length of data (must be multiple of 16)
 * @param offset    Byte offset in the firmware image (for IV derivation)
 * @return true on success
 */
bool fw_crypto_encrypt(uint8_t* data, size_t length, uint32_t offset);

/**
 * @brief Decrypt a buffer in place
 * 
 * Uses AES-256-CTR mode. CTR mode encryption and decryption are identical.
 * 
 * @param data      Buffer to decrypt (modified in place)
 * @param length    Length of data (must be multiple of 16)
 * @param offset    Byte offset in the firmware image (for IV derivation)
 * @return true on success
 */
bool fw_crypto_decrypt(uint8_t* data, size_t length, uint32_t offset);

/**
 * @brief Encrypt and write firmware chunk to external flash
 * 
 * @param flash_addr    Address in W25Q16 to write to
 * @param data          Plaintext firmware data
 * @param length        Length of data
 * @return true on success
 */
bool fw_crypto_write_chunk(uint32_t flash_addr, const uint8_t* data, size_t length);

/**
 * @brief Read and decrypt firmware chunk from external flash
 * 
 * @param flash_addr    Address in W25Q16 to read from
 * @param data          Buffer to store decrypted data
 * @param length        Length of data to read
 * @return true on success
 */
bool fw_crypto_read_chunk(uint32_t flash_addr, uint8_t* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif // FIRMWARE_CRYPTO_H
