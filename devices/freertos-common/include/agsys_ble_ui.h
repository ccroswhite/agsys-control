/**
 * @file agsys_ble_ui.h
 * @brief Shared BLE UI state and LED patterns for AgSys devices
 * 
 * Provides consistent BLE status indication across all devices:
 * - Devices with displays: Show BLE icon with flash patterns
 * - Devices with LED only: Use LED flash patterns
 * 
 * All devices should use the same timing for consistent UX.
 */

#ifndef AGSYS_BLE_UI_H
#define AGSYS_BLE_UI_H

#include <stdint.h>
#include <stdbool.h>
#include "agsys_ble.h"

/* ==========================================================================
 * BLE UI STATE
 * ========================================================================== */

/**
 * @brief BLE UI state for visual feedback
 * 
 * Maps to LED/icon behavior across all devices.
 */
typedef enum {
    BLE_UI_STATE_IDLE = 0,       /* BLE off, not advertising - LED/icon hidden */
    BLE_UI_STATE_ADVERTISING,    /* Pairing mode, discoverable - slow blink */
    BLE_UI_STATE_CONNECTED,      /* Connected, not authenticated - fast blink */
    BLE_UI_STATE_AUTHENTICATED,  /* Connected and PIN verified - solid on */
    BLE_UI_STATE_DISCONNECTED    /* Was connected, now lost - triple flash then idle */
} agsys_ble_ui_state_t;

/* ==========================================================================
 * LED/ICON TIMING (milliseconds)
 * ========================================================================== */

#define BLE_UI_BLINK_SLOW_MS        500     /* Advertising: 1 Hz (500ms on/off) */
#define BLE_UI_BLINK_FAST_MS        250     /* Connected: 2 Hz (250ms on/off) */
#define BLE_UI_BLINK_TRIPLE_MS      100     /* Disconnect flash: 100ms per toggle */
#define BLE_UI_TRIPLE_FLASH_COUNT   6       /* 3 on + 3 off = 6 toggles */

/* ==========================================================================
 * PAIRING TIMEOUT DEFAULTS
 * ========================================================================== */

#ifndef BLE_PAIRING_TIMEOUT_SEC
#define BLE_PAIRING_TIMEOUT_SEC     120     /* 2 minutes default */
#endif

#ifndef BLE_PAIRING_MAX_SEC
#define BLE_PAIRING_MAX_SEC         600     /* 10 minutes max */
#endif

#ifndef BLE_PAIRING_EXTEND_SEC
#define BLE_PAIRING_EXTEND_SEC      120     /* 2 minute extension on activity */
#endif

/* ==========================================================================
 * BLE UI CONTEXT
 * ========================================================================== */

/**
 * @brief BLE UI context for tracking state and animation
 */
typedef struct {
    agsys_ble_ui_state_t state;         /* Current UI state */
    bool                 visible;        /* Current visibility (for blinking) */
    uint32_t             last_toggle_ms; /* Last toggle timestamp */
    uint8_t              flash_count;    /* Counter for triple flash */
} agsys_ble_ui_ctx_t;

/* ==========================================================================
 * FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize BLE UI context
 * 
 * @param ctx   BLE UI context
 */
static inline void agsys_ble_ui_init(agsys_ble_ui_ctx_t *ctx)
{
    ctx->state = BLE_UI_STATE_IDLE;
    ctx->visible = false;
    ctx->last_toggle_ms = 0;
    ctx->flash_count = 0;
}

/**
 * @brief Update BLE UI state from BLE event
 * 
 * Call this from your BLE event handler.
 * 
 * @param ctx       BLE UI context
 * @param evt_type  BLE event type from agsys_ble.h
 * @param now_ms    Current time in milliseconds
 */
static inline void agsys_ble_ui_on_event(agsys_ble_ui_ctx_t *ctx,
                                          agsys_ble_evt_type_t evt_type,
                                          uint32_t now_ms)
{
    switch (evt_type) {
        case AGSYS_BLE_EVT_CONNECTED:
            ctx->state = BLE_UI_STATE_CONNECTED;
            ctx->visible = true;
            ctx->last_toggle_ms = now_ms;
            ctx->flash_count = 0;
            break;

        case AGSYS_BLE_EVT_DISCONNECTED:
            ctx->state = BLE_UI_STATE_DISCONNECTED;
            ctx->visible = true;
            ctx->last_toggle_ms = now_ms;
            ctx->flash_count = 0;
            break;

        case AGSYS_BLE_EVT_AUTHENTICATED:
            ctx->state = BLE_UI_STATE_AUTHENTICATED;
            ctx->visible = true;
            ctx->flash_count = 0;
            break;

        case AGSYS_BLE_EVT_AUTH_FAILED:
        case AGSYS_BLE_EVT_AUTH_TIMEOUT:
            /* Stay in connected state, keep blinking */
            break;

        default:
            break;
    }
}

/**
 * @brief Set advertising state (call when entering pairing mode)
 * 
 * @param ctx       BLE UI context
 * @param now_ms    Current time in milliseconds
 */
static inline void agsys_ble_ui_set_advertising(agsys_ble_ui_ctx_t *ctx,
                                                  uint32_t now_ms)
{
    ctx->state = BLE_UI_STATE_ADVERTISING;
    ctx->visible = true;
    ctx->last_toggle_ms = now_ms;
    ctx->flash_count = 0;
}

/**
 * @brief Set idle state (call when exiting pairing mode)
 * 
 * @param ctx   BLE UI context
 */
static inline void agsys_ble_ui_set_idle(agsys_ble_ui_ctx_t *ctx)
{
    ctx->state = BLE_UI_STATE_IDLE;
    ctx->visible = false;
    ctx->flash_count = 0;
}

/**
 * @brief Tick the BLE UI animation
 * 
 * Call this periodically (e.g., every 20-50ms) to update blink state.
 * Returns true if visibility changed (caller should update LED/icon).
 * 
 * @param ctx       BLE UI context
 * @param now_ms    Current time in milliseconds
 * @return true if visibility changed
 */
static inline bool agsys_ble_ui_tick(agsys_ble_ui_ctx_t *ctx, uint32_t now_ms)
{
    if (ctx->state == BLE_UI_STATE_IDLE) {
        return false;
    }

    if (ctx->state == BLE_UI_STATE_AUTHENTICATED) {
        /* Solid on - no blinking */
        if (!ctx->visible) {
            ctx->visible = true;
            return true;
        }
        return false;
    }

    uint32_t elapsed = now_ms - ctx->last_toggle_ms;
    uint32_t period_ms;

    switch (ctx->state) {
        case BLE_UI_STATE_ADVERTISING:
            period_ms = BLE_UI_BLINK_SLOW_MS;
            break;

        case BLE_UI_STATE_CONNECTED:
            period_ms = BLE_UI_BLINK_FAST_MS;
            break;

        case BLE_UI_STATE_DISCONNECTED:
            period_ms = BLE_UI_BLINK_TRIPLE_MS;
            if (ctx->flash_count >= BLE_UI_TRIPLE_FLASH_COUNT) {
                /* Done flashing, return to idle */
                ctx->state = BLE_UI_STATE_IDLE;
                ctx->visible = false;
                return true;
            }
            break;

        default:
            return false;
    }

    if (elapsed >= period_ms) {
        ctx->last_toggle_ms = now_ms;
        ctx->visible = !ctx->visible;
        ctx->flash_count++;
        return true;
    }

    return false;
}

/**
 * @brief Check if LED/icon should be visible
 * 
 * @param ctx   BLE UI context
 * @return true if LED/icon should be on
 */
static inline bool agsys_ble_ui_is_visible(const agsys_ble_ui_ctx_t *ctx)
{
    return ctx->visible && ctx->state != BLE_UI_STATE_IDLE;
}

/**
 * @brief Check if BLE UI is active (not idle)
 * 
 * @param ctx   BLE UI context
 * @return true if in any active state
 */
static inline bool agsys_ble_ui_is_active(const agsys_ble_ui_ctx_t *ctx)
{
    return ctx->state != BLE_UI_STATE_IDLE;
}

#endif /* AGSYS_BLE_UI_H */
