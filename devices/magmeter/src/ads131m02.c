/**
 * @file ads131m02.c
 * @brief ADS131M02 24-bit Delta-Sigma ADC Driver Implementation
 * 
 * Uses shared agsys_spi driver for DMA-based SPI transfers.
 */

#include "ads131m02.h"
#include "agsys_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

#define ADS131M02_DEVICE_ID         0x0082  /* Expected device ID */
#define ADS131M02_WORD_SIZE         3       /* 24-bit words */

/* Sample rates for each OSR (with 8.192 MHz clock) */
static const uint32_t osr_sample_rates[] = {
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
static const uint8_t gain_values[] = {1, 2, 4, 8, 16, 32, 64, 128};

/* ==========================================================================
 * STATIC VARIABLES
 * ========================================================================== */

static ads131m02_ctx_t *mp_active_ctx = NULL;

/* ==========================================================================
 * SPI HELPERS
 * ========================================================================== */

static bool spi_transfer(ads131m02_ctx_t *ctx, uint8_t *tx, uint8_t *rx, size_t len)
{
    agsys_spi_xfer_t xfer = {
        .tx_buf = tx,
        .rx_buf = rx,
        .length = len,
    };
    
    return (agsys_spi_transfer(ctx->spi_handle, &xfer) == AGSYS_OK);
}

static uint16_t build_command(uint16_t cmd, uint8_t addr)
{
    return cmd | ((uint16_t)addr << 7);
}

/* ==========================================================================
 * REGISTER ACCESS
 * ========================================================================== */

bool ads131m02_read_reg(ads131m02_ctx_t *ctx, uint8_t reg, uint16_t *value)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    /* Build read command */
    uint16_t cmd = build_command(ADS131M02_CMD_RREG, reg);
    
    /* Frame: CMD (24-bit) + 2x channel data (24-bit each) = 9 bytes */
    uint8_t tx[9] = {0};
    uint8_t rx[9] = {0};
    
    tx[0] = (cmd >> 8) & 0xFF;
    tx[1] = cmd & 0xFF;
    tx[2] = 0x00;  /* Padding to 24-bit */
    
    /* First transfer sends command */
    if (!spi_transfer(ctx, tx, rx, 9)) return false;
    
    /* Second transfer gets response */
    memset(tx, 0, sizeof(tx));
    if (!spi_transfer(ctx, tx, rx, 9)) return false;
    
    /* Response is in first word */
    *value = ((uint16_t)rx[0] << 8) | rx[1];
    
    return true;
}

bool ads131m02_write_reg(ads131m02_ctx_t *ctx, uint8_t reg, uint16_t value)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    /* Build write command */
    uint16_t cmd = build_command(ADS131M02_CMD_WREG, reg);
    
    /* Frame: CMD (24-bit) + DATA (24-bit) + 2x channel (24-bit each) = 12 bytes */
    uint8_t tx[12] = {0};
    uint8_t rx[12] = {0};
    
    /* Command word */
    tx[0] = (cmd >> 8) & 0xFF;
    tx[1] = cmd & 0xFF;
    tx[2] = 0x00;
    
    /* Data word */
    tx[3] = (value >> 8) & 0xFF;
    tx[4] = value & 0xFF;
    tx[5] = 0x00;
    
    if (!spi_transfer(ctx, tx, rx, 12)) return false;
    
    return true;
}

/* ==========================================================================
 * DRDY INTERRUPT HANDLER
 * ========================================================================== */

static void drdy_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    if (mp_active_ctx == NULL || mp_active_ctx->drdy_callback == NULL) return;
    
    ads131m02_sample_t sample;
    if (ads131m02_read_sample(mp_active_ctx, &sample)) {
        mp_active_ctx->drdy_callback(&sample, mp_active_ctx->callback_user_data);
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool ads131m02_init(ads131m02_ctx_t *ctx, const ads131m02_config_t *config)
{
    if (ctx == NULL || config == NULL) return false;
    
    memset(ctx, 0, sizeof(ads131m02_ctx_t));
    
    ctx->cs_pin = config->cs_pin;
    ctx->drdy_pin = config->drdy_pin;
    ctx->sync_pin = config->sync_pin;
    ctx->osr = config->osr;
    ctx->gain_ch0 = config->gain_ch0;
    ctx->gain_ch1 = config->gain_ch1;
    ctx->power_mode = config->power_mode;
    
    /* Register with SPI manager on bus 0 (ADC bus) */
    agsys_spi_config_t spi_config = {
        .cs_pin = config->cs_pin,
        .cs_active_low = true,
        .frequency = NRF_SPIM_FREQ_4M,
        .mode = 1,  /* CPOL=0, CPHA=1 for ADS131M02 */
        .bus = AGSYS_SPI_BUS_0,
    };
    
    if (agsys_spi_register(&spi_config, &ctx->spi_handle) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "ADS131M02: Failed to register SPI\n");
        return false;
    }
    
    /* Configure SYNC/RST pin */
    nrf_gpio_cfg_output(ctx->sync_pin);
    nrf_gpio_pin_set(ctx->sync_pin);
    
    /* Configure DRDY pin as input */
    nrf_gpio_cfg_input(ctx->drdy_pin, NRF_GPIO_PIN_NOPULL);
    
    ctx->initialized = true;
    mp_active_ctx = ctx;
    
    /* Hardware reset */
    nrf_gpio_pin_clear(ctx->sync_pin);
    nrf_delay_ms(1);
    nrf_gpio_pin_set(ctx->sync_pin);
    nrf_delay_ms(10);
    
    /* Software reset */
    if (!ads131m02_reset(ctx)) {
        SEGGER_RTT_printf(0, "ADS131M02: Reset failed\n");
        return false;
    }
    
    /* Read device ID */
    uint16_t id;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_ID, &id)) {
        SEGGER_RTT_printf(0, "ADS131M02: Failed to read ID\n");
        return false;
    }
    
    ctx->device_id = id >> 8;  /* Upper byte is device ID */
    SEGGER_RTT_printf(0, "ADS131M02: Device ID = 0x%04X\n", id);
    
    /* Configure MODE register */
    uint16_t mode = ADS131M02_MODE_WLENGTH_24;  /* 24-bit word length */
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_MODE, mode)) {
        return false;
    }
    
    /* Configure CLOCK register */
    uint16_t clock = ADS131M02_CLK_CH0_EN | ADS131M02_CLK_CH1_EN |
                     (ctx->osr << 2) | ctx->power_mode;
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_CLOCK, clock)) {
        return false;
    }
    
    /* Configure GAIN register */
    uint16_t gain = (ctx->gain_ch1 << 4) | ctx->gain_ch0;
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_GAIN, gain)) {
        return false;
    }
    
    SEGGER_RTT_printf(0, "ADS131M02: Initialized, OSR=%d, Gain CH0=%d, CH1=%d\n",
                      ctx->osr, gain_values[ctx->gain_ch0], gain_values[ctx->gain_ch1]);
    
    return true;
}

bool ads131m02_reset(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL) return false;
    
    uint8_t tx[9] = {0};
    uint8_t rx[9] = {0};
    
    /* Send RESET command */
    tx[0] = (ADS131M02_CMD_RESET >> 8) & 0xFF;
    tx[1] = ADS131M02_CMD_RESET & 0xFF;
    
    if (!spi_transfer(ctx, tx, rx, 9)) return false;
    
    nrf_delay_ms(5);
    
    return true;
}

bool ads131m02_set_osr(ads131m02_ctx_t *ctx, ads131m02_osr_t osr)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint16_t clock;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_CLOCK, &clock)) return false;
    
    clock = (clock & ~(0x07 << 2)) | (osr << 2);
    
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_CLOCK, clock)) return false;
    
    ctx->osr = osr;
    return true;
}

bool ads131m02_set_gain(ads131m02_ctx_t *ctx, uint8_t channel, ads131m02_gain_t gain)
{
    if (ctx == NULL || !ctx->initialized || channel > 1) return false;
    
    uint16_t gain_reg;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_GAIN, &gain_reg)) return false;
    
    if (channel == 0) {
        gain_reg = (gain_reg & 0xFFF0) | gain;
        ctx->gain_ch0 = gain;
    } else {
        gain_reg = (gain_reg & 0xFF0F) | (gain << 4);
        ctx->gain_ch1 = gain;
    }
    
    return ads131m02_write_reg(ctx, ADS131M02_REG_GAIN, gain_reg);
}

bool ads131m02_set_power_mode(ads131m02_ctx_t *ctx, ads131m02_power_t mode)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint16_t clock;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_CLOCK, &clock)) return false;
    
    clock = (clock & ~0x03) | mode;
    
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_CLOCK, clock)) return false;
    
    ctx->power_mode = mode;
    return true;
}

bool ads131m02_read_sample(ads131m02_ctx_t *ctx, ads131m02_sample_t *sample)
{
    if (ctx == NULL || !ctx->initialized || sample == NULL) return false;
    
    /* Frame: STATUS (24-bit) + CH0 (24-bit) + CH1 (24-bit) = 9 bytes */
    uint8_t tx[9] = {0};
    uint8_t rx[9] = {0};
    
    if (!spi_transfer(ctx, tx, rx, 9)) {
        sample->valid = false;
        return false;
    }
    
    /* Parse status word (first 24 bits, but only 16 bits used) */
    sample->status = ((uint16_t)rx[0] << 8) | rx[1];
    
    /* Parse CH0 (24-bit signed, sign-extend to 32-bit) */
    int32_t ch0_raw = ((int32_t)rx[3] << 16) | ((int32_t)rx[4] << 8) | rx[5];
    if (ch0_raw & 0x800000) {
        ch0_raw |= 0xFF000000;  /* Sign extend */
    }
    sample->ch0 = ch0_raw;
    
    /* Parse CH1 (24-bit signed, sign-extend to 32-bit) */
    int32_t ch1_raw = ((int32_t)rx[6] << 16) | ((int32_t)rx[7] << 8) | rx[8];
    if (ch1_raw & 0x800000) {
        ch1_raw |= 0xFF000000;  /* Sign extend */
    }
    sample->ch1 = ch1_raw;
    
    sample->valid = true;
    return true;
}

bool ads131m02_data_ready(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL) return false;
    return (nrf_gpio_pin_read(ctx->drdy_pin) == 0);  /* DRDY is active low */
}

void ads131m02_set_drdy_callback(ads131m02_ctx_t *ctx,
                                  void (*callback)(ads131m02_sample_t*, void*),
                                  void *user_data)
{
    if (ctx == NULL) return;
    ctx->drdy_callback = callback;
    ctx->callback_user_data = user_data;
}

void ads131m02_enable_drdy_interrupt(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL) return;
    
    if (!nrf_drv_gpiote_is_init()) {
        nrf_drv_gpiote_init();
    }
    
    nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    config.pull = NRF_GPIO_PIN_NOPULL;
    
    nrf_drv_gpiote_in_init(ctx->drdy_pin, &config, drdy_handler);
    nrf_drv_gpiote_in_event_enable(ctx->drdy_pin, true);
}

void ads131m02_disable_drdy_interrupt(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL) return;
    nrf_drv_gpiote_in_event_disable(ctx->drdy_pin);
}

bool ads131m02_standby(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint8_t tx[9] = {0};
    uint8_t rx[9] = {0};
    
    tx[0] = (ADS131M02_CMD_STANDBY >> 8) & 0xFF;
    tx[1] = ADS131M02_CMD_STANDBY & 0xFF;
    
    return spi_transfer(ctx, tx, rx, 9);
}

bool ads131m02_wakeup(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint8_t tx[9] = {0};
    uint8_t rx[9] = {0};
    
    tx[0] = (ADS131M02_CMD_WAKEUP >> 8) & 0xFF;
    tx[1] = ADS131M02_CMD_WAKEUP & 0xFF;
    
    return spi_transfer(ctx, tx, rx, 9);
}

uint32_t ads131m02_get_sample_rate(ads131m02_osr_t osr)
{
    if (osr > ADS131M02_OSR_16384) return 0;
    return osr_sample_rates[osr];
}

float ads131m02_to_voltage(int32_t raw, ads131m02_gain_t gain, float vref)
{
    /* Full scale is ±VREF/Gain */
    /* 24-bit signed: range is -8388608 to +8388607 */
    float full_scale = vref / (float)gain_values[gain];
    return ((float)raw / 8388608.0f) * full_scale;
}

/* ==========================================================================
 * CALIBRATION FUNCTIONS
 * ========================================================================== */

static uint8_t get_ocal_msb_reg(uint8_t channel)
{
    return (channel == 0) ? ADS131M02_REG_CH0_OCAL_MSB : ADS131M02_REG_CH1_OCAL_MSB;
}

static uint8_t get_ocal_lsb_reg(uint8_t channel)
{
    return (channel == 0) ? ADS131M02_REG_CH0_OCAL_LSB : ADS131M02_REG_CH1_OCAL_LSB;
}

static uint8_t get_gcal_msb_reg(uint8_t channel)
{
    return (channel == 0) ? ADS131M02_REG_CH0_GCAL_MSB : ADS131M02_REG_CH1_GCAL_MSB;
}

static uint8_t get_gcal_lsb_reg(uint8_t channel)
{
    return (channel == 0) ? ADS131M02_REG_CH0_GCAL_LSB : ADS131M02_REG_CH1_GCAL_LSB;
}

static uint8_t get_ch_cfg_reg(uint8_t channel)
{
    return (channel == 0) ? ADS131M02_REG_CH0_CFG : ADS131M02_REG_CH1_CFG;
}

bool ads131m02_set_offset_cal(ads131m02_ctx_t *ctx, uint8_t channel, int32_t offset)
{
    if (ctx == NULL || !ctx->initialized || channel > 1) return false;
    
    /* Offset is 24-bit signed, stored in two 16-bit registers */
    /* MSB register: bits [23:8], LSB register: bits [7:0] in upper byte */
    uint16_t msb = (offset >> 8) & 0xFFFF;
    uint16_t lsb = (offset & 0xFF) << 8;
    
    if (!ads131m02_write_reg(ctx, get_ocal_msb_reg(channel), msb)) return false;
    if (!ads131m02_write_reg(ctx, get_ocal_lsb_reg(channel), lsb)) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: CH%d offset cal set to %d\n", channel, offset);
    return true;
}

bool ads131m02_get_offset_cal(ads131m02_ctx_t *ctx, uint8_t channel, int32_t *offset)
{
    if (ctx == NULL || !ctx->initialized || channel > 1 || offset == NULL) return false;
    
    uint16_t msb, lsb;
    if (!ads131m02_read_reg(ctx, get_ocal_msb_reg(channel), &msb)) return false;
    if (!ads131m02_read_reg(ctx, get_ocal_lsb_reg(channel), &lsb)) return false;
    
    /* Reconstruct 24-bit signed value */
    int32_t val = ((int32_t)msb << 8) | ((lsb >> 8) & 0xFF);
    
    /* Sign extend from 24-bit to 32-bit */
    if (val & 0x800000) {
        val |= 0xFF000000;
    }
    
    *offset = val;
    return true;
}

bool ads131m02_set_gain_cal(ads131m02_ctx_t *ctx, uint8_t channel, uint32_t gain_cal)
{
    if (ctx == NULL || !ctx->initialized || channel > 1) return false;
    
    /* Gain is 24-bit unsigned, stored in two 16-bit registers */
    /* Limit to 24 bits */
    gain_cal &= 0xFFFFFF;
    
    uint16_t msb = (gain_cal >> 8) & 0xFFFF;
    uint16_t lsb = (gain_cal & 0xFF) << 8;
    
    if (!ads131m02_write_reg(ctx, get_gcal_msb_reg(channel), msb)) return false;
    if (!ads131m02_write_reg(ctx, get_gcal_lsb_reg(channel), lsb)) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: CH%d gain cal set to 0x%06X (%.4f)\n", 
                      channel, gain_cal, (float)gain_cal / 8388608.0f);
    return true;
}

bool ads131m02_get_gain_cal(ads131m02_ctx_t *ctx, uint8_t channel, uint32_t *gain_cal)
{
    if (ctx == NULL || !ctx->initialized || channel > 1 || gain_cal == NULL) return false;
    
    uint16_t msb, lsb;
    if (!ads131m02_read_reg(ctx, get_gcal_msb_reg(channel), &msb)) return false;
    if (!ads131m02_read_reg(ctx, get_gcal_lsb_reg(channel), &lsb)) return false;
    
    /* Reconstruct 24-bit unsigned value */
    *gain_cal = ((uint32_t)msb << 8) | ((lsb >> 8) & 0xFF);
    return true;
}

bool ads131m02_auto_offset_cal(ads131m02_ctx_t *ctx, uint8_t channel, uint16_t num_samples)
{
    if (ctx == NULL || !ctx->initialized || channel > 1 || num_samples == 0) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: Starting auto offset cal for CH%d (%d samples)\n", 
                      channel, num_samples);
    
    /* Save current mux setting */
    uint16_t ch_cfg;
    if (!ads131m02_read_reg(ctx, get_ch_cfg_reg(channel), &ch_cfg)) return false;
    
    /* Set mux to shorted inputs */
    uint16_t shorted_cfg = (ch_cfg & ~ADS131M02_CHCFG_MUX_MASK) | ADS131M02_CHCFG_MUX_SHORT;
    if (!ads131m02_write_reg(ctx, get_ch_cfg_reg(channel), shorted_cfg)) return false;
    
    /* Wait for settling (a few conversion cycles) */
    nrf_delay_ms(10);
    
    /* Accumulate samples */
    int64_t sum = 0;
    uint16_t valid_samples = 0;
    
    for (uint16_t i = 0; i < num_samples; i++) {
        /* Wait for data ready */
        uint32_t timeout = 1000;
        while (!ads131m02_data_ready(ctx) && timeout > 0) {
            nrf_delay_us(100);
            timeout--;
        }
        
        if (timeout == 0) {
            SEGGER_RTT_printf(0, "ADS131M02: Timeout waiting for sample %d\n", i);
            continue;
        }
        
        ads131m02_sample_t sample;
        if (ads131m02_read_sample(ctx, &sample) && sample.valid) {
            sum += (channel == 0) ? sample.ch0 : sample.ch1;
            valid_samples++;
        }
    }
    
    /* Restore original mux setting */
    ads131m02_write_reg(ctx, get_ch_cfg_reg(channel), ch_cfg);
    
    if (valid_samples == 0) {
        SEGGER_RTT_printf(0, "ADS131M02: No valid samples for offset cal\n");
        return false;
    }
    
    /* Calculate average offset */
    int32_t avg_offset = (int32_t)(sum / valid_samples);
    
    SEGGER_RTT_printf(0, "ADS131M02: CH%d measured offset = %d (%d samples)\n", 
                      channel, avg_offset, valid_samples);
    
    /* Store the offset */
    return ads131m02_set_offset_cal(ctx, channel, avg_offset);
}

bool ads131m02_reset_calibration(ads131m02_ctx_t *ctx, uint8_t channel)
{
    if (ctx == NULL || !ctx->initialized || channel > 1) return false;
    
    /* Reset offset to 0 */
    if (!ads131m02_set_offset_cal(ctx, channel, ADS131M02_OCAL_DEFAULT)) return false;
    
    /* Reset gain to 1.0 (0x800000) */
    if (!ads131m02_set_gain_cal(ctx, channel, ADS131M02_GCAL_DEFAULT)) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: CH%d calibration reset to defaults\n", channel);
    return true;
}

/* ==========================================================================
 * GLOBAL-CHOP FUNCTIONS
 * ========================================================================== */

bool ads131m02_enable_global_chop(ads131m02_ctx_t *ctx, uint16_t delay_setting)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint16_t cfg;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_CFG, &cfg)) return false;
    
    /* Clear existing delay, set new delay and enable */
    cfg &= ~ADS131M02_CFG_GC_DLY_MASK;
    cfg |= (delay_setting & ADS131M02_CFG_GC_DLY_MASK);
    cfg |= ADS131M02_CFG_GC_EN;
    
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_CFG, cfg)) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: Global-chop enabled (delay=%d)\n", 
                      (delay_setting >> 9) & 0x0F);
    return true;
}

bool ads131m02_disable_global_chop(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint16_t cfg;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_CFG, &cfg)) return false;
    
    cfg &= ~ADS131M02_CFG_GC_EN;
    
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_CFG, cfg)) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: Global-chop disabled\n");
    return true;
}

bool ads131m02_is_global_chop_enabled(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint16_t cfg;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_CFG, &cfg)) return false;
    
    return (cfg & ADS131M02_CFG_GC_EN) != 0;
}

/* ==========================================================================
 * CRC FUNCTIONS
 * ========================================================================== */

bool ads131m02_enable_crc(ads131m02_ctx_t *ctx, bool enable_input, bool enable_output, bool use_ccitt)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint16_t mode;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_MODE, &mode)) return false;
    
    /* Clear CRC bits */
    mode &= ~(ADS131M02_MODE_REG_CRC_EN | ADS131M02_MODE_RX_CRC_EN | ADS131M02_MODE_CRC_TYPE);
    
    /* Set requested options */
    if (enable_output) mode |= ADS131M02_MODE_REG_CRC_EN;
    if (enable_input) mode |= ADS131M02_MODE_RX_CRC_EN;
    if (!use_ccitt) mode |= ADS131M02_MODE_CRC_TYPE;  /* ANSI if not CCITT */
    
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_MODE, mode)) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: CRC enabled (in=%d, out=%d, %s)\n", 
                      enable_input, enable_output, use_ccitt ? "CCITT" : "ANSI");
    return true;
}

bool ads131m02_disable_crc(ads131m02_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) return false;
    
    uint16_t mode;
    if (!ads131m02_read_reg(ctx, ADS131M02_REG_MODE, &mode)) return false;
    
    mode &= ~(ADS131M02_MODE_REG_CRC_EN | ADS131M02_MODE_RX_CRC_EN);
    
    if (!ads131m02_write_reg(ctx, ADS131M02_REG_MODE, mode)) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: CRC disabled\n");
    return true;
}

bool ads131m02_read_regmap_crc(ads131m02_ctx_t *ctx, uint16_t *crc)
{
    if (ctx == NULL || !ctx->initialized || crc == NULL) return false;
    
    return ads131m02_read_reg(ctx, ADS131M02_REG_REGMAP_CRC, crc);
}

bool ads131m02_check_crc_error(uint16_t status)
{
    return (status & ADS131M02_STATUS_CRC_ERR) != 0;
}

/* ==========================================================================
 * PHASE CALIBRATION FUNCTIONS
 * ========================================================================== */

bool ads131m02_set_phase_delay(ads131m02_ctx_t *ctx, uint8_t channel, uint16_t phase_delay)
{
    if (ctx == NULL || !ctx->initialized || channel > 1) return false;
    
    /* Phase delay is 10-bit (0-1023) */
    phase_delay &= ADS131M02_CHCFG_PHASE_MASK;
    
    uint16_t ch_cfg;
    if (!ads131m02_read_reg(ctx, get_ch_cfg_reg(channel), &ch_cfg)) return false;
    
    /* Clear existing phase, set new value */
    ch_cfg = (ch_cfg & ~ADS131M02_CHCFG_PHASE_MASK) | phase_delay;
    
    if (!ads131m02_write_reg(ctx, get_ch_cfg_reg(channel), ch_cfg)) return false;
    
    SEGGER_RTT_printf(0, "ADS131M02: CH%d phase delay set to %d\n", channel, phase_delay);
    return true;
}

bool ads131m02_get_phase_delay(ads131m02_ctx_t *ctx, uint8_t channel, uint16_t *phase_delay)
{
    if (ctx == NULL || !ctx->initialized || channel > 1 || phase_delay == NULL) return false;
    
    uint16_t ch_cfg;
    if (!ads131m02_read_reg(ctx, get_ch_cfg_reg(channel), &ch_cfg)) return false;
    
    *phase_delay = ch_cfg & ADS131M02_CHCFG_PHASE_MASK;
    return true;
}

/* ==========================================================================
 * INPUT MULTIPLEXER FUNCTIONS
 * ========================================================================== */

bool ads131m02_set_input_mux(ads131m02_ctx_t *ctx, uint8_t channel, ads131m02_mux_t mux)
{
    if (ctx == NULL || !ctx->initialized || channel > 1 || mux > ADS131M02_MUX_NEG_DC) return false;
    
    uint16_t ch_cfg;
    if (!ads131m02_read_reg(ctx, get_ch_cfg_reg(channel), &ch_cfg)) return false;
    
    /* Clear existing mux, set new value */
    ch_cfg = (ch_cfg & ~ADS131M02_CHCFG_MUX_MASK) | ((uint16_t)mux << 10);
    
    if (!ads131m02_write_reg(ctx, get_ch_cfg_reg(channel), ch_cfg)) return false;
    
    const char *mux_names[] = {"NORMAL", "SHORTED", "POS_DC", "NEG_DC"};
    SEGGER_RTT_printf(0, "ADS131M02: CH%d mux set to %s\n", channel, mux_names[mux]);
    return true;
}
