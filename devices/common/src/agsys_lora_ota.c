/**
 * @file agsys_lora_ota.c
 * @brief LoRa OTA Handler for Firmware Updates
 */

#include "agsys_lora_ota.h"
#include "agsys_ota.h"
#include "agsys_lora.h"
#include "FreeRTOS.h"
#include "task.h"
#include "SEGGER_RTT.h"
#include <string.h>

#define DEFAULT_CHUNK_TIMEOUT_MS    30000

/* ==========================================================================
 * ACK SENDING
 * ========================================================================== */

static void send_ack(agsys_lora_ota_ctx_t *ctx, agsys_lora_ota_ack_t ack_status,
                     uint8_t progress, uint16_t extra)
{
    uint8_t payload[5];
    payload[0] = AGSYS_LORA_OTA_MSG_ACK;
    payload[1] = ack_status;
    payload[2] = progress;
    payload[3] = extra & 0xFF;
    payload[4] = (extra >> 8) & 0xFF;
    
    agsys_lora_transmit(ctx->lora_ctx, payload, sizeof(payload));
}

/* ==========================================================================
 * CALLBACKS
 * ========================================================================== */

static void ota_complete_callback(bool success, agsys_ota_error_t error, void *user_data)
{
    agsys_lora_ota_ctx_t *ctx = (agsys_lora_ota_ctx_t *)user_data;
    if (ctx == NULL) return;
    
    if (success) {
        send_ack(ctx, AGSYS_LORA_OTA_ACK_REBOOTING, 100, 0);
    } else {
        send_ack(ctx, AGSYS_LORA_OTA_ACK_ERROR, 0, (uint16_t)error);
    }
    ctx->session_active = false;
}

/* ==========================================================================
 * MESSAGE HANDLERS
 * ========================================================================== */

static bool handle_start(agsys_lora_ota_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (len < 14) return false;
    
    uint32_t fw_size = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    uint32_t fw_crc = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    uint8_t major = data[8];
    uint8_t minor = data[9];
    uint8_t patch = data[10];
    ctx->chunk_size = data[11];
    ctx->total_chunks = data[12] | (data[13] << 8);
    
    SEGGER_RTT_printf(0, "LoRa OTA: START size=%lu chunks=%d\n", fw_size, ctx->total_chunks);
    
    agsys_ota_suspend_tasks();
    
    agsys_ota_error_t err = agsys_ota_start(ctx->ota_ctx, fw_size, fw_crc, major, minor, patch);
    if (err != AGSYS_OTA_ERR_NONE) {
        send_ack(ctx, AGSYS_LORA_OTA_ACK_ERROR, 0, (uint16_t)err);
        agsys_ota_resume_tasks();
        return false;
    }
    
    ctx->session_active = true;
    ctx->expected_size = fw_size;
    ctx->last_chunk_received = 0xFFFF;
    ctx->session_start_tick = xTaskGetTickCount();
    ctx->last_chunk_tick = ctx->session_start_tick;
    
    send_ack(ctx, AGSYS_LORA_OTA_ACK_READY, 0, 0);
    return true;
}

static bool handle_chunk(agsys_lora_ota_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx->session_active || len < 5) return false;
    
    uint16_t chunk_idx = data[0] | (data[1] << 8);
    uint32_t offset = chunk_idx * ctx->chunk_size;
    size_t chunk_len = len - 4;
    
    agsys_ota_error_t err = agsys_ota_write_chunk(ctx->ota_ctx, offset, data + 4, chunk_len);
    if (err != AGSYS_OTA_ERR_NONE) {
        send_ack(ctx, AGSYS_LORA_OTA_ACK_ERROR, 0, (uint16_t)err);
        return false;
    }
    
    ctx->last_chunk_received = chunk_idx;
    ctx->last_chunk_tick = xTaskGetTickCount();
    
    uint8_t progress = agsys_ota_get_progress(ctx->ota_ctx);
    
    /* ACK every 10 chunks or on last chunk */
    if ((chunk_idx % 10 == 0) || (chunk_idx == ctx->total_chunks - 1)) {
        send_ack(ctx, AGSYS_LORA_OTA_ACK_CHUNK_OK, progress, chunk_idx);
    }
    
    return true;
}

static bool handle_finish(agsys_lora_ota_ctx_t *ctx)
{
    if (!ctx->session_active) return false;
    
    SEGGER_RTT_printf(0, "LoRa OTA: FINISH\n");
    
    agsys_ota_error_t err = agsys_ota_finish(ctx->ota_ctx);
    if (err != AGSYS_OTA_ERR_NONE) {
        send_ack(ctx, AGSYS_LORA_OTA_ACK_ERROR, 0, (uint16_t)err);
        agsys_ota_resume_tasks();
        ctx->session_active = false;
        return false;
    }
    
    return true;
}

static bool handle_abort(agsys_lora_ota_ctx_t *ctx)
{
    SEGGER_RTT_printf(0, "LoRa OTA: ABORT\n");
    agsys_ota_abort(ctx->ota_ctx);
    agsys_ota_resume_tasks();
    ctx->session_active = false;
    send_ack(ctx, AGSYS_LORA_OTA_ACK_OK, 0, 0);
    return true;
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool agsys_lora_ota_init(agsys_lora_ota_ctx_t *ctx,
                          agsys_ota_ctx_t *ota_ctx,
                          agsys_lora_ctx_t *lora_ctx)
{
    if (ctx == NULL || ota_ctx == NULL || lora_ctx == NULL) return false;
    
    memset(ctx, 0, sizeof(agsys_lora_ota_ctx_t));
    ctx->ota_ctx = ota_ctx;
    ctx->lora_ctx = lora_ctx;
    ctx->chunk_timeout_ms = DEFAULT_CHUNK_TIMEOUT_MS;
    ctx->initialized = true;
    
    agsys_ota_set_complete_callback(ota_ctx, ota_complete_callback, ctx);
    
    SEGGER_RTT_printf(0, "LoRa OTA: Initialized\n");
    return true;
}

bool agsys_lora_ota_handle_message(agsys_lora_ota_ctx_t *ctx,
                                    uint8_t msg_type,
                                    const uint8_t *data,
                                    size_t len)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    switch (msg_type) {
        case AGSYS_LORA_OTA_MSG_START:
            return handle_start(ctx, data, len);
        case AGSYS_LORA_OTA_MSG_CHUNK:
            return handle_chunk(ctx, data, len);
        case AGSYS_LORA_OTA_MSG_FINISH:
            return handle_finish(ctx);
        case AGSYS_LORA_OTA_MSG_ABORT:
            return handle_abort(ctx);
        default:
            return false;
    }
}

bool agsys_lora_ota_check_timeout(agsys_lora_ota_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->session_active) return false;
    
    TickType_t elapsed = xTaskGetTickCount() - ctx->last_chunk_tick;
    if (elapsed >= pdMS_TO_TICKS(ctx->chunk_timeout_ms)) {
        SEGGER_RTT_printf(0, "LoRa OTA: Timeout\n");
        agsys_ota_abort(ctx->ota_ctx);
        agsys_ota_resume_tasks();
        ctx->session_active = false;
        return true;
    }
    return false;
}

bool agsys_lora_ota_is_active(agsys_lora_ota_ctx_t *ctx)
{
    return (ctx != NULL && ctx->session_active);
}

uint8_t agsys_lora_ota_get_progress(agsys_lora_ota_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->session_active) return 0;
    return agsys_ota_get_progress(ctx->ota_ctx);
}
