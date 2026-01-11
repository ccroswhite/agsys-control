/**
 * @file ads131m02.c
 * @brief ADS131M02 24-bit Delta-Sigma ADC Driver Implementation
 */

#include "ads131m02.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_gpiote.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

#define ADS131M02_DEVICE_ID         0x0082  /* Expected device ID */
#define ADS131M02_SPI_FREQ          NRF_DRV_SPI_FREQ_4M
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

/* External SPI instance - initialized by main.c */
extern const nrf_drv_spi_t g_spi_adc;

/* ==========================================================================
 * SPI HELPERS
 * ========================================================================== */

static bool spi_transfer(ads131m02_ctx_t *ctx, uint8_t *tx, uint8_t *rx, size_t len)
{
    if (ctx->spi_mutex) {
        if (xSemaphoreTake(ctx->spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return false;
        }
    }
    
    nrf_gpio_pin_clear(ctx->cs_pin);
    nrf_delay_us(1);
    
    ret_code_t err = nrf_drv_spi_transfer(&g_spi_adc, tx, len, rx, len);
    
    nrf_delay_us(1);
    nrf_gpio_pin_set(ctx->cs_pin);
    
    if (ctx->spi_mutex) {
        xSemaphoreGive(ctx->spi_mutex);
    }
    
    return (err == NRF_SUCCESS);
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
    
    ctx->spi_instance = config->spi_instance;
    ctx->cs_pin = config->cs_pin;
    ctx->drdy_pin = config->drdy_pin;
    ctx->sync_pin = config->sync_pin;
    ctx->spi_mutex = config->spi_mutex;
    ctx->osr = config->osr;
    ctx->gain_ch0 = config->gain_ch0;
    ctx->gain_ch1 = config->gain_ch1;
    ctx->power_mode = config->power_mode;
    
    /* Configure CS pin */
    nrf_gpio_cfg_output(ctx->cs_pin);
    nrf_gpio_pin_set(ctx->cs_pin);
    
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
