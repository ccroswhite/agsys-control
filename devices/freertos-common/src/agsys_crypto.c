/**
 * @file agsys_crypto.c
 * @brief AES-128-GCM encryption implementation
 * 
 * Uses Nordic's nrf_crypto library which leverages hardware crypto
 * accelerator (CC310) on nRF52840, software fallback on others.
 */

#include "agsys_crypto.h"
#include "agsys_debug.h"

#include "nrf_crypto.h"
#include "nrf_crypto_aead.h"
#include "nrf_crypto_rng.h"

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static bool m_initialized = false;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

agsys_err_t agsys_crypto_init(void)
{
    if (m_initialized) {
        return AGSYS_OK;
    }

    ret_code_t err = nrf_crypto_init();
    if (err != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("Crypto: Init failed: %d", err);
        return AGSYS_ERR_CRYPTO;
    }

    m_initialized = true;
    AGSYS_LOG_INFO("Crypto: Initialized");
    return AGSYS_OK;
}

void agsys_crypto_deinit(void)
{
    if (!m_initialized) {
        return;
    }

    nrf_crypto_uninit();
    m_initialized = false;
}

/* ==========================================================================
 * KEY MANAGEMENT
 * ========================================================================== */

agsys_err_t agsys_crypto_derive_key(agsys_crypto_ctx_t *ctx,
                                     const uint8_t salt[AGSYS_CRYPTO_SALT_SIZE])
{
    if (ctx == NULL || salt == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (!m_initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    /* Get device UID */
    uint8_t device_uid[8];
    agsys_get_device_uid(device_uid);

    /* Simple key derivation: SHA256(salt || device_uid || "agsys-lora-v1")
     * Then take first 16 bytes as AES key.
     * 
     * For production, consider using proper HKDF, but this is sufficient
     * for our use case where salt is already random and unique per property.
     */
    
    /* Concatenate inputs */
    uint8_t input[AGSYS_CRYPTO_SALT_SIZE + 8 + 12];  /* salt + uid + label */
    memcpy(input, salt, AGSYS_CRYPTO_SALT_SIZE);
    memcpy(input + AGSYS_CRYPTO_SALT_SIZE, device_uid, 8);
    memcpy(input + AGSYS_CRYPTO_SALT_SIZE + 8, "agsys-lora-v1", 12);

    /* Hash with SHA256 */
    uint8_t hash[32];
    size_t hash_len = sizeof(hash);
    
    nrf_crypto_hash_context_t hash_ctx;
    ret_code_t err = nrf_crypto_hash_calculate(
        &hash_ctx,
        &g_nrf_crypto_hash_sha256_info,
        input, sizeof(input),
        hash, &hash_len
    );

    if (err != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("Crypto: Key derivation failed: %d", err);
        return AGSYS_ERR_CRYPTO;
    }

    /* Take first 16 bytes as key */
    memcpy(ctx->key, hash, AGSYS_CRYPTO_KEY_SIZE);
    ctx->initialized = true;

    AGSYS_LOG_DEBUG("Crypto: Key derived from salt");
    return AGSYS_OK;
}

agsys_err_t agsys_crypto_set_key(agsys_crypto_ctx_t *ctx,
                                  const uint8_t key[AGSYS_CRYPTO_KEY_SIZE])
{
    if (ctx == NULL || key == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    memcpy(ctx->key, key, AGSYS_CRYPTO_KEY_SIZE);
    ctx->initialized = true;
    return AGSYS_OK;
}

/* ==========================================================================
 * ENCRYPTION / DECRYPTION
 * ========================================================================== */

agsys_err_t agsys_crypto_encrypt(const agsys_crypto_ctx_t *ctx,
                                  const uint8_t *plaintext,
                                  size_t plaintext_len,
                                  const uint8_t *aad,
                                  size_t aad_len,
                                  const uint8_t iv[AGSYS_CRYPTO_IV_SIZE],
                                  uint8_t *ciphertext,
                                  uint8_t tag[AGSYS_CRYPTO_TAG_SIZE])
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }
    if (plaintext == NULL || ciphertext == NULL || iv == NULL || tag == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (!m_initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    /* Set up AEAD context */
    nrf_crypto_aead_context_t aead_ctx;
    
    ret_code_t err = nrf_crypto_aead_init(
        &aead_ctx,
        &g_nrf_crypto_aes_gcm_128_info,
        (uint8_t *)ctx->key
    );
    if (err != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("Crypto: AEAD init failed: %d", err);
        return AGSYS_ERR_CRYPTO;
    }

    /* Encrypt */
    size_t ciphertext_len = plaintext_len;
    size_t tag_len = AGSYS_CRYPTO_TAG_SIZE;

    err = nrf_crypto_aead_crypt(
        &aead_ctx,
        NRF_CRYPTO_ENCRYPT,
        (uint8_t *)iv, AGSYS_CRYPTO_IV_SIZE,
        (uint8_t *)aad, aad_len,
        (uint8_t *)plaintext, plaintext_len,
        ciphertext, &ciphertext_len,
        tag, &tag_len
    );

    nrf_crypto_aead_uninit(&aead_ctx);

    if (err != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("Crypto: Encrypt failed: %d", err);
        return AGSYS_ERR_CRYPTO;
    }

    return AGSYS_OK;
}

agsys_err_t agsys_crypto_decrypt(const agsys_crypto_ctx_t *ctx,
                                  const uint8_t *ciphertext,
                                  size_t ciphertext_len,
                                  const uint8_t *aad,
                                  size_t aad_len,
                                  const uint8_t iv[AGSYS_CRYPTO_IV_SIZE],
                                  const uint8_t tag[AGSYS_CRYPTO_TAG_SIZE],
                                  uint8_t *plaintext)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }
    if (ciphertext == NULL || plaintext == NULL || iv == NULL || tag == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (!m_initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    /* Set up AEAD context */
    nrf_crypto_aead_context_t aead_ctx;
    
    ret_code_t err = nrf_crypto_aead_init(
        &aead_ctx,
        &g_nrf_crypto_aes_gcm_128_info,
        (uint8_t *)ctx->key
    );
    if (err != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("Crypto: AEAD init failed: %d", err);
        return AGSYS_ERR_CRYPTO;
    }

    /* Decrypt */
    size_t plaintext_len = ciphertext_len;
    uint8_t tag_copy[AGSYS_CRYPTO_TAG_SIZE];
    memcpy(tag_copy, tag, AGSYS_CRYPTO_TAG_SIZE);
    size_t tag_len = AGSYS_CRYPTO_TAG_SIZE;

    err = nrf_crypto_aead_crypt(
        &aead_ctx,
        NRF_CRYPTO_DECRYPT,
        (uint8_t *)iv, AGSYS_CRYPTO_IV_SIZE,
        (uint8_t *)aad, aad_len,
        (uint8_t *)ciphertext, ciphertext_len,
        plaintext, &plaintext_len,
        tag_copy, &tag_len
    );

    nrf_crypto_aead_uninit(&aead_ctx);

    if (err != NRF_SUCCESS) {
        if (err == NRF_ERROR_CRYPTO_AEAD_INVALID_MAC) {
            AGSYS_LOG_WARNING("Crypto: Authentication failed");
        } else {
            AGSYS_LOG_ERROR("Crypto: Decrypt failed: %d", err);
        }
        return AGSYS_ERR_CRYPTO;
    }

    return AGSYS_OK;
}

/* ==========================================================================
 * UTILITIES
 * ========================================================================== */

agsys_err_t agsys_crypto_random(uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (!m_initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    ret_code_t err = nrf_crypto_rng_vector_generate(buf, len);
    if (err != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("Crypto: RNG failed: %d", err);
        return AGSYS_ERR_CRYPTO;
    }

    return AGSYS_OK;
}

agsys_err_t agsys_crypto_generate_iv(uint8_t iv[AGSYS_CRYPTO_IV_SIZE])
{
    return agsys_crypto_random(iv, AGSYS_CRYPTO_IV_SIZE);
}
