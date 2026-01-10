/**
 * @file agsys_crypto.h
 * @brief AES-128-GCM encryption for AgSys LoRa protocol
 * 
 * Uses Nordic's hardware crypto accelerator (CC310) when available,
 * falls back to software implementation on nRF52810.
 */

#ifndef AGSYS_CRYPTO_H
#define AGSYS_CRYPTO_H

#include "agsys_common.h"

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

#define AGSYS_CRYPTO_KEY_SIZE       16  /* AES-128 key size */
#define AGSYS_CRYPTO_IV_SIZE        12  /* GCM IV/nonce size */
#define AGSYS_CRYPTO_TAG_SIZE       16  /* GCM authentication tag size */
#define AGSYS_CRYPTO_SALT_SIZE      16  /* Secret salt for key derivation */

/* ==========================================================================
 * TYPES
 * ========================================================================== */

/**
 * @brief Crypto context
 */
typedef struct {
    uint8_t key[AGSYS_CRYPTO_KEY_SIZE];
    bool    initialized;
} agsys_crypto_ctx_t;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize the crypto subsystem
 * 
 * Must be called before any crypto operations.
 * Initializes hardware crypto if available.
 * 
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_crypto_init(void);

/**
 * @brief Deinitialize the crypto subsystem
 */
void agsys_crypto_deinit(void);

/* ==========================================================================
 * KEY MANAGEMENT
 * ========================================================================== */

/**
 * @brief Derive encryption key from secret salt and device UID
 * 
 * Uses HKDF-SHA256 to derive a unique key per device.
 * Key = HKDF(salt, device_uid, "agsys-lora-v1")
 * 
 * @param ctx       Crypto context to initialize
 * @param salt      16-byte secret salt (from provisioning)
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_crypto_derive_key(agsys_crypto_ctx_t *ctx,
                                     const uint8_t salt[AGSYS_CRYPTO_SALT_SIZE]);

/**
 * @brief Set encryption key directly (for testing)
 * 
 * @param ctx       Crypto context
 * @param key       16-byte AES key
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_crypto_set_key(agsys_crypto_ctx_t *ctx,
                                  const uint8_t key[AGSYS_CRYPTO_KEY_SIZE]);

/* ==========================================================================
 * ENCRYPTION / DECRYPTION
 * ========================================================================== */

/**
 * @brief Encrypt data using AES-128-GCM
 * 
 * @param ctx           Initialized crypto context
 * @param plaintext     Input data
 * @param plaintext_len Input length
 * @param aad           Additional authenticated data (can be NULL)
 * @param aad_len       AAD length
 * @param iv            12-byte IV/nonce (must be unique per message)
 * @param ciphertext    Output buffer (same size as plaintext)
 * @param tag           Output: 16-byte authentication tag
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_crypto_encrypt(const agsys_crypto_ctx_t *ctx,
                                  const uint8_t *plaintext,
                                  size_t plaintext_len,
                                  const uint8_t *aad,
                                  size_t aad_len,
                                  const uint8_t iv[AGSYS_CRYPTO_IV_SIZE],
                                  uint8_t *ciphertext,
                                  uint8_t tag[AGSYS_CRYPTO_TAG_SIZE]);

/**
 * @brief Decrypt data using AES-128-GCM
 * 
 * @param ctx           Initialized crypto context
 * @param ciphertext    Input encrypted data
 * @param ciphertext_len Input length
 * @param aad           Additional authenticated data (can be NULL)
 * @param aad_len       AAD length
 * @param iv            12-byte IV/nonce
 * @param tag           16-byte authentication tag
 * @param plaintext     Output buffer (same size as ciphertext)
 * @return AGSYS_OK on success, AGSYS_ERR_CRYPTO if tag verification fails
 */
agsys_err_t agsys_crypto_decrypt(const agsys_crypto_ctx_t *ctx,
                                  const uint8_t *ciphertext,
                                  size_t ciphertext_len,
                                  const uint8_t *aad,
                                  size_t aad_len,
                                  const uint8_t iv[AGSYS_CRYPTO_IV_SIZE],
                                  const uint8_t tag[AGSYS_CRYPTO_TAG_SIZE],
                                  uint8_t *plaintext);

/* ==========================================================================
 * UTILITIES
 * ========================================================================== */

/**
 * @brief Generate random bytes using hardware RNG
 * 
 * @param buf       Output buffer
 * @param len       Number of bytes to generate
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_crypto_random(uint8_t *buf, size_t len);

/**
 * @brief Generate a random IV for encryption
 * 
 * @param iv        Output: 12-byte IV
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_crypto_generate_iv(uint8_t iv[AGSYS_CRYPTO_IV_SIZE]);

#endif /* AGSYS_CRYPTO_H */
