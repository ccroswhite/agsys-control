/**
 * @file temp_sensor.c
 * @brief Temperature Sensor Driver Implementation
 */

#include "temp_sensor.h"
#include "agsys_config.h"
#include "nrf_gpio.h"
#include "nrf_drv_saadc.h"
/* TWI disabled for now - TMP102 not populated on current boards */
/* #include "nrf_drv_twi.h" */
#include "SEGGER_RTT.h"
#include <math.h>

/* Stub out TWI functionality until boards have TMP102 populated */
#define TWI_DISABLED 1

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

/* NTC Steinhart-Hart coefficients (simplified B-parameter equation) */
#define NTC_T0_KELVIN       298.15f     /* 25°C in Kelvin */
#define KELVIN_OFFSET       273.15f

/* TMP102 registers */
#define TMP102_REG_TEMP     0x00
#define TMP102_REG_CONFIG   0x01

/* ADC configuration */
#define ADC_RESOLUTION      12
#define ADC_MAX_VALUE       ((1 << ADC_RESOLUTION) - 1)
#define ADC_REF_VOLTAGE_MV  3300

/* Copper temperature coefficient */
#define COPPER_TEMPCO       0.00393f    /* +0.393%/°C */

/* ==========================================================================
 * STATIC VARIABLES
 * ========================================================================== */

#if !TWI_DISABLED
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);
static bool s_twi_initialized = false;
#endif
static nrf_saadc_channel_config_t s_ntc_channel_config;

/* ==========================================================================
 * PRIVATE FUNCTIONS
 * ========================================================================== */

/**
 * @brief Convert NTC ADC reading to temperature
 * 
 * Uses B-parameter equation:
 *   1/T = 1/T0 + (1/B) * ln(R/R0)
 */
static float ntc_adc_to_temp(uint16_t adc_raw)
{
    if (adc_raw == 0 || adc_raw >= ADC_MAX_VALUE) {
        return NAN;
    }
    
    /* Calculate NTC resistance from voltage divider
     * Vout = Vcc * R_ref / (R_ntc + R_ref)
     * R_ntc = R_ref * (Vcc/Vout - 1) = R_ref * (ADC_MAX/adc_raw - 1)
     */
    float r_ntc = (float)AGSYS_TEMP_REF_R * ((float)ADC_MAX_VALUE / (float)adc_raw - 1.0f);
    
    /* B-parameter equation */
    float ln_r_ratio = logf(r_ntc / (float)AGSYS_TEMP_NTC_R25);
    float inv_t = (1.0f / NTC_T0_KELVIN) + (ln_r_ratio / (float)AGSYS_TEMP_NTC_B_VALUE);
    float temp_k = 1.0f / inv_t;
    
    return temp_k - KELVIN_OFFSET;
}

#if !TWI_DISABLED
/**
 * @brief Initialize I2C for TMP102
 */
static bool init_twi(void)
{
    if (s_twi_initialized) {
        return true;
    }
    
    nrf_drv_twi_config_t twi_config = {
        .scl = AGSYS_TEMP_I2C_SCL_PIN,
        .sda = AGSYS_TEMP_I2C_SDA_PIN,
        .frequency = NRF_DRV_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_LOW,
        .clear_bus_init = true
    };
    
    ret_code_t err = nrf_drv_twi_init(&m_twi, &twi_config, NULL, NULL);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "TEMP: TWI init failed (err=%d)\n", err);
        return false;
    }
    
    nrf_drv_twi_enable(&m_twi);
    s_twi_initialized = true;
    
    SEGGER_RTT_printf(0, "TEMP: TWI initialized\n");
    return true;
}
#endif

#if !TWI_DISABLED
/**
 * @brief Read TMP102 temperature register
 */
static bool tmp102_read_temp(float *temp_c)
{
    if (!s_twi_initialized) {
        return false;
    }
    
    uint8_t reg = TMP102_REG_TEMP;
    uint8_t data[2];
    
    /* Write register address */
    ret_code_t err = nrf_drv_twi_tx(&m_twi, AGSYS_TEMP_TMP102_ADDR, &reg, 1, true);
    if (err != NRF_SUCCESS) {
        return false;
    }
    
    /* Read 2 bytes */
    err = nrf_drv_twi_rx(&m_twi, AGSYS_TEMP_TMP102_ADDR, data, 2);
    if (err != NRF_SUCCESS) {
        return false;
    }
    
    /* Convert to temperature
     * TMP102 returns 12-bit value, MSB first
     * Bits [15:4] = temperature, [3:0] = 0
     * Resolution: 0.0625°C per LSB
     */
    int16_t raw = ((int16_t)data[0] << 4) | (data[1] >> 4);
    
    /* Handle negative temperatures (sign extend) */
    if (raw & 0x800) {
        raw |= 0xF000;
    }
    
    *temp_c = (float)raw * 0.0625f;
    return true;
}
#endif /* !TWI_DISABLED */

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool temp_sensor_init(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    
    /* Initialize context */
    ctx->initialized = false;
    ctx->ntc_valid = false;
    ctx->board_temp_c = NAN;
    ctx->ntc_adc_raw = 0;
    ctx->tmp102_present = false;
    ctx->pipe_temp_c = NAN;
    ctx->coil_temp_c = NAN;
    
    /* Initialize SAADC for NTC */
    ret_code_t err = nrf_drv_saadc_init(NULL, NULL);
    if (err != NRF_SUCCESS && err != NRF_ERROR_INVALID_STATE) {
        SEGGER_RTT_printf(0, "TEMP: SAADC init failed (err=%d)\n", err);
        return false;
    }
    
    /* Configure NTC ADC channel */
    s_ntc_channel_config = (nrf_saadc_channel_config_t)NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(
        NRF_SAADC_INPUT_AIN5);  /* P0.29 = AIN5 */
    s_ntc_channel_config.gain = NRF_SAADC_GAIN1_4;
    s_ntc_channel_config.reference = NRF_SAADC_REFERENCE_VDD4;
    
    err = nrf_drv_saadc_channel_init(0, &s_ntc_channel_config);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "TEMP: NTC channel init failed (err=%d)\n", err);
        return false;
    }
    
    ctx->ntc_valid = true;
    SEGGER_RTT_printf(0, "TEMP: NTC initialized on AIN5\n");
    
#if !TWI_DISABLED
    /* Initialize I2C for TMP102 */
    if (init_twi()) {
        /* Check if TMP102 is present */
        float temp;
        if (tmp102_read_temp(&temp)) {
            ctx->tmp102_present = true;
            ctx->pipe_temp_c = temp;
            SEGGER_RTT_printf(0, "TEMP: TMP102 detected (%.1f°C)\n", temp);
        } else {
            SEGGER_RTT_printf(0, "TEMP: TMP102 not detected\n");
        }
    }
#else
    SEGGER_RTT_printf(0, "TEMP: TMP102 disabled (TWI not configured)\n");
#endif
    
    ctx->initialized = true;
    return true;
}

float temp_sensor_read_board(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->ntc_valid) {
        return NAN;
    }
    
    nrf_saadc_value_t adc_value;
    ret_code_t err = nrf_drv_saadc_sample_convert(0, &adc_value);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "TEMP: NTC read failed (err=%d)\n", err);
        return NAN;
    }
    
    /* Clamp to valid range */
    if (adc_value < 0) adc_value = 0;
    if (adc_value > ADC_MAX_VALUE) adc_value = ADC_MAX_VALUE;
    
    ctx->ntc_adc_raw = (uint16_t)adc_value;
    ctx->board_temp_c = ntc_adc_to_temp(ctx->ntc_adc_raw);
    
    return ctx->board_temp_c;
}

float temp_sensor_read_pipe(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->tmp102_present) {
        return NAN;
    }
    
#if !TWI_DISABLED
    float temp;
    if (tmp102_read_temp(&temp)) {
        ctx->pipe_temp_c = temp;
        return temp;
    }
#endif
    
    return NAN;
}

void temp_sensor_read_all(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    
    temp_sensor_read_board(ctx);
    temp_sensor_read_pipe(ctx);
}

bool temp_sensor_tmp102_present(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    return ctx->tmp102_present;
}

float temp_sensor_estimate_coil_temp(uint32_t r_measured, uint32_t r_cal)
{
    if (r_cal == 0) {
        return NAN;
    }
    
    /* R(T) = R(25°C) * [1 + α * (T - 25)]
     * Solving for T:
     * T = 25 + (R_measured/R_cal - 1) / α
     */
    float r_ratio = (float)r_measured / (float)r_cal;
    float temp_c = 25.0f + (r_ratio - 1.0f) / COPPER_TEMPCO;
    
    return temp_c;
}
