/**
 * @file agsys_ble_auth.c
 * @brief BLE PIN Authentication implementation
 */

#include "agsys_ble_auth.h"
#include "agsys_fram.h"
#include "agsys_debug.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

/* FRAM context - set during init */
static agsys_fram_ctx_t *m_fram_ctx = NULL;

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static uint32_t get_time_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static bool is_valid_pin(const uint8_t *pin, uint16_t len)
{
    if (len != AGSYS_PIN_LENGTH) {
        return false;
    }
    
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (pin[i] < '0' || pin[i] > '9') {
            return false;
        }
    }
    
    return true;
}

static bool pins_match(const char *stored, const uint8_t *input)
{
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (stored[i] != (char)input[i]) {
            return false;
        }
    }
    return true;
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool agsys_ble_auth_init(agsys_ble_auth_ctx_t *ctx, 
                          agsys_fram_ctx_t *fram_ctx,
                          uint16_t fram_pin_addr)
{
    if (ctx == NULL || fram_ctx == NULL) {
        return false;
    }
    
    /* Store FRAM context for later use */
    m_fram_ctx = fram_ctx;
    
    memset(ctx, 0, sizeof(agsys_ble_auth_ctx_t));
    ctx->fram_pin_addr = fram_pin_addr;
    ctx->authenticated = false;
    ctx->failed_attempts = 0;
    ctx->lockout_start_ms = 0;
    ctx->auth_time_ms = 0;
    
    /* Load PIN from FRAM */
    agsys_ble_auth_load_pin(ctx);
    
    ctx->initialized = true;
    
    AGSYS_LOG_INFO("BLE Auth: Initialized (FRAM addr=0x%04X)", fram_pin_addr);
    
    return true;
}

/* ==========================================================================
 * AUTHENTICATION
 * ========================================================================== */

uint8_t agsys_ble_auth_verify_pin(agsys_ble_auth_ctx_t *ctx, 
                                   const uint8_t *pin, 
                                   uint16_t len)
{
    if (ctx == NULL || !ctx->initialized || pin == NULL) {
        return AGSYS_AUTH_FAILED;
    }
    
    /* Check lockout */
    if (agsys_ble_auth_is_locked_out(ctx)) {
        AGSYS_LOG_WARNING("BLE Auth: Locked out, %lu sec remaining",
                          agsys_ble_auth_lockout_remaining(ctx));
        return AGSYS_AUTH_LOCKED_OUT;
    }
    
    /* Validate PIN format */
    if (!is_valid_pin(pin, len)) {
        AGSYS_LOG_WARNING("BLE Auth: Invalid PIN format");
        return AGSYS_AUTH_FAILED;
    }
    
    /* Compare PIN */
    if (pins_match(ctx->stored_pin, pin)) {
        ctx->authenticated = true;
        ctx->auth_time_ms = get_time_ms();
        ctx->failed_attempts = 0;
        
        AGSYS_LOG_INFO("BLE Auth: Authenticated");
        return AGSYS_AUTH_AUTHENTICATED;
    }
    
    /* Failed attempt */
    ctx->failed_attempts++;
    AGSYS_LOG_WARNING("BLE Auth: Failed attempt %d/%d", 
                      ctx->failed_attempts, AGSYS_PIN_MAX_ATTEMPTS);
    
    if (ctx->failed_attempts >= AGSYS_PIN_MAX_ATTEMPTS) {
        ctx->lockout_start_ms = get_time_ms();
        AGSYS_LOG_WARNING("BLE Auth: Lockout started");
        return AGSYS_AUTH_LOCKED_OUT;
    }
    
    return AGSYS_AUTH_FAILED;
}

uint8_t agsys_ble_auth_change_pin(agsys_ble_auth_ctx_t *ctx,
                                   const uint8_t *old_pin,
                                   const uint8_t *new_pin)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_AUTH_FAILED;
    }
    
    /* Must be authenticated */
    if (!agsys_ble_auth_is_authenticated(ctx)) {
        AGSYS_LOG_WARNING("BLE Auth: PIN change requires authentication");
        return AGSYS_AUTH_NOT_AUTHENTICATED;
    }
    
    /* Verify old PIN */
    if (!pins_match(ctx->stored_pin, old_pin)) {
        AGSYS_LOG_WARNING("BLE Auth: Old PIN mismatch");
        return AGSYS_AUTH_FAILED;
    }
    
    /* Validate new PIN format */
    if (!is_valid_pin(new_pin, AGSYS_PIN_LENGTH)) {
        AGSYS_LOG_WARNING("BLE Auth: Invalid new PIN format");
        return AGSYS_AUTH_FAILED;
    }
    
    /* Set new PIN */
    memcpy(ctx->stored_pin, new_pin, AGSYS_PIN_LENGTH);
    ctx->stored_pin[AGSYS_PIN_LENGTH] = '\0';
    
    /* Save to FRAM */
    agsys_ble_auth_save_pin(ctx);
    
    AGSYS_LOG_INFO("BLE Auth: PIN changed");
    return AGSYS_AUTH_PIN_CHANGED;
}

bool agsys_ble_auth_is_authenticated(agsys_ble_auth_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (!ctx->authenticated) {
        return false;
    }
    
    /* Check session timeout */
    uint32_t now = get_time_ms();
    uint32_t elapsed = now - ctx->auth_time_ms;
    
    if (elapsed > AGSYS_AUTH_TIMEOUT_MS) {
        ctx->authenticated = false;
        AGSYS_LOG_INFO("BLE Auth: Session timeout");
        return false;
    }
    
    return true;
}

void agsys_ble_auth_clear(agsys_ble_auth_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->authenticated = false;
    ctx->auth_time_ms = 0;
    
    AGSYS_LOG_DEBUG("BLE Auth: Session cleared");
}

uint8_t agsys_ble_auth_get_status(const agsys_ble_auth_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_AUTH_NOT_AUTHENTICATED;
    }
    
    /* Note: Can't call is_locked_out here as it's not const-safe */
    if (ctx->failed_attempts >= AGSYS_PIN_MAX_ATTEMPTS) {
        uint32_t now = get_time_ms();
        uint32_t elapsed = now - ctx->lockout_start_ms;
        if (elapsed < AGSYS_PIN_LOCKOUT_MS) {
            return AGSYS_AUTH_LOCKED_OUT;
        }
    }
    
    if (ctx->authenticated) {
        uint32_t now = get_time_ms();
        uint32_t elapsed = now - ctx->auth_time_ms;
        if (elapsed <= AGSYS_AUTH_TIMEOUT_MS) {
            return AGSYS_AUTH_AUTHENTICATED;
        }
    }
    
    return AGSYS_AUTH_NOT_AUTHENTICATED;
}

bool agsys_ble_auth_is_locked_out(agsys_ble_auth_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (ctx->failed_attempts < AGSYS_PIN_MAX_ATTEMPTS) {
        return false;
    }
    
    uint32_t now = get_time_ms();
    uint32_t elapsed = now - ctx->lockout_start_ms;
    
    if (elapsed >= AGSYS_PIN_LOCKOUT_MS) {
        /* Lockout expired, reset attempts */
        ctx->failed_attempts = 0;
        return false;
    }
    
    return true;
}

uint32_t agsys_ble_auth_lockout_remaining(agsys_ble_auth_ctx_t *ctx)
{
    if (!agsys_ble_auth_is_locked_out(ctx)) {
        return 0;
    }
    
    uint32_t now = get_time_ms();
    uint32_t elapsed = now - ctx->lockout_start_ms;
    uint32_t remaining_ms = AGSYS_PIN_LOCKOUT_MS - elapsed;
    
    return remaining_ms / 1000;
}

/* ==========================================================================
 * PIN MANAGEMENT
 * ========================================================================== */

void agsys_ble_auth_reset_pin(agsys_ble_auth_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    
    memcpy(ctx->stored_pin, AGSYS_DEFAULT_PIN, AGSYS_PIN_LENGTH);
    ctx->stored_pin[AGSYS_PIN_LENGTH] = '\0';
    
    agsys_ble_auth_save_pin(ctx);
    
    AGSYS_LOG_INFO("BLE Auth: PIN reset to default");
}

void agsys_ble_auth_load_pin(agsys_ble_auth_ctx_t *ctx)
{
    if (ctx == NULL || m_fram_ctx == NULL) {
        return;
    }
    
    char loaded[AGSYS_PIN_LENGTH + 1];
    
    /* Read from FRAM */
    agsys_fram_read(m_fram_ctx, ctx->fram_pin_addr, (uint8_t *)loaded, AGSYS_PIN_LENGTH);
    loaded[AGSYS_PIN_LENGTH] = '\0';
    
    /* Validate loaded PIN */
    bool valid = true;
    bool all_ff = true;
    
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (loaded[i] != (char)0xFF) {
            all_ff = false;
        }
        if (loaded[i] < '0' || loaded[i] > '9') {
            valid = false;
        }
    }
    
    if (valid && !all_ff) {
        memcpy(ctx->stored_pin, loaded, AGSYS_PIN_LENGTH);
        ctx->stored_pin[AGSYS_PIN_LENGTH] = '\0';
        AGSYS_LOG_DEBUG("BLE Auth: PIN loaded from FRAM");
    } else {
        /* Use default and save */
        memcpy(ctx->stored_pin, AGSYS_DEFAULT_PIN, AGSYS_PIN_LENGTH);
        ctx->stored_pin[AGSYS_PIN_LENGTH] = '\0';
        agsys_ble_auth_save_pin(ctx);
        AGSYS_LOG_INFO("BLE Auth: Using default PIN");
    }
}

void agsys_ble_auth_save_pin(agsys_ble_auth_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized || m_fram_ctx == NULL) {
        return;
    }
    
    agsys_fram_write(m_fram_ctx, ctx->fram_pin_addr, (uint8_t *)ctx->stored_pin, AGSYS_PIN_LENGTH);
    
    AGSYS_LOG_DEBUG("BLE Auth: PIN saved to FRAM");
}
