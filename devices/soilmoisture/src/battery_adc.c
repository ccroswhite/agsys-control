/**
 * @file battery_adc.c
 * @brief Battery voltage ADC driver using nRF52 SAADC
 * 
 * Uses single-shot sampling with internal reference for low power.
 * The battery voltage is read through a resistor divider.
 */

#include "sdk_config.h"
#include "battery_adc.h"
#include "board_config.h"

#include "nrf.h"
#include "nrfx_saadc.h"
#include "SEGGER_RTT.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

/* SAADC configuration:
 * - Internal reference: 0.6V
 * - Gain: 1/6 (input range 0-3.6V)
 * - Resolution: 12-bit (0-4095)
 * - Acquisition time: 10us
 */
#define ADC_REFERENCE_MV        600     /* Internal reference in mV */
#define ADC_GAIN_RECIPROCAL     6       /* 1/6 gain = multiply by 6 */
#define ADC_RESOLUTION_BITS     12
#define ADC_MAX_VALUE           ((1 << ADC_RESOLUTION_BITS) - 1)

/* Full scale voltage = 0.6V * 6 = 3.6V = 3600mV */
#define ADC_FULL_SCALE_MV       (ADC_REFERENCE_MV * ADC_GAIN_RECIPROCAL)

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static bool m_initialized = false;
static volatile bool m_conversion_done = false;
static volatile nrf_saadc_value_t m_adc_value = 0;

/* ==========================================================================
 * SAADC EVENT HANDLER
 * ========================================================================== */

static void saadc_event_handler(nrfx_saadc_evt_t const *p_event)
{
    if (p_event->type == NRFX_SAADC_EVT_DONE) {
        m_adc_value = p_event->data.done.p_buffer[0];
        m_conversion_done = true;
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool battery_adc_init(void)
{
    if (m_initialized) {
        return true;
    }
    
    nrfx_err_t err;
    
    /* Initialize SAADC */
    nrfx_saadc_config_t saadc_config = NRFX_SAADC_DEFAULT_CONFIG;
    saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;
    saadc_config.oversample = NRF_SAADC_OVERSAMPLE_DISABLED;
    saadc_config.interrupt_priority = 6;
    
    err = nrfx_saadc_init(&saadc_config, saadc_event_handler);
    if (err != NRFX_SUCCESS) {
        SEGGER_RTT_printf(0, "BattADC: SAADC init failed: %d\n", err);
        return false;
    }
    
    /* Configure channel for battery voltage */
    nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(BATTERY_ADC_CHANNEL);
    channel_config.gain = NRF_SAADC_GAIN1_6;
    channel_config.reference = NRF_SAADC_REFERENCE_INTERNAL;
    channel_config.acq_time = NRF_SAADC_ACQTIME_10US;
    
    err = nrfx_saadc_channel_init(0, &channel_config);
    if (err != NRFX_SUCCESS) {
        SEGGER_RTT_printf(0, "BattADC: Channel init failed: %d\n", err);
        nrfx_saadc_uninit();
        return false;
    }
    
    m_initialized = true;
    SEGGER_RTT_printf(0, "BattADC: Initialized (channel AIN%d)\n", 
                      BATTERY_ADC_CHANNEL - NRF_SAADC_INPUT_AIN0);
    return true;
}

uint16_t battery_adc_read_mv(void)
{
    if (!m_initialized) {
        if (!battery_adc_init()) {
            return 0;
        }
    }
    
    nrfx_err_t err;
    static nrf_saadc_value_t adc_buffer[1];
    
    /* Start conversion */
    m_conversion_done = false;
    
    err = nrfx_saadc_buffer_convert(adc_buffer, 1);
    if (err != NRFX_SUCCESS) {
        SEGGER_RTT_printf(0, "BattADC: Buffer convert failed: %d\n", err);
        return 0;
    }
    
    err = nrfx_saadc_sample();
    if (err != NRFX_SUCCESS) {
        SEGGER_RTT_printf(0, "BattADC: Sample failed: %d\n", err);
        return 0;
    }
    
    /* Wait for conversion (with timeout) */
    uint32_t timeout = 100;  /* 100ms timeout */
    while (!m_conversion_done && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout--;
    }
    
    if (!m_conversion_done) {
        SEGGER_RTT_printf(0, "BattADC: Conversion timeout\n");
        return 0;
    }
    
    /* Handle negative values (shouldn't happen but be safe) */
    if (m_adc_value < 0) {
        m_adc_value = 0;
    }
    
    /* Convert ADC value to millivolts
     * voltage_at_pin = (adc_value / 4095) * 3600mV
     * battery_voltage = voltage_at_pin * BATTERY_DIVIDER_RATIO
     */
    uint32_t voltage_at_pin_mv = ((uint32_t)m_adc_value * ADC_FULL_SCALE_MV) / ADC_MAX_VALUE;
    uint16_t battery_mv = (uint16_t)(voltage_at_pin_mv * BATTERY_DIVIDER_RATIO);
    
    SEGGER_RTT_printf(0, "BattADC: raw=%d, pin=%lu mV, battery=%u mV\n",
                      m_adc_value, voltage_at_pin_mv, battery_mv);
    
    return battery_mv;
}

void battery_adc_deinit(void)
{
    if (m_initialized) {
        nrfx_saadc_uninit();
        m_initialized = false;
        SEGGER_RTT_printf(0, "BattADC: Deinitialized\n");
    }
}
