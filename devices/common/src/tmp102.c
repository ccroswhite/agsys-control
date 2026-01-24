/**
 * @file tmp102.c
 * @brief TMP102 Digital Temperature Sensor Driver Implementation
 * 
 * @author AgSys
 * @date 2026
 * @license MIT
 */

#include "tmp102.h"
#include <string.h>

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

#define TMP102_RESOLUTION_12BIT     0.0625f     /* 12-bit: 0.0625°C per LSB */
#define TMP102_RESOLUTION_13BIT     0.0625f     /* 13-bit: 0.0625°C per LSB */

/* Conversion time in milliseconds */
#define TMP102_CONVERSION_TIME_MS   26

/* ==========================================================================
 * PRIVATE FUNCTIONS
 * ========================================================================== */

/**
 * @brief Read 16-bit register
 */
static bool read_reg16(tmp102_ctx_t *ctx, uint8_t reg, uint16_t *value)
{
    uint8_t data[2];
    
    if (!ctx->i2c.read(ctx->addr, reg, data, 2, ctx->i2c.user_data)) {
        return false;
    }
    
    *value = ((uint16_t)data[0] << 8) | data[1];
    return true;
}

/**
 * @brief Write 16-bit register
 */
static bool write_reg16(tmp102_ctx_t *ctx, uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
    
    return ctx->i2c.write(ctx->addr, reg, data, 2, ctx->i2c.user_data);
}

/**
 * @brief Modify configuration register bits
 */
static bool modify_config(tmp102_ctx_t *ctx, uint16_t mask, uint16_t value)
{
    uint16_t config;
    
    if (!read_reg16(ctx, TMP102_REG_CONFIG, &config)) {
        return false;
    }
    
    config = (config & ~mask) | (value & mask);
    
    return write_reg16(ctx, TMP102_REG_CONFIG, config);
}

/**
 * @brief Convert temperature register to raw signed value
 */
static int16_t temp_reg_to_raw(uint16_t reg, bool extended_mode)
{
    int16_t raw;
    
    if (extended_mode) {
        /* 13-bit mode: bits [15:3] contain temperature */
        raw = (int16_t)(reg >> 3);
        /* Sign extend from 13 bits */
        if (raw & 0x1000) {
            raw |= 0xE000;
        }
    } else {
        /* 12-bit mode: bits [15:4] contain temperature */
        raw = (int16_t)(reg >> 4);
        /* Sign extend from 12 bits */
        if (raw & 0x0800) {
            raw |= 0xF000;
        }
    }
    
    return raw;
}

/**
 * @brief Convert raw signed value to temperature register format
 */
static uint16_t raw_to_temp_reg(int16_t raw, bool extended_mode)
{
    if (extended_mode) {
        /* 13-bit mode: shift left by 3 */
        return (uint16_t)(raw << 3);
    } else {
        /* 12-bit mode: shift left by 4 */
        return (uint16_t)(raw << 4);
    }
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool tmp102_init(tmp102_ctx_t *ctx, const tmp102_i2c_t *i2c, 
                 const tmp102_config_t *config)
{
    if (ctx == NULL || i2c == NULL || config == NULL) {
        return false;
    }
    
    if (i2c->read == NULL || i2c->write == NULL) {
        return false;
    }
    
    /* Initialize context */
    memset(ctx, 0, sizeof(tmp102_ctx_t));
    ctx->i2c = *i2c;
    ctx->addr = config->addr;
    ctx->extended_mode = config->extended_mode;
    
    /* Check if device is present */
    if (!tmp102_is_present(i2c, config->addr)) {
        return false;
    }
    
    /* Build configuration register value */
    uint16_t cfg = 0;
    
    /* Byte 1 (MSB) */
    cfg |= ((uint16_t)(config->faults & 0x03) << 11);       /* F1:F0 */
    cfg |= ((uint16_t)(config->alert_pol & 0x01) << 10);    /* POL */
    cfg |= ((uint16_t)(config->therm_mode & 0x01) << 9);    /* TM */
    cfg |= ((uint16_t)(config->shutdown & 0x01) << 8);      /* SD */
    
    /* Byte 2 (LSB) */
    cfg |= ((uint16_t)(config->rate & 0x03) << 6);          /* CR1:CR0 */
    cfg |= ((uint16_t)(config->extended_mode & 0x01) << 4); /* EM */
    
    /* Write configuration */
    if (!write_reg16(ctx, TMP102_REG_CONFIG, cfg)) {
        return false;
    }
    
    ctx->initialized = true;
    return true;
}

bool tmp102_is_present(const tmp102_i2c_t *i2c, uint8_t addr)
{
    if (i2c == NULL || i2c->read == NULL) {
        return false;
    }
    
    /* Try to read the configuration register */
    uint8_t data[2];
    if (!i2c->read(addr, TMP102_REG_CONFIG, data, 2, i2c->user_data)) {
        return false;
    }
    
    /* Check resolution bits (R1:R0) - should be 11 for 12-bit resolution */
    uint8_t res_bits = (data[0] >> 5) & 0x03;
    return (res_bits == 0x03);
}

bool tmp102_reset(tmp102_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* Default configuration: 4Hz, comparator mode, active-low alert */
    uint16_t cfg = (TMP102_RATE_4HZ << 6);
    
    if (!write_reg16(ctx, TMP102_REG_CONFIG, cfg)) {
        return false;
    }
    
    ctx->extended_mode = false;
    
    /* Reset thresholds to defaults: T_LOW = 75°C, T_HIGH = 80°C */
    int16_t t_low_raw = tmp102_celsius_to_raw(75.0f, false);
    int16_t t_high_raw = tmp102_celsius_to_raw(80.0f, false);
    
    if (!write_reg16(ctx, TMP102_REG_TLOW, raw_to_temp_reg(t_low_raw, false))) {
        return false;
    }
    
    if (!write_reg16(ctx, TMP102_REG_THIGH, raw_to_temp_reg(t_high_raw, false))) {
        return false;
    }
    
    return true;
}

/* ==========================================================================
 * TEMPERATURE READING
 * ========================================================================== */

bool tmp102_read_temp_c(tmp102_ctx_t *ctx, float *temp_c)
{
    int16_t raw;
    
    if (!tmp102_read_raw(ctx, &raw)) {
        return false;
    }
    
    *temp_c = tmp102_raw_to_celsius(raw, ctx->extended_mode);
    return true;
}

bool tmp102_read_temp_f(tmp102_ctx_t *ctx, float *temp_f)
{
    float temp_c;
    
    if (!tmp102_read_temp_c(ctx, &temp_c)) {
        return false;
    }
    
    *temp_f = (temp_c * 9.0f / 5.0f) + 32.0f;
    return true;
}

bool tmp102_read_raw(tmp102_ctx_t *ctx, int16_t *raw)
{
    if (ctx == NULL || !ctx->initialized || raw == NULL) {
        return false;
    }
    
    uint16_t reg;
    if (!read_reg16(ctx, TMP102_REG_TEMP, &reg)) {
        return false;
    }
    
    *raw = temp_reg_to_raw(reg, ctx->extended_mode);
    return true;
}

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

bool tmp102_set_rate(tmp102_ctx_t *ctx, tmp102_rate_t rate)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* CR1:CR0 are bits 7:6 of byte 2 (bits 7:6 of the 16-bit value) */
    uint16_t mask = 0x00C0;
    uint16_t value = ((uint16_t)(rate & 0x03) << 6);
    
    return modify_config(ctx, mask, value);
}

bool tmp102_set_extended_mode(tmp102_ctx_t *ctx, bool enable)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* EM is bit 4 of byte 2 (bit 4 of the 16-bit value) */
    uint16_t mask = 0x0010;
    uint16_t value = enable ? 0x0010 : 0x0000;
    
    if (!modify_config(ctx, mask, value)) {
        return false;
    }
    
    ctx->extended_mode = enable;
    return true;
}

bool tmp102_set_shutdown(tmp102_ctx_t *ctx, bool shutdown)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* SD is bit 0 of byte 1 (bit 8 of the 16-bit value) */
    uint16_t mask = 0x0100;
    uint16_t value = shutdown ? 0x0100 : 0x0000;
    
    return modify_config(ctx, mask, value);
}

bool tmp102_one_shot(tmp102_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* OS is bit 7 of byte 1 (bit 15 of the 16-bit value) */
    /* Writing 1 triggers one-shot conversion */
    uint16_t mask = 0x8000;
    uint16_t value = 0x8000;
    
    return modify_config(ctx, mask, value);
}

bool tmp102_conversion_ready(tmp102_ctx_t *ctx, bool *ready)
{
    if (ctx == NULL || !ctx->initialized || ready == NULL) {
        return false;
    }
    
    uint16_t config;
    if (!read_reg16(ctx, TMP102_REG_CONFIG, &config)) {
        return false;
    }
    
    /* OS bit reads as 1 when conversion is complete */
    *ready = (config & 0x8000) != 0;
    return true;
}

/* ==========================================================================
 * ALERT CONFIGURATION
 * ========================================================================== */

bool tmp102_set_alert_thresholds(tmp102_ctx_t *ctx, float t_low, float t_high)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    int16_t raw_low = tmp102_celsius_to_raw(t_low, ctx->extended_mode);
    int16_t raw_high = tmp102_celsius_to_raw(t_high, ctx->extended_mode);
    
    if (!write_reg16(ctx, TMP102_REG_TLOW, raw_to_temp_reg(raw_low, ctx->extended_mode))) {
        return false;
    }
    
    if (!write_reg16(ctx, TMP102_REG_THIGH, raw_to_temp_reg(raw_high, ctx->extended_mode))) {
        return false;
    }
    
    return true;
}

bool tmp102_get_alert_thresholds(tmp102_ctx_t *ctx, float *t_low, float *t_high)
{
    if (ctx == NULL || !ctx->initialized || t_low == NULL || t_high == NULL) {
        return false;
    }
    
    uint16_t reg_low, reg_high;
    
    if (!read_reg16(ctx, TMP102_REG_TLOW, &reg_low)) {
        return false;
    }
    
    if (!read_reg16(ctx, TMP102_REG_THIGH, &reg_high)) {
        return false;
    }
    
    int16_t raw_low = temp_reg_to_raw(reg_low, ctx->extended_mode);
    int16_t raw_high = temp_reg_to_raw(reg_high, ctx->extended_mode);
    
    *t_low = tmp102_raw_to_celsius(raw_low, ctx->extended_mode);
    *t_high = tmp102_raw_to_celsius(raw_high, ctx->extended_mode);
    
    return true;
}

bool tmp102_set_alert_polarity(tmp102_ctx_t *ctx, tmp102_alert_polarity_t polarity)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* POL is bit 2 of byte 1 (bit 10 of the 16-bit value) */
    uint16_t mask = 0x0400;
    uint16_t value = (polarity == TMP102_ALERT_ACTIVE_HIGH) ? 0x0400 : 0x0000;
    
    return modify_config(ctx, mask, value);
}

bool tmp102_set_thermostat_mode(tmp102_ctx_t *ctx, tmp102_thermostat_mode_t mode)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* TM is bit 1 of byte 1 (bit 9 of the 16-bit value) */
    uint16_t mask = 0x0200;
    uint16_t value = (mode == TMP102_MODE_INTERRUPT) ? 0x0200 : 0x0000;
    
    return modify_config(ctx, mask, value);
}

bool tmp102_set_fault_queue(tmp102_ctx_t *ctx, tmp102_faults_t faults)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* F1:F0 are bits 4:3 of byte 1 (bits 12:11 of the 16-bit value) */
    uint16_t mask = 0x1800;
    uint16_t value = ((uint16_t)(faults & 0x03) << 11);
    
    return modify_config(ctx, mask, value);
}

bool tmp102_read_alert_status(tmp102_ctx_t *ctx, bool *alert)
{
    if (ctx == NULL || !ctx->initialized || alert == NULL) {
        return false;
    }
    
    uint16_t config;
    if (!read_reg16(ctx, TMP102_REG_CONFIG, &config)) {
        return false;
    }
    
    /* AL is bit 5 of byte 2 (bit 5 of the 16-bit value) */
    *alert = (config & 0x0020) != 0;
    return true;
}

/* ==========================================================================
 * LOW-LEVEL REGISTER ACCESS
 * ========================================================================== */

bool tmp102_read_config(tmp102_ctx_t *ctx, uint16_t *config)
{
    if (ctx == NULL || !ctx->initialized || config == NULL) {
        return false;
    }
    
    return read_reg16(ctx, TMP102_REG_CONFIG, config);
}

bool tmp102_write_config(tmp102_ctx_t *ctx, uint16_t config)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    return write_reg16(ctx, TMP102_REG_CONFIG, config);
}

/* ==========================================================================
 * UTILITY FUNCTIONS
 * ========================================================================== */

float tmp102_raw_to_celsius(int16_t raw, bool extended_mode)
{
    (void)extended_mode;  /* Resolution is same for both modes */
    return (float)raw * TMP102_RESOLUTION_12BIT;
}

int16_t tmp102_celsius_to_raw(float temp_c, bool extended_mode)
{
    (void)extended_mode;  /* Resolution is same for both modes */
    return (int16_t)(temp_c / TMP102_RESOLUTION_12BIT);
}
