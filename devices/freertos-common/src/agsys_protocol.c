/**
 * @file agsys_protocol.c
 * @brief AgSys LoRa protocol encoding/decoding
 */

#include "agsys_protocol.h"
#include "agsys_crypto.h"
#include "agsys_debug.h"

/* ==========================================================================
 * SEQUENCE NUMBER
 * ========================================================================== */

static uint16_t m_seq_num = 0;

static uint16_t get_next_seq_num(void)
{
    return m_seq_num++;
}

/* ==========================================================================
 * ENCODING / DECODING
 * ========================================================================== */

agsys_err_t agsys_protocol_encode(const agsys_msg_header_t *header,
                                   const void *payload,
                                   size_t payload_len,
                                   const void *crypto_ctx,
                                   uint8_t *out_buf,
                                   size_t *out_len)
{
    if (header == NULL || out_buf == NULL || out_len == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (payload_len > 0 && payload == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (payload_len > AGSYS_MAX_PAYLOAD_SIZE) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    const agsys_crypto_ctx_t *ctx = (const agsys_crypto_ctx_t *)crypto_ctx;
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    /* Build plaintext: header + payload */
    uint8_t plaintext[AGSYS_MSG_HEADER_SIZE + AGSYS_MAX_PAYLOAD_SIZE];
    size_t plaintext_len = AGSYS_MSG_HEADER_SIZE + payload_len;

    /* Copy header (with updated seq_num if zero) */
    agsys_msg_header_t hdr = *header;
    if (hdr.seq_num == 0) {
        hdr.seq_num = get_next_seq_num();
    }
    memcpy(plaintext, &hdr, AGSYS_MSG_HEADER_SIZE);

    /* Copy payload */
    if (payload_len > 0) {
        memcpy(plaintext + AGSYS_MSG_HEADER_SIZE, payload, payload_len);
    }

    /* Generate IV */
    uint8_t iv[AGSYS_CRYPTO_IV_SIZE];
    agsys_err_t err = agsys_crypto_generate_iv(iv);
    if (err != AGSYS_OK) {
        return err;
    }

    /* Encrypt */
    uint8_t ciphertext[AGSYS_MSG_HEADER_SIZE + AGSYS_MAX_PAYLOAD_SIZE];
    uint8_t tag[AGSYS_CRYPTO_TAG_SIZE];

    err = agsys_crypto_encrypt(ctx, plaintext, plaintext_len,
                                NULL, 0,  /* No AAD */
                                iv, ciphertext, tag);
    if (err != AGSYS_OK) {
        return err;
    }

    /* Build output: ciphertext + IV + tag */
    size_t total_len = plaintext_len + AGSYS_CRYPTO_IV_SIZE + AGSYS_CRYPTO_TAG_SIZE;
    
    memcpy(out_buf, ciphertext, plaintext_len);
    memcpy(out_buf + plaintext_len, iv, AGSYS_CRYPTO_IV_SIZE);
    memcpy(out_buf + plaintext_len + AGSYS_CRYPTO_IV_SIZE, tag, AGSYS_CRYPTO_TAG_SIZE);

    *out_len = total_len;

    AGSYS_LOG_DEBUG("Protocol: Encoded msg type=%d, len=%d", hdr.msg_type, total_len);
    return AGSYS_OK;
}

agsys_err_t agsys_protocol_decode(const uint8_t *in_buf,
                                   size_t in_len,
                                   const void *crypto_ctx,
                                   agsys_msg_header_t *header,
                                   void *payload,
                                   size_t *payload_len)
{
    if (in_buf == NULL || header == NULL || payload_len == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    const agsys_crypto_ctx_t *ctx = (const agsys_crypto_ctx_t *)crypto_ctx;
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    /* Minimum size: header + IV + tag */
    size_t min_len = AGSYS_MSG_HEADER_SIZE + AGSYS_CRYPTO_IV_SIZE + AGSYS_CRYPTO_TAG_SIZE;
    if (in_len < min_len) {
        AGSYS_LOG_WARNING("Protocol: Message too short: %d", in_len);
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Extract components */
    size_t ciphertext_len = in_len - AGSYS_CRYPTO_IV_SIZE - AGSYS_CRYPTO_TAG_SIZE;
    const uint8_t *ciphertext = in_buf;
    const uint8_t *iv = in_buf + ciphertext_len;
    const uint8_t *tag = in_buf + ciphertext_len + AGSYS_CRYPTO_IV_SIZE;

    /* Decrypt */
    uint8_t plaintext[AGSYS_MSG_HEADER_SIZE + AGSYS_MAX_PAYLOAD_SIZE];

    agsys_err_t err = agsys_crypto_decrypt(ctx, ciphertext, ciphertext_len,
                                            NULL, 0,  /* No AAD */
                                            iv, tag, plaintext);
    if (err != AGSYS_OK) {
        AGSYS_LOG_WARNING("Protocol: Decryption failed");
        return err;
    }

    /* Extract header */
    memcpy(header, plaintext, AGSYS_MSG_HEADER_SIZE);

    /* Validate header */
    if (header->version != AGSYS_PROTOCOL_VERSION) {
        AGSYS_LOG_WARNING("Protocol: Version mismatch: %d", header->version);
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Extract payload */
    size_t plen = ciphertext_len - AGSYS_MSG_HEADER_SIZE;
    if (plen > 0 && payload != NULL) {
        memcpy(payload, plaintext + AGSYS_MSG_HEADER_SIZE, plen);
    }
    *payload_len = plen;

    AGSYS_LOG_DEBUG("Protocol: Decoded msg type=%d, payload=%d", header->msg_type, plen);
    return AGSYS_OK;
}
