/**
 * @file ads131m0x.c
 * @brief ADS131M0x 24-bit Delta-Sigma ADC Driver Implementation
 * 
 * @author AgSys
 * @date 2026
 * @license MIT
 */

#include "ads131m0x.h"
#include <string.h>

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

/* Sample rates for each OSR (with 8.192 MHz clock) */
static const uint32_t s_osr_sample_rates[] = {
    32000,  /* OSR_128 */
    16000,  /* OSR_256 */
    8000,   /* OSR_512 */
    4000,   /* OSR_1024 */
    2000,   /* OSR_2048 */
    1000,   /* OSR_4096 */
    500,    /* OSR_8192 */
    250,    /* OSR_16384 */
};

/* Gain multipliers */
static const uint8_t s_gain_values[] = {1, 2, 4, 8, 16, 32, 64, 128};

/* Maximum SPI frame size: STATUS + N channels, 32-bit words */
/* For compile-time selected device: (1 + NUM_CHANNELS) * 4 bytes max */
#define MAX_FRAME_SIZE  ((1 + ADS131M0X_NUM_CHANNELS) * 4)

/* ==========================================================================
 * PRIVATE HELPERS
 * ========================================================================== */

/**
 * @brief Calculate frame size based on word length
 */
static uint8_t calc_frame_size(ads131m0x_word_length_t word_len)
{
    uint8_t word_bytes;
    switch (word_len) {
        case ADS131M0X_WORD_16BIT: word_bytes = 2; break;
        case ADS131M0X_WORD_24BIT: word_bytes = 3; break;
        case ADS131M0X_WORD_32BIT: word_bytes = 4; break;
        default: word_bytes = 3; break;
    }
    /* Frame = STATUS word + N channel words (N is compile-time constant) */
    return word_bytes * (1 + ADS131M0X_NUM_CHANNELS);
}

/**
 * @brief Build command word
 */
static uint16_t build_command(uint16_t cmd, uint8_t addr)
{
    return cmd | ((uint16_t)addr << 7);
}

/**
 * @brief Get channel config register address
 */
static uint8_t get_ch_cfg_reg(uint8_t channel)
{
    return ADS131M0X_REG_CH0_CFG + (channel * ADS131M0X_CH_REG_STRIDE);
}

/**
 * @brief Get channel offset calibration MSB register address
 */
static uint8_t get_ocal_msb_reg(uint8_t channel)
{
    return ADS131M0X_REG_CH0_OCAL_MSB + (channel * ADS131M0X_CH_REG_STRIDE);
}

/**
 * @brief Get channel offset calibration LSB register address
 */
static uint8_t get_ocal_lsb_reg(uint8_t channel)
{
    return ADS131M0X_REG_CH0_OCAL_LSB + (channel * ADS131M0X_CH_REG_STRIDE);
}

/**
 * @brief Get channel gain calibration MSB register address
 */
static uint8_t get_gcal_msb_reg(uint8_t channel)
{
    return ADS131M0X_REG_CH0_GCAL_MSB + (channel * ADS131M0X_CH_REG_STRIDE);
}

/**
 * @brief Get channel gain calibration LSB register address
 */
static uint8_t get_gcal_lsb_reg(uint8_t channel)
{
    return ADS131M0X_REG_CH0_GCAL_LSB + (channel * ADS131M0X_CH_REG_STRIDE);
}

/**
 * @brief Perform SPI transfer
 */
static bool spi_transfer(ads131m0x_ctx_t *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (ctx->hal.spi_transfer == NULL) {
        return false;
    }
    return ctx->hal.spi_transfer(tx, rx, len, ctx->hal.user_data);
}

/**
 * @brief Delay in milliseconds
 */
static void delay_ms(ads131m0x_ctx_t *ctx, uint32_t ms)
{
    if (ctx->hal.delay_ms != NULL) {
        ctx->hal.delay_ms(ms, ctx->hal.user_data);
    }
}

/**
 * @brief Set GPIO pin
 */
static void gpio_write(ads131m0x_ctx_t *ctx, uint8_t pin, bool value)
{
    if (ctx->hal.gpio_write != NULL) {
        ctx->hal.gpio_write(pin, value, ctx->hal.user_data);
    }
}

/**
 * @brief Read GPIO pin
 */
static bool gpio_read(ads131m0x_ctx_t *ctx, uint8_t pin)
{
    if (ctx->hal.gpio_read != NULL) {
        return ctx->hal.gpio_read(pin, ctx->hal.user_data);
    }
    return false;
}

/**
 * @brief Send command and receive response
 */
static bool send_command(ads131m0x_ctx_t *ctx, uint16_t cmd)
{
    uint8_t tx[MAX_FRAME_SIZE] = {0};
    uint8_t rx[MAX_FRAME_SIZE] = {0};
    (void)rx;  /* Response not needed for commands */
    
    /* Build command in first word */
    if (ctx->word_length == ADS131M0X_WORD_16BIT) {
        tx[0] = (cmd >> 8) & 0xFF;
        tx[1] = cmd & 0xFF;
    } else {
        /* 24-bit or 32-bit: command in upper 16 bits, padded */
        tx[0] = (cmd >> 8) & 0xFF;
        tx[1] = cmd & 0xFF;
        tx[2] = 0x00;
        if (ctx->word_length == ADS131M0X_WORD_32BIT) {
            tx[3] = 0x00;
        }
    }
    
    return spi_transfer(ctx, tx, rx, ctx->frame_size);
}

/* ==========================================================================
 * REGISTER ACCESS
 * ========================================================================== */

bool ads131m0x_read_reg(ads131m0x_ctx_t *ctx, uint8_t reg, uint16_t *value)
{
    if (ctx == NULL || !ctx->initialized || value == NULL) {
        return false;
    }
    
    uint8_t tx[MAX_FRAME_SIZE] = {0};
    uint8_t rx[MAX_FRAME_SIZE] = {0};
    
    /* Build read command */
    uint16_t cmd = build_command(ADS131M0X_CMD_RREG, reg);
    
    if (ctx->word_length == ADS131M0X_WORD_16BIT) {
        tx[0] = (cmd >> 8) & 0xFF;
        tx[1] = cmd & 0xFF;
    } else {
        tx[0] = (cmd >> 8) & 0xFF;
        tx[1] = cmd & 0xFF;
        tx[2] = 0x00;
    }
    
    /* First transfer sends command */
    if (!spi_transfer(ctx, tx, rx, ctx->frame_size)) {
        return false;
    }
    
    /* Second transfer gets response */
    memset(tx, 0, sizeof(tx));
    if (!spi_transfer(ctx, tx, rx, ctx->frame_size)) {
        return false;
    }
    
    /* Response is in first word */
    *value = ((uint16_t)rx[0] << 8) | rx[1];
    
    return true;
}

bool ads131m0x_write_reg(ads131m0x_ctx_t *ctx, uint8_t reg, uint16_t value)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint8_t tx[MAX_FRAME_SIZE] = {0};
    uint8_t rx[MAX_FRAME_SIZE] = {0};
    
    /* Build write command */
    uint16_t cmd = build_command(ADS131M0X_CMD_WREG, reg);
    
    uint8_t word_bytes = (ctx->word_length == ADS131M0X_WORD_16BIT) ? 2 :
                         (ctx->word_length == ADS131M0X_WORD_24BIT) ? 3 : 4;
    
    /* Command word */
    tx[0] = (cmd >> 8) & 0xFF;
    tx[1] = cmd & 0xFF;
    if (word_bytes >= 3) tx[2] = 0x00;
    if (word_bytes >= 4) tx[3] = 0x00;
    
    /* Data word */
    tx[word_bytes + 0] = (value >> 8) & 0xFF;
    tx[word_bytes + 1] = value & 0xFF;
    if (word_bytes >= 3) tx[word_bytes + 2] = 0x00;
    if (word_bytes >= 4) tx[word_bytes + 3] = 0x00;
    
    return spi_transfer(ctx, tx, rx, ctx->frame_size);
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool ads131m0x_init(ads131m0x_ctx_t *ctx, const ads131m0x_hal_t *hal,
                    const ads131m0x_config_t *config)
{
    if (ctx == NULL || hal == NULL || config == NULL) {
        return false;
    }
    
    if (hal->spi_transfer == NULL) {
        return false;
    }
    
    /* Initialize context */
    memset(ctx, 0, sizeof(ads131m0x_ctx_t));
    ctx->hal = *hal;
    ctx->sync_reset_pin = config->sync_reset_pin;
    ctx->drdy_pin = config->drdy_pin;
    ctx->osr = config->osr;
    ctx->power_mode = config->power_mode;
    ctx->word_length = config->word_length;
    memcpy(ctx->gain, config->gain, sizeof(ctx->gain));
    
    /* Calculate frame size for compile-time channel count */
    ctx->frame_size = calc_frame_size(ctx->word_length);
    ctx->initialized = true;  /* Temporarily set for register access */
    
    /* Hardware reset via SYNC/RESET pin */
    gpio_write(ctx, ctx->sync_reset_pin, false);
    delay_ms(ctx, 1);
    gpio_write(ctx, ctx->sync_reset_pin, true);
    delay_ms(ctx, 10);
    
    /* Software reset */
    if (!ads131m0x_reset(ctx)) {
        ctx->initialized = false;
        return false;
    }
    
    /* Read and verify device ID */
    uint16_t id;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_ID, &id)) {
        ctx->initialized = false;
        return false;
    }
    
    ctx->device_id_raw = id;
    uint8_t id_upper = (id >> 8) & 0xFF;
    
    /* Verify device ID matches compile-time selection */
    if (id_upper != ADS131M0X_DEVICE_ID) {
        /* Device ID mismatch - wrong chip or communication error */
        ctx->initialized = false;
        return false;
    }
    
    /* Configure MODE register */
    uint16_t mode = ((uint16_t)ctx->word_length << 8);
    if (!ads131m0x_write_reg(ctx, ADS131M0X_REG_MODE, mode)) {
        ctx->initialized = false;
        return false;
    }
    
    /* Configure CLOCK register - enable all channels */
    uint16_t clock = 0;
    for (uint8_t i = 0; i < ADS131M0X_NUM_CHANNELS; i++) {
        clock |= (1 << (8 + i));  /* CHn_EN bits start at bit 8 */
    }
    clock |= ((uint16_t)ctx->osr << 2);
    clock |= ctx->power_mode;
    
    if (!ads131m0x_write_reg(ctx, ADS131M0X_REG_CLOCK, clock)) {
        ctx->initialized = false;
        return false;
    }
    
    /* Configure GAIN1 register: CH0-CH3 (4 bits each) */
    uint16_t gain1 = 0;
    for (uint8_t i = 0; i < 4 && i < ADS131M0X_NUM_CHANNELS; i++) {
        gain1 |= ((uint16_t)ctx->gain[i] << (i * 4));
    }
    if (!ads131m0x_write_reg(ctx, ADS131M0X_REG_GAIN1, gain1)) {
        ctx->initialized = false;
        return false;
    }
    
#if ADS131M0X_HAS_GAIN2
    /* GAIN2 register: CH4-CH7 (only for M06/M08) */
    uint16_t gain2 = 0;
    for (uint8_t i = 4; i < ADS131M0X_NUM_CHANNELS; i++) {
        gain2 |= ((uint16_t)ctx->gain[i] << ((i - 4) * 4));
    }
    if (!ads131m0x_write_reg(ctx, ADS131M0X_REG_GAIN2, gain2)) {
        ctx->initialized = false;
        return false;
    }
#endif
    
    /* Enable CRC if requested */
    if (config->enable_crc) {
        if (!ads131m0x_enable_crc(ctx, true, true, true)) {
            ctx->initialized = false;
            return false;
        }
    }
    
    return true;
}

bool ads131m0x_reset(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    
    if (!send_command(ctx, ADS131M0X_CMD_RESET)) {
        return false;
    }
    
    delay_ms(ctx, 5);
    return true;
}

bool ads131m0x_verify_device_id(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint8_t id_upper = (ctx->device_id_raw >> 8) & 0xFF;
    return (id_upper == ADS131M0X_DEVICE_ID);
}

/* ==========================================================================
 * SAMPLING
 * ========================================================================== */

bool ads131m0x_read_sample(ads131m0x_ctx_t *ctx, ads131m0x_sample_t *sample)
{
    if (ctx == NULL || !ctx->initialized || sample == NULL) {
        return false;
    }
    
    uint8_t tx[MAX_FRAME_SIZE] = {0};
    uint8_t rx[MAX_FRAME_SIZE] = {0};
    
    if (!spi_transfer(ctx, tx, rx, ctx->frame_size)) {
        sample->valid = false;
        return false;
    }
    
    uint8_t word_bytes = (ctx->word_length == ADS131M0X_WORD_16BIT) ? 2 :
                         (ctx->word_length == ADS131M0X_WORD_24BIT) ? 3 : 4;
    
    /* Parse status word */
    sample->status = ((uint16_t)rx[0] << 8) | rx[1];
    sample->crc_error = (sample->status & ADS131M0X_STATUS_CRC_ERR) != 0;
    
    /* Parse channel data (compile-time channel count) */
    for (uint8_t ch = 0; ch < ADS131M0X_NUM_CHANNELS; ch++) {
        uint8_t offset = word_bytes * (1 + ch);  /* Skip status word */
        
        int32_t raw;
        if (ctx->word_length == ADS131M0X_WORD_16BIT) {
            /* 16-bit mode: data in upper 16 bits, sign extend */
            raw = ((int32_t)(int16_t)(((uint16_t)rx[offset] << 8) | rx[offset + 1])) << 8;
        } else {
            /* 24-bit or 32-bit mode: data in upper 24 bits */
            raw = ((int32_t)rx[offset] << 16) | ((int32_t)rx[offset + 1] << 8) | rx[offset + 2];
            /* Sign extend from 24-bit to 32-bit */
            if (raw & 0x800000) {
                raw |= 0xFF000000;
            }
        }
        
        sample->ch[ch] = raw;
    }
    
    sample->valid = true;
    return true;
}

bool ads131m0x_data_ready(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    /* DRDY is active low */
    return !gpio_read(ctx, ctx->drdy_pin);
}

bool ads131m0x_wait_data_ready(ads131m0x_ctx_t *ctx, uint32_t timeout_ms)
{
    if (ctx == NULL) {
        return false;
    }
    
    /* Simple polling with 1ms resolution */
    for (uint32_t i = 0; i < timeout_ms; i++) {
        if (ads131m0x_data_ready(ctx)) {
            return true;
        }
        delay_ms(ctx, 1);
    }
    
    return false;
}

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

bool ads131m0x_set_osr(ads131m0x_ctx_t *ctx, ads131m0x_osr_t osr)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint16_t clock;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_CLOCK, &clock)) {
        return false;
    }
    
    clock = (clock & ~ADS131M0X_CLK_OSR_MASK) | ((uint16_t)osr << 2);
    
    if (!ads131m0x_write_reg(ctx, ADS131M0X_REG_CLOCK, clock)) {
        return false;
    }
    
    ctx->osr = osr;
    return true;
}

bool ads131m0x_set_gain(ads131m0x_ctx_t *ctx, uint8_t channel, ads131m0x_gain_t gain)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS) {
        return false;
    }
    
    /* Determine which GAIN register to use */
#if ADS131M0X_HAS_GAIN2
    uint8_t reg = (channel < 4) ? ADS131M0X_REG_GAIN1 : ADS131M0X_REG_GAIN2;
#else
    uint8_t reg = ADS131M0X_REG_GAIN1;
#endif
    uint8_t shift = (channel % 4) * 4;
    
    uint16_t gain_reg;
    if (!ads131m0x_read_reg(ctx, reg, &gain_reg)) {
        return false;
    }
    
    gain_reg = (gain_reg & ~(0x0F << shift)) | ((uint16_t)gain << shift);
    
    if (!ads131m0x_write_reg(ctx, reg, gain_reg)) {
        return false;
    }
    
    ctx->gain[channel] = gain;
    return true;
}

bool ads131m0x_set_power_mode(ads131m0x_ctx_t *ctx, ads131m0x_power_t mode)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint16_t clock;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_CLOCK, &clock)) {
        return false;
    }
    
    clock = (clock & ~ADS131M0X_CLK_PWR_MASK) | mode;
    
    if (!ads131m0x_write_reg(ctx, ADS131M0X_REG_CLOCK, clock)) {
        return false;
    }
    
    ctx->power_mode = mode;
    return true;
}

bool ads131m0x_set_channel_enable(ads131m0x_ctx_t *ctx, uint8_t channel, bool enable)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS) {
        return false;
    }
    
    uint16_t clock;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_CLOCK, &clock)) {
        return false;
    }
    
    uint16_t mask = (1 << (8 + channel));
    if (enable) {
        clock |= mask;
    } else {
        clock &= ~mask;
    }
    
    return ads131m0x_write_reg(ctx, ADS131M0X_REG_CLOCK, clock);
}

bool ads131m0x_set_input_mux(ads131m0x_ctx_t *ctx, uint8_t channel, ads131m0x_mux_t mux)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS) {
        return false;
    }
    
    uint16_t ch_cfg;
    if (!ads131m0x_read_reg(ctx, get_ch_cfg_reg(channel), &ch_cfg)) {
        return false;
    }
    
    ch_cfg = (ch_cfg & ~ADS131M0X_CHCFG_MUX_MASK) | ((uint16_t)mux << 10);
    
    return ads131m0x_write_reg(ctx, get_ch_cfg_reg(channel), ch_cfg);
}

/* ==========================================================================
 * POWER MANAGEMENT
 * ========================================================================== */

bool ads131m0x_standby(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    return send_command(ctx, ADS131M0X_CMD_STANDBY);
}

bool ads131m0x_wakeup(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    return send_command(ctx, ADS131M0X_CMD_WAKEUP);
}

/* ==========================================================================
 * CALIBRATION
 * ========================================================================== */

bool ads131m0x_set_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, int32_t offset)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS) {
        return false;
    }
    
    /* Offset is 24-bit signed, stored in two 16-bit registers */
    uint16_t msb = (offset >> 8) & 0xFFFF;
    uint16_t lsb = (offset & 0xFF) << 8;
    
    if (!ads131m0x_write_reg(ctx, get_ocal_msb_reg(channel), msb)) {
        return false;
    }
    if (!ads131m0x_write_reg(ctx, get_ocal_lsb_reg(channel), lsb)) {
        return false;
    }
    
    return true;
}

bool ads131m0x_get_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, int32_t *offset)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS || offset == NULL) {
        return false;
    }
    
    uint16_t msb, lsb;
    if (!ads131m0x_read_reg(ctx, get_ocal_msb_reg(channel), &msb)) {
        return false;
    }
    if (!ads131m0x_read_reg(ctx, get_ocal_lsb_reg(channel), &lsb)) {
        return false;
    }
    
    /* Reconstruct 24-bit signed value */
    int32_t val = ((int32_t)msb << 8) | ((lsb >> 8) & 0xFF);
    
    /* Sign extend from 24-bit to 32-bit */
    if (val & 0x800000) {
        val |= 0xFF000000;
    }
    
    *offset = val;
    return true;
}

bool ads131m0x_set_gain_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint32_t gain_cal)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS) {
        return false;
    }
    
    /* Gain is 24-bit unsigned */
    gain_cal &= 0xFFFFFF;
    
    uint16_t msb = (gain_cal >> 8) & 0xFFFF;
    uint16_t lsb = (gain_cal & 0xFF) << 8;
    
    if (!ads131m0x_write_reg(ctx, get_gcal_msb_reg(channel), msb)) {
        return false;
    }
    if (!ads131m0x_write_reg(ctx, get_gcal_lsb_reg(channel), lsb)) {
        return false;
    }
    
    return true;
}

bool ads131m0x_get_gain_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint32_t *gain_cal)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS || gain_cal == NULL) {
        return false;
    }
    
    uint16_t msb, lsb;
    if (!ads131m0x_read_reg(ctx, get_gcal_msb_reg(channel), &msb)) {
        return false;
    }
    if (!ads131m0x_read_reg(ctx, get_gcal_lsb_reg(channel), &lsb)) {
        return false;
    }
    
    *gain_cal = ((uint32_t)msb << 8) | ((lsb >> 8) & 0xFF);
    return true;
}

bool ads131m0x_auto_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t num_samples)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS || num_samples == 0) {
        return false;
    }
    
    /* Save current mux setting */
    uint16_t ch_cfg;
    if (!ads131m0x_read_reg(ctx, get_ch_cfg_reg(channel), &ch_cfg)) {
        return false;
    }
    
    /* Set mux to shorted inputs */
    uint16_t shorted_cfg = (ch_cfg & ~ADS131M0X_CHCFG_MUX_MASK) | 
                           ((uint16_t)ADS131M0X_MUX_SHORTED << 10);
    if (!ads131m0x_write_reg(ctx, get_ch_cfg_reg(channel), shorted_cfg)) {
        return false;
    }
    
    /* Wait for settling */
    delay_ms(ctx, 10);
    
    /* Accumulate samples */
    int64_t sum = 0;
    uint16_t valid_samples = 0;
    
    for (uint16_t i = 0; i < num_samples; i++) {
        /* Wait for data ready */
        if (!ads131m0x_wait_data_ready(ctx, 100)) {
            continue;
        }
        
        ads131m0x_sample_t sample;
        if (ads131m0x_read_sample(ctx, &sample) && sample.valid) {
            sum += sample.ch[channel];
            valid_samples++;
        }
    }
    
    /* Restore original mux setting */
    ads131m0x_write_reg(ctx, get_ch_cfg_reg(channel), ch_cfg);
    
    if (valid_samples == 0) {
        return false;
    }
    
    /* Calculate and store average offset */
    int32_t avg_offset = (int32_t)(sum / valid_samples);
    return ads131m0x_set_offset_cal(ctx, channel, avg_offset);
}

bool ads131m0x_reset_calibration(ads131m0x_ctx_t *ctx, uint8_t channel)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS) {
        return false;
    }
    
    if (!ads131m0x_set_offset_cal(ctx, channel, ADS131M0X_OCAL_DEFAULT)) {
        return false;
    }
    
    if (!ads131m0x_set_gain_cal(ctx, channel, ADS131M0X_GCAL_DEFAULT)) {
        return false;
    }
    
    return true;
}

bool ads131m0x_set_phase_delay(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t phase_delay)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS) {
        return false;
    }
    
    phase_delay &= ADS131M0X_CHCFG_PHASE_MASK;
    
    uint16_t ch_cfg;
    if (!ads131m0x_read_reg(ctx, get_ch_cfg_reg(channel), &ch_cfg)) {
        return false;
    }
    
    ch_cfg = (ch_cfg & ~ADS131M0X_CHCFG_PHASE_MASK) | phase_delay;
    
    return ads131m0x_write_reg(ctx, get_ch_cfg_reg(channel), ch_cfg);
}

bool ads131m0x_get_phase_delay(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t *phase_delay)
{
    if (ctx == NULL || !ctx->initialized || channel >= ADS131M0X_NUM_CHANNELS || phase_delay == NULL) {
        return false;
    }
    
    uint16_t ch_cfg;
    if (!ads131m0x_read_reg(ctx, get_ch_cfg_reg(channel), &ch_cfg)) {
        return false;
    }
    
    *phase_delay = ch_cfg & ADS131M0X_CHCFG_PHASE_MASK;
    return true;
}

/* ==========================================================================
 * GLOBAL-CHOP MODE
 * ========================================================================== */

bool ads131m0x_enable_global_chop(ads131m0x_ctx_t *ctx, ads131m0x_gc_delay_t delay)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint16_t cfg;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_CFG, &cfg)) {
        return false;
    }
    
    cfg &= ~ADS131M0X_CFG_GC_DLY_MASK;
    cfg |= ((uint16_t)delay << 9);
    cfg |= ADS131M0X_CFG_GC_EN;
    
    return ads131m0x_write_reg(ctx, ADS131M0X_REG_CFG, cfg);
}

bool ads131m0x_disable_global_chop(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint16_t cfg;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_CFG, &cfg)) {
        return false;
    }
    
    cfg &= ~ADS131M0X_CFG_GC_EN;
    
    return ads131m0x_write_reg(ctx, ADS131M0X_REG_CFG, cfg);
}

bool ads131m0x_is_global_chop_enabled(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint16_t cfg;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_CFG, &cfg)) {
        return false;
    }
    
    return (cfg & ADS131M0X_CFG_GC_EN) != 0;
}

/* ==========================================================================
 * CRC
 * ========================================================================== */

bool ads131m0x_enable_crc(ads131m0x_ctx_t *ctx, bool enable_input, 
                          bool enable_output, bool use_ccitt)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint16_t mode;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_MODE, &mode)) {
        return false;
    }
    
    mode &= ~(ADS131M0X_MODE_REG_CRC_EN | ADS131M0X_MODE_RX_CRC_EN | ADS131M0X_MODE_CRC_TYPE);
    
    if (enable_output) mode |= ADS131M0X_MODE_REG_CRC_EN;
    if (enable_input) mode |= ADS131M0X_MODE_RX_CRC_EN;
    if (!use_ccitt) mode |= ADS131M0X_MODE_CRC_TYPE;
    
    if (!ads131m0x_write_reg(ctx, ADS131M0X_REG_MODE, mode)) {
        return false;
    }
    
    ctx->crc_enabled = enable_input || enable_output;
    return true;
}

bool ads131m0x_disable_crc(ads131m0x_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint16_t mode;
    if (!ads131m0x_read_reg(ctx, ADS131M0X_REG_MODE, &mode)) {
        return false;
    }
    
    mode &= ~(ADS131M0X_MODE_REG_CRC_EN | ADS131M0X_MODE_RX_CRC_EN);
    
    if (!ads131m0x_write_reg(ctx, ADS131M0X_REG_MODE, mode)) {
        return false;
    }
    
    ctx->crc_enabled = false;
    return true;
}

bool ads131m0x_read_regmap_crc(ads131m0x_ctx_t *ctx, uint16_t *crc)
{
    if (ctx == NULL || !ctx->initialized || crc == NULL) {
        return false;
    }
    
    return ads131m0x_read_reg(ctx, ADS131M0X_REG_REGMAP_CRC, crc);
}

/* ==========================================================================
 * UTILITY FUNCTIONS
 * ========================================================================== */

uint32_t ads131m0x_get_sample_rate(ads131m0x_osr_t osr)
{
    if (osr > ADS131M0X_OSR_16384) {
        return 0;
    }
    return s_osr_sample_rates[osr];
}

float ads131m0x_to_voltage(int32_t raw, ads131m0x_gain_t gain, float vref)
{
    float full_scale = vref / (float)s_gain_values[gain];
    return ((float)raw / 8388608.0f) * full_scale;
}

uint8_t ads131m0x_get_gain_multiplier(ads131m0x_gain_t gain)
{
    if (gain > ADS131M0X_GAIN_128X) {
        return 1;
    }
    return s_gain_values[gain];
}

