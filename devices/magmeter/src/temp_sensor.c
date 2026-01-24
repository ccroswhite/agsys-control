/**
 * @file temp_sensor.c
 * @brief Temperature Sensor Driver Implementation
 */

#include "temp_sensor.h"
#include "tmp102.h"
#include "agsys_config.h"
#include "nrf_gpio.h"
#include "nrf_drv_saadc.h"
/* TWI disabled for now - TMP102 not populated on current boards */
/* #include "nrf_drv_twi.h" */
#include "SEGGER_RTT.h"
#include <math.h>
#include <string.h>

/* Stub out TWI functionality until boards have TMP102 populated */
#define TWI_DISABLED 1

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

/* NTC Steinhart-Hart coefficients (simplified B-parameter equation) */
#define NTC_T0_KELVIN       298.15f     /* 25°C in Kelvin */
#define KELVIN_OFFSET       273.15f

/* TMP102 device contexts */
#if !TWI_DISABLED
static tmp102_ctx_t s_tmp102_coil;
static tmp102_ctx_t s_tmp102_electrode;
#endif

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
 * @brief I2C read function for TMP102 driver
 */
static bool nrf_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len, void *user_data)
{
    (void)user_data;
    
    if (!s_twi_initialized) {
        return false;
    }
    
    ret_code_t err = nrf_drv_twi_tx(&m_twi, addr, &reg, 1, true);
    if (err != NRF_SUCCESS) {
        return false;
    }
    
    return nrf_drv_twi_rx(&m_twi, addr, data, len) == NRF_SUCCESS;
}

/**
 * @brief I2C write function for TMP102 driver
 */
static bool nrf_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len, void *user_data)
{
    (void)user_data;
    
    if (!s_twi_initialized) {
        return false;
    }
    
    /* Build buffer with register address + data */
    uint8_t buf[17];
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    
    return nrf_drv_twi_tx(&m_twi, addr, buf, len + 1, false) == NRF_SUCCESS;
}

/**
 * @brief Initialize I2C for TMP102 sensors
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

/* I2C interface for TMP102 driver */
static tmp102_i2c_t s_i2c = {
    .read = nrf_i2c_read,
    .write = nrf_i2c_write,
    .user_data = NULL
};
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
    ctx->tmp102_coil_present = false;
    ctx->coil_temp_c = NAN;
    ctx->tmp102_electrode_present = false;
    ctx->electrode_temp_c = NAN;
    
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
    /* Initialize I2C for TMP102 sensors */
    if (init_twi()) {
        tmp102_config_t coil_config = TMP102_CONFIG_DEFAULT(AGSYS_TEMP_TMP102_COIL_ADDR);
        tmp102_config_t electrode_config = TMP102_CONFIG_DEFAULT(AGSYS_TEMP_TMP102_ELECTRODE_ADDR);
        
        /* Initialize coil TMP102 (address 0x48) */
        if (tmp102_init(&s_tmp102_coil, &s_i2c, &coil_config)) {
            ctx->tmp102_coil_present = true;
            float temp;
            if (tmp102_read_temp_c(&s_tmp102_coil, &temp)) {
                ctx->coil_temp_c = temp;
                SEGGER_RTT_printf(0, "TEMP: Coil TMP102 detected @ 0x%02X (%.1f°C)\n", 
                                  AGSYS_TEMP_TMP102_COIL_ADDR, temp);
            }
        } else {
            SEGGER_RTT_printf(0, "TEMP: Coil TMP102 not detected @ 0x%02X\n",
                              AGSYS_TEMP_TMP102_COIL_ADDR);
        }
        
        /* Initialize electrode TMP102 (address 0x49) */
        if (tmp102_init(&s_tmp102_electrode, &s_i2c, &electrode_config)) {
            ctx->tmp102_electrode_present = true;
            float temp;
            if (tmp102_read_temp_c(&s_tmp102_electrode, &temp)) {
                ctx->electrode_temp_c = temp;
                SEGGER_RTT_printf(0, "TEMP: Electrode TMP102 detected @ 0x%02X (%.1f°C)\n",
                                  AGSYS_TEMP_TMP102_ELECTRODE_ADDR, temp);
            }
        } else {
            SEGGER_RTT_printf(0, "TEMP: Electrode TMP102 not detected @ 0x%02X\n",
                              AGSYS_TEMP_TMP102_ELECTRODE_ADDR);
        }
    }
#else
    SEGGER_RTT_printf(0, "TEMP: TMP102 sensors disabled (TWI not configured)\n");
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

float temp_sensor_read_coil(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->tmp102_coil_present) {
        return NAN;
    }
    
#if !TWI_DISABLED
    float temp;
    if (tmp102_read_temp_c(&s_tmp102_coil, &temp)) {
        ctx->coil_temp_c = temp;
        return temp;
    }
#endif
    
    return NAN;
}

float temp_sensor_read_electrode(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->tmp102_electrode_present) {
        return NAN;
    }
    
#if !TWI_DISABLED
    float temp;
    if (tmp102_read_temp_c(&s_tmp102_electrode, &temp)) {
        ctx->electrode_temp_c = temp;
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
    temp_sensor_read_coil(ctx);
    temp_sensor_read_electrode(ctx);
}

bool temp_sensor_coil_present(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    return ctx->tmp102_coil_present;
}

bool temp_sensor_electrode_present(temp_sensor_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    return ctx->tmp102_electrode_present;
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
