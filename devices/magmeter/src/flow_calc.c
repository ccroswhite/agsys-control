/**
 * @file flow_calc.c
 * @brief Electromagnetic Flow Meter Signal Processing Implementation
 * 
 * Synchronous detection algorithm:
 *   1. Accumulate ADC samples during coil-ON phase
 *   2. Accumulate ADC samples during coil-OFF phase
 *   3. At end of each cycle: V_flow = mean(V_on) - mean(V_off)
 *   4. Average over 32 cycles for stable output
 *   5. Convert to flow rate using Faraday's law
 */

#include "flow_calc.h"
#include "coil_driver.h"
#include "agsys_memory_layout.h"
#include "agsys_fram.h"
#include "SEGGER_RTT.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <math.h>

/* External FRAM context (initialized in main.c via agsys_device) */
extern agsys_fram_ctx_t *g_fram_ctx;

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

/* Pipe inner diameters (meters) for Schedule 40/80 PVC */
static const float PIPE_DIAMETERS_M[PIPE_SIZE_COUNT] = {
    0.0381f,    /* 1.5" Sch 80: 38.1mm ID */
    0.0525f,    /* 2" Sch 80: 52.5mm ID */
    0.0635f,    /* 2.5" Sch 40: 63.5mm ID */
    0.0779f,    /* 3" Sch 40: 77.9mm ID */
    0.1023f,    /* 4" Sch 40: 102.3mm ID */
    0.1282f,    /* 5" Sch 40: 128.2mm ID */
    0.1541f,    /* 6" Sch 40: 154.1mm ID */
};

/* Default span coefficients (µV per m/s) - empirical, needs calibration */
static const float DEFAULT_SPAN_UV_PER_MPS[PIPE_SIZE_COUNT] = {
    150.0f,     /* 1.5" */
    180.0f,     /* 2" */
    200.0f,     /* 2.5" */
    220.0f,     /* 3" */
    250.0f,     /* 4" */
    280.0f,     /* 5" */
    300.0f,     /* 6" */
};

/* Tier ID voltage thresholds (mV) - from power board voltage dividers */
#define TIER_S_VOLTAGE_MV   825     /* 0.825V ± 10% */
#define TIER_M_VOLTAGE_MV   1650    /* 1.65V ± 10% */
#define TIER_L_VOLTAGE_MV   2475    /* 2.475V ± 10% */
#define TIER_TOLERANCE_MV   165     /* ±10% tolerance */

/* Current sense resistor and gain for coil current measurement */
#define CURRENT_SENSE_RESISTOR_OHM  0.1f    /* MM-S uses 0.1Ω */
#define CURRENT_SENSE_GAIN          1.0f    /* Direct measurement for MM-S */

/* ADC full scale (24-bit signed) */
#define ADC_FULL_SCALE              8388607  /* 2^23 - 1 */

/* ==========================================================================
 * INTERNAL FUNCTIONS
 * ========================================================================== */

static uint32_t crc32_calc(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

static void reset_detector(sync_detector_t *det)
{
    det->sum_on = 0;
    det->sum_off = 0;
    det->count_on = 0;
    det->count_off = 0;
    det->phase_sample_count = 0;
    det->sum_coil_current = 0;
    det->count_coil = 0;
}

static void reset_cycle_buffer(sync_detector_t *det)
{
    memset(det->cycle_results, 0, sizeof(det->cycle_results));
    det->cycle_index = 0;
    det->cycles_valid = 0;
    det->sum_sq_diff = 0.0f;
    det->last_result = 0.0f;
}

static float gain_to_multiplier(ads131m02_gain_t gain)
{
    switch (gain) {
        case ADS131M02_GAIN_1:   return 1.0f;
        case ADS131M02_GAIN_2:   return 2.0f;
        case ADS131M02_GAIN_4:   return 4.0f;
        case ADS131M02_GAIN_8:   return 8.0f;
        case ADS131M02_GAIN_16:  return 16.0f;
        case ADS131M02_GAIN_32:  return 32.0f;
        case ADS131M02_GAIN_64:  return 64.0f;
        case ADS131M02_GAIN_128: return 128.0f;
        default:                 return 1.0f;
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

float flow_calc_raw_to_uv(int32_t raw, ads131m02_gain_t gain)
{
    float gain_mult = gain_to_multiplier(gain);
    float voltage_v = ((float)raw / (float)ADC_FULL_SCALE) * FLOW_ADC_VREF_V / gain_mult;
    return voltage_v * 1000000.0f;  /* Convert to µV */
}

float flow_calc_get_pipe_diameter(flow_pipe_size_t pipe_size)
{
    if (pipe_size >= PIPE_SIZE_COUNT) {
        return PIPE_DIAMETERS_M[PIPE_SIZE_2_INCH];  /* Default */
    }
    return PIPE_DIAMETERS_M[pipe_size];
}

flow_tier_t flow_calc_detect_tier(uint32_t tier_id_mv)
{
    if (tier_id_mv >= (TIER_S_VOLTAGE_MV - TIER_TOLERANCE_MV) &&
        tier_id_mv <= (TIER_S_VOLTAGE_MV + TIER_TOLERANCE_MV)) {
        return FLOW_TIER_S;
    }
    if (tier_id_mv >= (TIER_M_VOLTAGE_MV - TIER_TOLERANCE_MV) &&
        tier_id_mv <= (TIER_M_VOLTAGE_MV + TIER_TOLERANCE_MV)) {
        return FLOW_TIER_M;
    }
    if (tier_id_mv >= (TIER_L_VOLTAGE_MV - TIER_TOLERANCE_MV) &&
        tier_id_mv <= (TIER_L_VOLTAGE_MV + TIER_TOLERANCE_MV)) {
        return FLOW_TIER_L;
    }
    return FLOW_TIER_UNKNOWN;
}

bool flow_calc_init(flow_calc_ctx_t *ctx, ads131m02_ctx_t *adc)
{
    if (ctx == NULL || adc == NULL) {
        return false;
    }
    
    memset(ctx, 0, sizeof(flow_calc_ctx_t));
    ctx->adc = adc;
    ctx->adc_gain = ADS131M02_GAIN_32;  /* Start with moderate gain */
    ctx->auto_gain = true;
    
    /* Initialize detector state */
    reset_detector(&ctx->detector);
    reset_cycle_buffer(&ctx->detector);
    
    /* Initialize state */
    ctx->state.calibration_valid = false;
    ctx->state.signal_quality = 0;
    
    ctx->initialized = true;
    ctx->running = false;
    
    SEGGER_RTT_printf(0, "FLOW: Initialized\n");
    return true;
}

void flow_calc_set_defaults(flow_calc_ctx_t *ctx, flow_pipe_size_t pipe_size)
{
    if (ctx == NULL || pipe_size >= PIPE_SIZE_COUNT) {
        return;
    }
    
    flow_calibration_t *cal = &ctx->calibration;
    
    cal->magic = FLOW_CAL_MAGIC;
    cal->version = FLOW_CAL_VERSION;
    cal->pipe_size = (uint8_t)pipe_size;
    cal->tier = FLOW_TIER_UNKNOWN;
    cal->auto_zero_enabled = 1;  /* Auto-zero on by default */
    
    cal->zero_offset_uv = 0.0f;
    cal->span_uv_per_mps = DEFAULT_SPAN_UV_PER_MPS[pipe_size];
    cal->temp_coeff_offset = 0.0f;
    cal->temp_coeff_span = 0.0f;
    cal->ref_temp_c = 25.0f;
    cal->pipe_diameter_m = PIPE_DIAMETERS_M[pipe_size];
    cal->k_factor = 0.0f;  /* Mag mode */
    
    /* Default duty cycle: 1.1s on / 13.9s off (~7.3% duty) */
    cal->coil_on_time_ms = COIL_DEFAULT_ON_TIME_MS;
    cal->coil_off_time_ms = COIL_DEFAULT_OFF_TIME_MS;
    
    /* Default PWM current control (MM-S tier defaults) */
    cal->target_current_ma = COIL_DEFAULT_TARGET_MA;
    cal->supply_voltage_mv = COIL_DEFAULT_SUPPLY_MV / 10;  /* Store as /10 to fit uint16 */
    cal->coil_resistance_mo = COIL_DEFAULT_RESISTANCE_MO;
    
    cal->cal_date = 0;
    cal->serial_number = 0;
    
    /* Calculate CRC */
    cal->crc32 = crc32_calc(cal, offsetof(flow_calibration_t, crc32));
    
    ctx->state.calibration_valid = true;
    ctx->auto_zero_enabled = cal->auto_zero_enabled;
    
    SEGGER_RTT_printf(0, "FLOW: Defaults set for pipe size %d (D=%.1fmm)\n",
                      pipe_size, cal->pipe_diameter_m * 1000.0f);
}

bool flow_calc_load_calibration(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || g_fram_ctx == NULL) {
        return false;
    }
    
    /* Read calibration from FRAM */
    flow_calibration_t cal;
    agsys_err_t err = agsys_fram_read(g_fram_ctx, 
                                       AGSYS_FRAM_FLOW_CAL_ADDR,
                                       (uint8_t *)&cal,
                                       sizeof(cal));
    
    if (err != AGSYS_OK) {
        SEGGER_RTT_printf(0, "FLOW: FRAM read failed (err=%d)\n", err);
        return false;
    }
    
    /* Validate magic */
    if (cal.magic != FLOW_CAL_MAGIC) {
        SEGGER_RTT_printf(0, "FLOW: No valid calibration in FRAM (magic=0x%08X)\n", cal.magic);
        return false;
    }
    
    /* Validate CRC */
    uint32_t expected_crc = crc32_calc(&cal, offsetof(flow_calibration_t, crc32));
    if (cal.crc32 != expected_crc) {
        SEGGER_RTT_printf(0, "FLOW: Calibration CRC mismatch (got=0x%08X, exp=0x%08X)\n",
                          cal.crc32, expected_crc);
        return false;
    }
    
    /* Validate version */
    if (cal.version != FLOW_CAL_VERSION) {
        SEGGER_RTT_printf(0, "FLOW: Calibration version mismatch (got=%d, exp=%d)\n",
                          cal.version, FLOW_CAL_VERSION);
        /* Could add migration here for future versions */
        return false;
    }
    
    /* Copy to context */
    memcpy(&ctx->calibration, &cal, sizeof(cal));
    ctx->state.calibration_valid = true;
    ctx->auto_zero_enabled = cal.auto_zero_enabled;
    
    SEGGER_RTT_printf(0, "FLOW: Loaded calibration (pipe=%d, span=%.1f uV/(m/s), zero=%.1f uV)\n",
                      cal.pipe_size, cal.span_uv_per_mps, cal.zero_offset_uv);
    SEGGER_RTT_printf(0, "FLOW: Duty cycle: %ums on / %ums off, auto-zero=%d\n",
                      cal.coil_on_time_ms, cal.coil_off_time_ms, cal.auto_zero_enabled);
    
    return true;
}

bool flow_calc_save_calibration(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || g_fram_ctx == NULL) {
        return false;
    }
    
    /* Ensure magic and version are set */
    ctx->calibration.magic = FLOW_CAL_MAGIC;
    ctx->calibration.version = FLOW_CAL_VERSION;
    
    /* Update CRC before saving */
    ctx->calibration.crc32 = crc32_calc(&ctx->calibration, 
                                         offsetof(flow_calibration_t, crc32));
    
    /* Write to FRAM */
    agsys_err_t err = agsys_fram_write(g_fram_ctx,
                                        AGSYS_FRAM_FLOW_CAL_ADDR,
                                        (const uint8_t *)&ctx->calibration,
                                        sizeof(ctx->calibration));
    
    if (err != AGSYS_OK) {
        SEGGER_RTT_printf(0, "FLOW: FRAM write failed (err=%d)\n", err);
        return false;
    }
    
    SEGGER_RTT_printf(0, "FLOW: Calibration saved (pipe=%d, span=%.1f, zero=%.1f)\n",
                      ctx->calibration.pipe_size,
                      ctx->calibration.span_uv_per_mps,
                      ctx->calibration.zero_offset_uv);
    
    return true;
}

bool flow_calc_start(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* Reset detector state */
    reset_detector(&ctx->detector);
    reset_cycle_buffer(&ctx->detector);
    
    /* Reset statistics */
    ctx->state.min_flow_lpm = 1e9f;
    ctx->state.max_flow_lpm = -1e9f;
    ctx->state.avg_flow_lpm = 0.0f;
    ctx->state.sample_count = 0;
    
    ctx->running = true;
    
    SEGGER_RTT_printf(0, "FLOW: Started\n");
    return true;
}

void flow_calc_stop(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->running = false;
    SEGGER_RTT_printf(0, "FLOW: Stopped\n");
}

void flow_calc_process_sample(flow_calc_ctx_t *ctx, 
                               const ads131m02_sample_t *sample,
                               bool coil_on)
{
    if (ctx == NULL || sample == NULL || !ctx->running) {
        return;
    }
    
    sync_detector_t *det = &ctx->detector;
    flow_calibration_t *cal = &ctx->calibration;
    flow_state_t *state = &ctx->state;
    
    /* Accumulate electrode signal (channel 0) based on coil state */
    if (coil_on) {
        det->sum_on += sample->ch0;
        det->count_on++;
    } else {
        det->sum_off += sample->ch0;
        det->count_off++;
    }
    
    /* Accumulate coil current (channel 1) during ON phase only */
    if (coil_on) {
        det->sum_coil_current += sample->ch1;
        det->count_coil++;
    }
    
    det->phase_sample_count++;
    
    /* Check for end of excitation cycle (after both ON and OFF phases) */
    /* At 16kHz with 2kHz excitation: 8 samples per cycle (4 ON + 4 OFF) */
    if (det->count_on >= FLOW_SAMPLES_PER_HALF && 
        det->count_off >= FLOW_SAMPLES_PER_HALF) {
        
        /* Calculate synchronous detection result for this cycle */
        float mean_on = (float)det->sum_on / (float)det->count_on;
        float mean_off = (float)det->sum_off / (float)det->count_off;
        float diff_raw = mean_on - mean_off;
        
        /* Convert to microvolts */
        float signal_uv = flow_calc_raw_to_uv((int32_t)diff_raw, ctx->adc_gain);
        
        /* Store in circular buffer */
        det->cycle_results[det->cycle_index] = signal_uv;
        det->cycle_index = (det->cycle_index + 1) % FLOW_AVG_CYCLES;
        if (det->cycles_valid < FLOW_AVG_CYCLES) {
            det->cycles_valid++;
        }
        
        /* Update noise estimate (running variance) */
        float diff_from_last = signal_uv - det->last_result;
        det->sum_sq_diff += diff_from_last * diff_from_last;
        det->last_result = signal_uv;
        
        /* Calculate coil current for this cycle */
        if (det->count_coil > 0) {
            float mean_coil_raw = (float)det->sum_coil_current / (float)det->count_coil;
            float coil_voltage_uv = flow_calc_raw_to_uv((int32_t)mean_coil_raw, ADS131M02_GAIN_1);
            /* Convert to mA: I = V / R, voltage is in µV, resistor in Ω */
            state->coil_current_ma = (coil_voltage_uv / 1000000.0f) / CURRENT_SENSE_RESISTOR_OHM * 1000.0f;
        }
        
        /* Reset accumulators for next cycle */
        reset_detector(det);
        
        /* Calculate averaged signal when we have enough cycles */
        if (det->cycles_valid >= FLOW_AVG_CYCLES) {
            float sum = 0.0f;
            for (uint32_t i = 0; i < FLOW_AVG_CYCLES; i++) {
                sum += det->cycle_results[i];
            }
            float avg_signal_uv = sum / (float)FLOW_AVG_CYCLES;
            
            /* Apply temperature compensation if calibrated */
            if (cal->temp_coeff_offset != 0.0f || cal->temp_coeff_span != 0.0f) {
                float temp_diff = state->temperature_c - cal->ref_temp_c;
                avg_signal_uv -= cal->temp_coeff_offset * temp_diff;
                /* Span compensation would be applied to the span coefficient */
            }
            
            /* Apply zero offset */
            avg_signal_uv -= cal->zero_offset_uv;
            
            /* Store raw signal */
            state->signal_uv = avg_signal_uv;
            
            /* Check signal status */
            float abs_signal = fabsf(avg_signal_uv);
            state->signal_low = (abs_signal < FLOW_MIN_SIGNAL_UV);
            state->signal_high = (abs_signal > FLOW_MAX_SIGNAL_UV);
            state->reverse_flow = (avg_signal_uv < FLOW_REVERSE_THRESHOLD_UV);
            
            /* Check coil status */
            state->coil_fault = (state->coil_current_ma < FLOW_COIL_CURRENT_MIN_MA ||
                                 state->coil_current_ma > FLOW_COIL_CURRENT_MAX_MA);
            
            /* Convert signal to velocity using span coefficient */
            /* V_signal = span × velocity → velocity = V_signal / span */
            float velocity_mps = 0.0f;
            if (cal->span_uv_per_mps > 0.0f && !state->signal_low) {
                velocity_mps = avg_signal_uv / cal->span_uv_per_mps;
            }
            
            /* Apply zero threshold */
            if (fabsf(avg_signal_uv) < FLOW_ZERO_THRESHOLD_UV) {
                velocity_mps = 0.0f;
            }
            
            state->velocity_mps = velocity_mps;
            
            /* Convert velocity to volumetric flow rate */
            /* Q = A × v = π × (D/2)² × v */
            float radius_m = cal->pipe_diameter_m / 2.0f;
            float area_m2 = 3.14159265f * radius_m * radius_m;
            float flow_m3_per_s = area_m2 * fabsf(velocity_mps);
            
            /* Convert to L/min and GPM */
            /* 1 m³/s = 60000 L/min */
            state->flow_rate_lpm = flow_m3_per_s * 60000.0f;
            state->flow_rate_gpm = state->flow_rate_lpm / FLOW_LITERS_PER_GALLON;
            
            /* Handle reverse flow (negative values) */
            if (state->reverse_flow) {
                state->flow_rate_lpm = -state->flow_rate_lpm;
                state->flow_rate_gpm = -state->flow_rate_gpm;
            }
            
            /* Update totalization (integrate flow over time) */
            /* Each update is ~16ms (32 cycles at 2kHz) */
            float dt_min = (float)FLOW_AVG_CYCLES / (float)FLOW_EXCITATION_FREQ_HZ / 60.0f;
            state->total_volume_l += state->flow_rate_lpm * dt_min;
            state->total_volume_gal = state->total_volume_l / FLOW_LITERS_PER_GALLON;
            
            /* Update statistics */
            if (state->flow_rate_lpm < state->min_flow_lpm) {
                state->min_flow_lpm = state->flow_rate_lpm;
            }
            if (state->flow_rate_lpm > state->max_flow_lpm) {
                state->max_flow_lpm = state->flow_rate_lpm;
            }
            /* Running average */
            state->sample_count++;
            state->avg_flow_lpm += (state->flow_rate_lpm - state->avg_flow_lpm) / (float)state->sample_count;
            
            /* Calculate signal quality (0-100%) */
            /* Based on noise level relative to signal */
            if (det->cycles_valid > 1) {
                float variance = det->sum_sq_diff / (float)(det->cycles_valid - 1);
                state->noise_uv = sqrtf(variance);
                
                if (abs_signal > 0.0f) {
                    float snr = abs_signal / (state->noise_uv + 0.1f);
                    state->signal_quality = (uint8_t)fminf(100.0f, snr * 10.0f);
                } else {
                    state->signal_quality = 0;
                }
            }
            
            /* Auto-gain adjustment */
            if (ctx->auto_gain) {
                /* If signal is too low and not at max gain, increase */
                if (abs_signal < 50.0f && ctx->adc_gain < ADS131M02_GAIN_128) {
                    ctx->adc_gain = (ads131m02_gain_t)(ctx->adc_gain + 1);
                    ads131m02_set_gain(ctx->adc, 0, ctx->adc_gain);
                    SEGGER_RTT_printf(0, "FLOW: Gain increased to %d\n", 
                                      (int)gain_to_multiplier(ctx->adc_gain));
                }
                /* If signal is too high (near saturation), decrease */
                else if (abs_signal > 400.0f && ctx->adc_gain > ADS131M02_GAIN_1) {
                    ctx->adc_gain = (ads131m02_gain_t)(ctx->adc_gain - 1);
                    ads131m02_set_gain(ctx->adc, 0, ctx->adc_gain);
                    SEGGER_RTT_printf(0, "FLOW: Gain decreased to %d\n",
                                      (int)gain_to_multiplier(ctx->adc_gain));
                }
            }
        }
    }
}

void flow_calc_get_state(flow_calc_ctx_t *ctx, flow_state_t *state)
{
    if (ctx == NULL || state == NULL) {
        return;
    }
    
    /* Simple copy - in production, use critical section or mutex */
    memcpy(state, &ctx->state, sizeof(flow_state_t));
}

void flow_calc_reset_total(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->state.total_volume_l = 0.0f;
    ctx->state.total_volume_gal = 0.0f;
    
    SEGGER_RTT_printf(0, "FLOW: Totals reset\n");
}

void flow_calc_reset_stats(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->state.min_flow_lpm = 1e9f;
    ctx->state.max_flow_lpm = -1e9f;
    ctx->state.avg_flow_lpm = 0.0f;
    ctx->state.sample_count = 0;
    
    SEGGER_RTT_printf(0, "FLOW: Stats reset\n");
}

bool flow_calc_zero_calibrate(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->running) {
        return false;
    }
    
    /* Use current averaged signal as zero offset */
    /* Requires stable reading with no flow */
    if (ctx->detector.cycles_valid < FLOW_AVG_CYCLES) {
        SEGGER_RTT_printf(0, "FLOW: Zero cal failed - not enough samples\n");
        return false;
    }
    
    /* Calculate current average */
    float sum = 0.0f;
    for (uint32_t i = 0; i < FLOW_AVG_CYCLES; i++) {
        sum += ctx->detector.cycle_results[i];
    }
    float avg_signal_uv = sum / (float)FLOW_AVG_CYCLES;
    
    /* Check if signal is stable (low noise) */
    if (ctx->state.noise_uv > 10.0f) {
        SEGGER_RTT_printf(0, "FLOW: Zero cal failed - signal too noisy (%.1f µV)\n",
                          ctx->state.noise_uv);
        return false;
    }
    
    ctx->calibration.zero_offset_uv = avg_signal_uv;
    ctx->calibration.ref_temp_c = ctx->state.temperature_c;
    
    SEGGER_RTT_printf(0, "FLOW: Zero calibrated at %.1f µV (T=%.1f°C)\n",
                      avg_signal_uv, ctx->state.temperature_c);
    
    return true;
}

bool flow_calc_span_calibrate(flow_calc_ctx_t *ctx, float known_flow_lpm)
{
    if (ctx == NULL || !ctx->running || known_flow_lpm <= 0.0f) {
        return false;
    }
    
    if (ctx->detector.cycles_valid < FLOW_AVG_CYCLES) {
        SEGGER_RTT_printf(0, "FLOW: Span cal failed - not enough samples\n");
        return false;
    }
    
    /* Get current signal (already zero-corrected in state) */
    float signal_uv = ctx->state.signal_uv;
    
    if (fabsf(signal_uv) < FLOW_MIN_SIGNAL_UV) {
        SEGGER_RTT_printf(0, "FLOW: Span cal failed - signal too low\n");
        return false;
    }
    
    /* Convert known flow to velocity */
    /* Q = A × v → v = Q / A */
    float radius_m = ctx->calibration.pipe_diameter_m / 2.0f;
    float area_m2 = 3.14159265f * radius_m * radius_m;
    float flow_m3_per_s = known_flow_lpm / 60000.0f;
    float velocity_mps = flow_m3_per_s / area_m2;
    
    /* Calculate span coefficient */
    /* signal = span × velocity → span = signal / velocity */
    ctx->calibration.span_uv_per_mps = signal_uv / velocity_mps;
    
    SEGGER_RTT_printf(0, "FLOW: Span calibrated: %.1f µV/(m/s) at %.1f L/min\n",
                      ctx->calibration.span_uv_per_mps, known_flow_lpm);
    
    return true;
}

void flow_calc_set_auto_zero(flow_calc_ctx_t *ctx, bool enable)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->auto_zero_enabled = enable;
    
    /* Reset auto-zero state */
    ctx->stable_start_tick = 0;
    ctx->stable_signal_sum = 0.0f;
    ctx->stable_sample_count = 0;
    
    SEGGER_RTT_printf(0, "FLOW: Auto-zero %s\n", enable ? "enabled" : "disabled");
}

bool flow_calc_auto_zero_check(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->running || !ctx->auto_zero_enabled) {
        return false;
    }
    
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    /* Check minimum interval since last auto-zero */
    if (ctx->last_auto_zero_tick > 0 && 
        (now - ctx->last_auto_zero_tick) < AUTO_ZERO_MIN_INTERVAL_MS) {
        return false;
    }
    
    /* Get current signal and noise */
    float signal_uv = fabsf(ctx->state.signal_uv + ctx->calibration.zero_offset_uv);
    float noise_uv = ctx->state.noise_uv;
    
    /* Check if conditions are met for "zero flow" */
    bool is_stable = (signal_uv < AUTO_ZERO_MAX_SIGNAL_UV) && 
                     (noise_uv < AUTO_ZERO_MAX_NOISE_UV);
    
    if (is_stable) {
        if (ctx->stable_start_tick == 0) {
            /* Start tracking stable period */
            ctx->stable_start_tick = now;
            ctx->stable_signal_sum = 0.0f;
            ctx->stable_sample_count = 0;
            SEGGER_RTT_printf(0, "FLOW: Auto-zero tracking started (signal=%.1f uV)\n", signal_uv);
        }
        
        /* Accumulate samples during stable period */
        ctx->stable_signal_sum += (ctx->state.signal_uv + ctx->calibration.zero_offset_uv);
        ctx->stable_sample_count++;
        
        /* Check if stable long enough */
        uint32_t stable_duration = now - ctx->stable_start_tick;
        if (stable_duration >= AUTO_ZERO_STABLE_TIME_MS && ctx->stable_sample_count > 0) {
            /* Perform auto-zero */
            float avg_offset = ctx->stable_signal_sum / (float)ctx->stable_sample_count;
            
            /* Update zero offset */
            ctx->calibration.zero_offset_uv = avg_offset;
            ctx->last_auto_zero_tick = now;
            
            /* Reset tracking */
            ctx->stable_start_tick = 0;
            ctx->stable_signal_sum = 0.0f;
            ctx->stable_sample_count = 0;
            
            SEGGER_RTT_printf(0, "FLOW: Auto-zero complete (offset=%.1f uV, samples=%lu)\n",
                              avg_offset, ctx->stable_sample_count);
            
            /* Save to FRAM */
            flow_calc_save_calibration(ctx);
            
            return true;
        }
    } else {
        /* Conditions not met, reset tracking */
        if (ctx->stable_start_tick != 0) {
            SEGGER_RTT_printf(0, "FLOW: Auto-zero aborted (signal=%.1f, noise=%.1f)\n",
                              signal_uv, noise_uv);
            ctx->stable_start_tick = 0;
            ctx->stable_signal_sum = 0.0f;
            ctx->stable_sample_count = 0;
        }
    }
    
    return false;
}

/* ==========================================================================
 * AUTO-DETECTION FUNCTIONS
 * ========================================================================== */

bool flow_calc_is_calibrated(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    
    /* Check if calibration is valid AND has been performed (cal_date set) */
    if (!ctx->state.calibration_valid) {
        return false;
    }
    
    /* cal_date == 0 means defaults were loaded but never calibrated */
    if (ctx->calibration.cal_date == 0) {
        return false;
    }
    
    return true;
}

void flow_calc_apply_tier_defaults(flow_calc_ctx_t *ctx, flow_tier_t tier)
{
    if (ctx == NULL) {
        return;
    }
    
    flow_calibration_t *cal = &ctx->calibration;
    cal->tier = (uint8_t)tier;
    
    switch (tier) {
        case FLOW_TIER_S:
            /* MM-S: 1.5" - 2" pipes */
            cal->target_current_ma = 500;           /* 500mA target */
            cal->coil_resistance_mo = 4800;         /* ~4.8Ω typical */
            cal->supply_voltage_mv = 2400;          /* 24V / 10 */
            cal->coil_on_time_ms = 1100;            /* 1.1s on */
            cal->coil_off_time_ms = 13900;          /* 13.9s off */
            SEGGER_RTT_printf(0, "FLOW: Applied MM-S tier defaults\n");
            break;
            
        case FLOW_TIER_M:
            /* MM-M: 2.5" - 3" pipes */
            cal->target_current_ma = 750;           /* 750mA target */
            cal->coil_resistance_mo = 3200;         /* ~3.2Ω typical */
            cal->supply_voltage_mv = 2400;          /* 24V / 10 */
            cal->coil_on_time_ms = 1500;            /* 1.5s on */
            cal->coil_off_time_ms = 13500;          /* 13.5s off */
            SEGGER_RTT_printf(0, "FLOW: Applied MM-M tier defaults\n");
            break;
            
        case FLOW_TIER_L:
            /* MM-L: 4"+ pipes */
            cal->target_current_ma = 1000;          /* 1A target */
            cal->coil_resistance_mo = 2400;         /* ~2.4Ω typical */
            cal->supply_voltage_mv = 2400;          /* 24V / 10 */
            cal->coil_on_time_ms = 2000;            /* 2s on */
            cal->coil_off_time_ms = 13000;          /* 13s off */
            SEGGER_RTT_printf(0, "FLOW: Applied MM-L tier defaults\n");
            break;
            
        default:
            /* Unknown tier - use conservative defaults */
            cal->target_current_ma = 500;
            cal->coil_resistance_mo = 4800;
            cal->supply_voltage_mv = 2400;
            cal->coil_on_time_ms = 1100;
            cal->coil_off_time_ms = 13900;
            SEGGER_RTT_printf(0, "FLOW: Unknown tier, using MM-S defaults\n");
            break;
    }
}

/* ==========================================================================
 * ADC CALIBRATION FUNCTIONS
 * ========================================================================== */

/* ADC calibration state (loaded from FRAM) */
static flow_adc_cal_t s_adc_cal;
static bool s_adc_cal_loaded = false;

/* Calibration thresholds */
#define ADC_CAL_MAX_AGE_SEC         (24 * 60 * 60)  /* 24 hours */
#define ADC_CAL_TEMP_THRESHOLD_C    10.0f           /* Re-cal if temp changes >10°C */
#define ADC_CAL_NUM_SAMPLES         32              /* Samples for offset averaging */

/* Global-chop delay setting for best offset performance */
#define ADC_GLOBAL_CHOP_DELAY       ADS131M02_CFG_GC_DLY_16

bool flow_calc_adc_calibrate(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || ctx->adc == NULL) {
        return false;
    }
    
    SEGGER_RTT_printf(0, "FLOW: Starting ADC calibration...\n");
    
    /* Disable DRDY interrupt during calibration */
    ads131m02_disable_drdy_interrupt(ctx->adc);
    
    /* Enable global-chop mode for better offset stability */
    if (!ads131m02_enable_global_chop(ctx->adc, ADC_GLOBAL_CHOP_DELAY)) {
        SEGGER_RTT_printf(0, "FLOW: Failed to enable global-chop\n");
        return false;
    }
    
    /* Wait for global-chop to settle */
    vTaskDelay(pdMS_TO_TICKS(50));
    
    /* Perform automatic offset calibration on CH0 (electrode signal) */
    SEGGER_RTT_printf(0, "FLOW: Calibrating CH0 offset...\n");
    if (!ads131m02_auto_offset_cal(ctx->adc, 0, ADC_CAL_NUM_SAMPLES)) {
        SEGGER_RTT_printf(0, "FLOW: CH0 offset calibration failed\n");
        ads131m02_enable_drdy_interrupt(ctx->adc);
        return false;
    }
    
    /* Perform automatic offset calibration on CH1 (coil current sense) */
    SEGGER_RTT_printf(0, "FLOW: Calibrating CH1 offset...\n");
    if (!ads131m02_auto_offset_cal(ctx->adc, 1, ADC_CAL_NUM_SAMPLES)) {
        SEGGER_RTT_printf(0, "FLOW: CH1 offset calibration failed\n");
        ads131m02_enable_drdy_interrupt(ctx->adc);
        return false;
    }
    
    /* Read back the calibration values */
    int32_t ch0_offset, ch1_offset;
    uint32_t ch0_gain, ch1_gain;
    
    ads131m02_get_offset_cal(ctx->adc, 0, &ch0_offset);
    ads131m02_get_offset_cal(ctx->adc, 1, &ch1_offset);
    ads131m02_get_gain_cal(ctx->adc, 0, &ch0_gain);
    ads131m02_get_gain_cal(ctx->adc, 1, &ch1_gain);
    
    /* Store in calibration structure */
    s_adc_cal.magic = FLOW_ADC_CAL_MAGIC;
    s_adc_cal.version = FLOW_ADC_CAL_VERSION;
    s_adc_cal.ch0_offset = ch0_offset;
    s_adc_cal.ch0_gain = ch0_gain;
    s_adc_cal.ch1_offset = ch1_offset;
    s_adc_cal.ch1_gain = ch1_gain;
    s_adc_cal.cal_temperature_c = ctx->state.temperature_c;
    s_adc_cal.cal_timestamp = 0;  /* TODO: Get RTC time if available */
    
    s_adc_cal_loaded = true;
    
    /* Save to FRAM */
    if (!flow_calc_adc_save_calibration(ctx)) {
        SEGGER_RTT_printf(0, "FLOW: Warning - failed to save ADC calibration to FRAM\n");
    }
    
    /* Re-enable DRDY interrupt */
    ads131m02_enable_drdy_interrupt(ctx->adc);
    
    SEGGER_RTT_printf(0, "FLOW: ADC calibration complete\n");
    SEGGER_RTT_printf(0, "  CH0: offset=%d, gain=0x%06X\n", ch0_offset, ch0_gain);
    SEGGER_RTT_printf(0, "  CH1: offset=%d, gain=0x%06X\n", ch1_offset, ch1_gain);
    
    return true;
}

bool flow_calc_adc_load_calibration(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || ctx->adc == NULL || g_fram_ctx == NULL) {
        return false;
    }
    
    /* Read calibration from FRAM */
    flow_adc_cal_t cal;
    agsys_err_t err = agsys_fram_read(g_fram_ctx,
                                       AGSYS_FRAM_ADC_CAL_ADDR,
                                       (uint8_t *)&cal,
                                       sizeof(cal));
    
    if (err != AGSYS_OK) {
        SEGGER_RTT_printf(0, "FLOW: ADC cal FRAM read failed (err=%d)\n", err);
        return false;
    }
    
    /* Validate magic */
    if (cal.magic != FLOW_ADC_CAL_MAGIC) {
        SEGGER_RTT_printf(0, "FLOW: No valid ADC calibration in FRAM\n");
        return false;
    }
    
    /* Validate CRC */
    uint32_t expected_crc = crc32_calc(&cal, offsetof(flow_adc_cal_t, crc32));
    if (cal.crc32 != expected_crc) {
        SEGGER_RTT_printf(0, "FLOW: ADC cal CRC mismatch\n");
        return false;
    }
    
    /* Apply calibration to ADC */
    if (!ads131m02_set_offset_cal(ctx->adc, 0, cal.ch0_offset)) {
        SEGGER_RTT_printf(0, "FLOW: Failed to apply CH0 offset\n");
        return false;
    }
    
    if (!ads131m02_set_offset_cal(ctx->adc, 1, cal.ch1_offset)) {
        SEGGER_RTT_printf(0, "FLOW: Failed to apply CH1 offset\n");
        return false;
    }
    
    if (!ads131m02_set_gain_cal(ctx->adc, 0, cal.ch0_gain)) {
        SEGGER_RTT_printf(0, "FLOW: Failed to apply CH0 gain\n");
        return false;
    }
    
    if (!ads131m02_set_gain_cal(ctx->adc, 1, cal.ch1_gain)) {
        SEGGER_RTT_printf(0, "FLOW: Failed to apply CH1 gain\n");
        return false;
    }
    
    /* Store locally */
    memcpy(&s_adc_cal, &cal, sizeof(cal));
    s_adc_cal_loaded = true;
    
    SEGGER_RTT_printf(0, "FLOW: ADC calibration loaded from FRAM\n");
    SEGGER_RTT_printf(0, "  CH0: offset=%d, gain=0x%06X\n", cal.ch0_offset, cal.ch0_gain);
    SEGGER_RTT_printf(0, "  CH1: offset=%d, gain=0x%06X\n", cal.ch1_offset, cal.ch1_gain);
    SEGGER_RTT_printf(0, "  Cal temp: %.1f°C\n", cal.cal_temperature_c);
    
    return true;
}

bool flow_calc_adc_save_calibration(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || g_fram_ctx == NULL || !s_adc_cal_loaded) {
        return false;
    }
    
    /* Calculate CRC */
    s_adc_cal.crc32 = crc32_calc(&s_adc_cal, offsetof(flow_adc_cal_t, crc32));
    
    /* Write to FRAM */
    agsys_err_t err = agsys_fram_write(g_fram_ctx,
                                        AGSYS_FRAM_ADC_CAL_ADDR,
                                        (const uint8_t *)&s_adc_cal,
                                        sizeof(s_adc_cal));
    
    if (err != AGSYS_OK) {
        SEGGER_RTT_printf(0, "FLOW: ADC cal FRAM write failed (err=%d)\n", err);
        return false;
    }
    
    SEGGER_RTT_printf(0, "FLOW: ADC calibration saved to FRAM\n");
    return true;
}

bool flow_calc_adc_needs_calibration(flow_calc_ctx_t *ctx, float current_temp_c)
{
    if (ctx == NULL) {
        return true;
    }
    
    /* No calibration loaded = needs calibration */
    if (!s_adc_cal_loaded) {
        SEGGER_RTT_printf(0, "FLOW: ADC cal needed - no calibration loaded\n");
        return true;
    }
    
    /* Check calibration age (if timestamp is available) */
    if (s_adc_cal.cal_timestamp > 0) {
        /* TODO: Compare with current RTC time */
        /* For now, skip age check if no RTC */
    }
    
    /* Check temperature drift */
    float temp_diff = fabsf(current_temp_c - s_adc_cal.cal_temperature_c);
    if (temp_diff > ADC_CAL_TEMP_THRESHOLD_C) {
        SEGGER_RTT_printf(0, "FLOW: ADC cal needed - temp drift %.1f°C (was %.1f, now %.1f)\n",
                          temp_diff, s_adc_cal.cal_temperature_c, current_temp_c);
        return true;
    }
    
    return false;
}

bool flow_calc_adc_prepare(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || ctx->adc == NULL) {
        return false;
    }
    
    SEGGER_RTT_printf(0, "FLOW: Preparing ADC for measurement...\n");
    
    /* Try to load existing calibration from FRAM */
    bool cal_loaded = flow_calc_adc_load_calibration(ctx);
    
    /* Check if calibration is needed */
    if (!cal_loaded || flow_calc_adc_needs_calibration(ctx, ctx->state.temperature_c)) {
        SEGGER_RTT_printf(0, "FLOW: Performing ADC calibration...\n");
        if (!flow_calc_adc_calibrate(ctx)) {
            SEGGER_RTT_printf(0, "FLOW: ADC calibration failed - continuing with defaults\n");
            /* Reset calibration to defaults */
            ads131m02_reset_calibration(ctx->adc, 0);
            ads131m02_reset_calibration(ctx->adc, 1);
        }
    }
    
    /* Enable global-chop mode for measurement */
    if (!ads131m02_enable_global_chop(ctx->adc, ADC_GLOBAL_CHOP_DELAY)) {
        SEGGER_RTT_printf(0, "FLOW: Warning - failed to enable global-chop\n");
    }
    
    /* Verify ADC is responding by reading a sample */
    ads131m02_sample_t test_sample;
    if (!ads131m02_read_sample(ctx->adc, &test_sample)) {
        SEGGER_RTT_printf(0, "FLOW: ADC not responding!\n");
        return false;
    }
    
    SEGGER_RTT_printf(0, "FLOW: ADC ready (test sample: CH0=%d, CH1=%d)\n",
                      test_sample.ch0, test_sample.ch1);
    
    return true;
}

bool flow_calc_adc_quick_offset_cal(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || ctx->adc == NULL) {
        return false;
    }
    
    SEGGER_RTT_printf(0, "FLOW: Quick offset recalibration...\n");
    
    /* Disable DRDY interrupt during calibration */
    ads131m02_disable_drdy_interrupt(ctx->adc);
    
    /* Perform offset calibration on both channels with fewer samples */
    bool success = true;
    
    if (!ads131m02_auto_offset_cal(ctx->adc, 0, 16)) {
        SEGGER_RTT_printf(0, "FLOW: Quick cal CH0 failed\n");
        success = false;
    }
    
    if (!ads131m02_auto_offset_cal(ctx->adc, 1, 16)) {
        SEGGER_RTT_printf(0, "FLOW: Quick cal CH1 failed\n");
        success = false;
    }
    
    /* Update stored calibration */
    if (success && s_adc_cal_loaded) {
        ads131m02_get_offset_cal(ctx->adc, 0, &s_adc_cal.ch0_offset);
        ads131m02_get_offset_cal(ctx->adc, 1, &s_adc_cal.ch1_offset);
        s_adc_cal.cal_temperature_c = ctx->state.temperature_c;
        
        /* Save to FRAM (non-blocking, best effort) */
        flow_calc_adc_save_calibration(ctx);
    }
    
    /* Re-enable DRDY interrupt */
    ads131m02_enable_drdy_interrupt(ctx->adc);
    
    return success;
}

/* ==========================================================================
 * AUTO-DETECTION FUNCTIONS
 * ========================================================================== */

uint16_t flow_calc_measure_coil_resistance(flow_calc_ctx_t *ctx)
{
    if (ctx == NULL || ctx->adc == NULL) {
        return 0;
    }
    
    SEGGER_RTT_printf(0, "FLOW: Measuring coil resistance...\n");
    
    /* We need to:
     * 1. Turn on coil at 100% duty (no PWM limiting)
     * 2. Wait for current to stabilize (~200ms)
     * 3. Read I_SENSE ADC channel
     * 4. Calculate R = V_supply / I_measured
     * 
     * Note: This function requires the coil driver context to be passed.
     * For now, we use the external global coil context.
     */
    
    /* Get coil driver context (external) */
    extern coil_driver_ctx_t m_coil_ctx;
    
    /* Save current target and set to max for resistance measurement */
    uint32_t saved_target = m_coil_ctx.target_current_ma;
    coil_driver_set_target_current(&m_coil_ctx, 2000);  /* 2A max for measurement */
    
    /* Start coil */
    coil_driver_start(&m_coil_ctx);
    
    /* Wait for current to stabilize */
    vTaskDelay(pdMS_TO_TICKS(200));
    
    /* Read multiple samples and average */
    int64_t sum = 0;
    int count = 0;
    
    for (int i = 0; i < 100; i++) {
        ads131m02_sample_t sample;
        if (ads131m02_read_sample(ctx->adc, &sample) && sample.valid) {
            sum += sample.ch1;  /* CH1 is I_SENSE */
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    /* Stop coil and restore target */
    coil_driver_stop(&m_coil_ctx);
    coil_driver_set_target_current(&m_coil_ctx, saved_target);
    
    if (count == 0) {
        SEGGER_RTT_printf(0, "FLOW: No valid ADC samples for resistance measurement\n");
        return 0;
    }
    
    /* Calculate average current */
    int32_t avg_raw = (int32_t)(sum / count);
    float current_uv = flow_calc_raw_to_uv(avg_raw, ctx->adc_gain);
    
    /* Convert to current: I = V_sense / R_sense
     * V_sense is measured voltage, R_sense is 0.1Ω for MM-S
     * Current in mA = (V_sense in µV) / (R_sense in Ω) / 1000
     */
    float current_ma = current_uv / (CURRENT_SENSE_RESISTOR_OHM * 1000.0f);
    
    if (current_ma < 10.0f) {
        SEGGER_RTT_printf(0, "FLOW: Current too low (%.1f mA) - coil disconnected?\n", current_ma);
        return 0;
    }
    
    /* Calculate coil resistance: R = V / I
     * V_supply is 24V, I is measured current
     */
    float supply_v = (float)(ctx->calibration.supply_voltage_mv * 10) / 1000.0f;
    float resistance_ohm = supply_v / (current_ma / 1000.0f);
    uint16_t resistance_mo = (uint16_t)(resistance_ohm * 1000.0f);
    
    SEGGER_RTT_printf(0, "FLOW: Measured current=%.1f mA, resistance=%.2f Ω (%u mΩ)\n",
                      current_ma, resistance_ohm, resistance_mo);
    
    /* Store in calibration */
    ctx->calibration.coil_resistance_mo = resistance_mo;
    
    return resistance_mo;
}
