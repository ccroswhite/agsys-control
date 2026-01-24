/**
 * @file flow_calc.h
 * @brief Electromagnetic Flow Meter Signal Processing
 * 
 * Implements synchronous detection and flow calculation for the
 * capacitively-coupled electromagnetic flow meter.
 * 
 * Signal Chain:
 *   Electrodes → ADA4522 (guard) → THS4551 (diff amp) → ADS131M02 (ADC)
 * 
 * Measurement Principle:
 *   - Pulsed DC excitation at 2kHz (coil on/off)
 *   - Synchronous detection: V_flow = V_on - V_off
 *   - Faraday's law: V = B × D × v
 * 
 * Key Parameters:
 *   - Excitation: 2 kHz, tiered current (0.5A-5A by pipe size)
 *   - Expected signal: 100-500 µV
 *   - ADC: 16 kSPS, 8 samples per half-cycle
 *   - Output: 32-cycle average (16ms update rate)
 */

#ifndef FLOW_CALC_H
#define FLOW_CALC_H

#include <stdint.h>
#include <stdbool.h>
#include "ads131m0x_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

/* Excitation frequency (Hz) - coil switching rate */
#define FLOW_EXCITATION_FREQ_HZ     2000

/* ADC sample rate (Hz) - must be multiple of excitation freq */
#define FLOW_ADC_SAMPLE_RATE_HZ     16000

/* Samples per excitation half-cycle (16000 / 2000 / 2 = 4) */
#define FLOW_SAMPLES_PER_HALF       (FLOW_ADC_SAMPLE_RATE_HZ / FLOW_EXCITATION_FREQ_HZ / 2)

/* Averaging window for flow output (number of excitation cycles) */
#define FLOW_AVG_CYCLES             32

/* Signal thresholds (µV) */
#define FLOW_MIN_SIGNAL_UV          5.0f      /* Below this is noise */
#define FLOW_MAX_SIGNAL_UV          600.0f    /* Above this is saturation */
#define FLOW_ZERO_THRESHOLD_UV      5.0f      /* Below this report zero */
#define FLOW_REVERSE_THRESHOLD_UV   -5.0f     /* Negative = reverse flow */

/* Coil current fault thresholds (mA) */
#define FLOW_COIL_CURRENT_MIN_MA    50.0f     /* Below = open coil */
#define FLOW_COIL_CURRENT_MAX_MA    6000.0f   /* Above = short circuit */

/* ADC reference voltage */
#define FLOW_ADC_VREF_V             1.2f

/* Liters per gallon conversion */
#define FLOW_LITERS_PER_GALLON      3.78541f

/* ==========================================================================
 * PIPE SIZE CONFIGURATION
 * ========================================================================== */

typedef enum {
    PIPE_SIZE_1_5_INCH = 0,     /* 1.5" Schedule 80 */
    PIPE_SIZE_2_INCH,           /* 2" Schedule 80 */
    PIPE_SIZE_2_5_INCH,         /* 2.5" Schedule 40 */
    PIPE_SIZE_3_INCH,           /* 3" Schedule 40 */
    PIPE_SIZE_4_INCH,           /* 4" Schedule 40 */
    PIPE_SIZE_5_INCH,           /* 5" Schedule 40 */
    PIPE_SIZE_6_INCH,           /* 6" Schedule 40 */
    PIPE_SIZE_COUNT
} flow_pipe_size_t;

/* Pipe inner diameters (meters) for Schedule 40/80 PVC */
static const float FLOW_PIPE_DIAMETERS_M[PIPE_SIZE_COUNT] = {
    0.0381f,    /* 1.5" Sch 80: 38.1mm ID */
    0.0525f,    /* 2" Sch 80: 52.5mm ID */
    0.0635f,    /* 2.5" Sch 40: 63.5mm ID */
    0.0779f,    /* 3" Sch 40: 77.9mm ID */
    0.1023f,    /* 4" Sch 40: 102.3mm ID */
    0.1282f,    /* 5" Sch 40: 128.2mm ID */
    0.1541f,    /* 6" Sch 40: 154.1mm ID */
};

/* Default span coefficients (µV per m/s) - empirical, needs calibration */
static const float FLOW_DEFAULT_SPAN_UV_PER_MPS[PIPE_SIZE_COUNT] = {
    150.0f,     /* 1.5" */
    180.0f,     /* 2" */
    200.0f,     /* 2.5" */
    220.0f,     /* 3" */
    250.0f,     /* 4" */
    280.0f,     /* 5" */
    300.0f,     /* 6" */
};

/* ==========================================================================
 * BOARD TIER CONFIGURATION
 * ========================================================================== */

/* Board tier (detected via TIER_ID voltage divider) */
typedef enum {
    FLOW_TIER_S = 0,            /* MM-S: 1.5" - 2" pipes */
    FLOW_TIER_M,                /* MM-M: 2.5" - 4" pipes */
    FLOW_TIER_L,                /* MM-L: 5" - 6" pipes */
    FLOW_TIER_COUNT,
    FLOW_TIER_UNKNOWN = 0xFF
} flow_tier_t;

/* Tier ID voltage thresholds (mV) - from power board voltage dividers */
#define FLOW_TIER_S_VOLTAGE_MV      825     /* 0.825V ± 10% */
#define FLOW_TIER_M_VOLTAGE_MV      1650    /* 1.65V ± 10% */
#define FLOW_TIER_L_VOLTAGE_MV      2475    /* 2.475V ± 10% */
#define FLOW_TIER_TOLERANCE_MV      165     /* ±10% tolerance */

/* ==========================================================================
 * HARDWARE CONSTANTS
 * ========================================================================== */

/* Current sense resistor and gain for coil current measurement */
#define FLOW_CURRENT_SENSE_RESISTOR_OHM     0.1f    /* MM-S uses 0.1Ω */
#define FLOW_CURRENT_SENSE_GAIN             1.0f    /* Direct measurement for MM-S */

/* ADC full scale (24-bit signed) */
#define FLOW_ADC_FULL_SCALE                 8388607 /* 2^23 - 1 */

/* ==========================================================================
 * ADC CALIBRATION CONFIGURATION
 * ========================================================================== */

/* Calibration thresholds */
#define FLOW_ADC_CAL_MAX_AGE_SEC            (24 * 60 * 60)  /* 24 hours */
#define FLOW_ADC_CAL_TEMP_THRESHOLD_C       10.0f           /* Re-cal if temp changes >10°C */
#define FLOW_ADC_CAL_NUM_SAMPLES            32              /* Samples for offset averaging */

/* Global-chop delay setting for best offset performance */
#define FLOW_ADC_GLOBAL_CHOP_DELAY          ADS131M0X_GC_DLY_16

/* ==========================================================================
 * CALIBRATION DATA (stored in FRAM)
 * ========================================================================== */

#define FLOW_CAL_MAGIC              0x464C4F57  /* "FLOW" */
#define FLOW_CAL_VERSION            1

typedef struct {
    uint32_t magic;                 /* FLOW_CAL_MAGIC */
    uint8_t  version;               /* Calibration data version */
    uint8_t  pipe_size;             /* flow_pipe_size_t */
    uint8_t  tier;                  /* flow_tier_t (auto-detected) */
    uint8_t  auto_zero_enabled;     /* Auto-zero feature enabled */
    
    /* Zero offset (µV) - measured with no flow */
    float zero_offset_uv;
    
    /* Span coefficient (µV per m/s) - from calibration */
    float span_uv_per_mps;
    
    /* Temperature coefficients */
    float temp_coeff_offset;        /* µV/°C */
    float temp_coeff_span;          /* fractional change per °C */
    
    /* Reference temperature for calibration (°C) */
    float ref_temp_c;
    
    /* Pipe inner diameter (m) - can override default */
    float pipe_diameter_m;
    
    /* K-factor override (pulses per liter, 0 = use mag mode) */
    float k_factor;
    
    /* Duty cycle configuration (thermal management) */
    uint16_t coil_on_time_ms;       /* Measurement duration (500-10000) */
    uint16_t coil_off_time_ms;      /* Sleep duration (0-60000, 0=continuous) */
    
    /* Display and reporting intervals */
    uint8_t  display_update_sec;    /* Display refresh interval (1-60, default 15) */
    uint8_t  lora_report_mult;      /* LoRa report = display_update_sec * mult (1-10, default 4) */
    uint8_t  reserved[2];           /* Alignment padding */
    
    /* PWM current control parameters */
    uint16_t target_current_ma;     /* Target coil current in mA */
    uint16_t supply_voltage_mv;     /* Supply voltage in mV (stored as /10, e.g., 2400 = 24V) */
    uint16_t coil_resistance_mo;    /* Coil resistance in milliohms */
    
    /* Calibration metadata */
    uint32_t cal_date;              /* Unix timestamp */
    uint32_t serial_number;
    
    /* CRC32 of above fields */
    uint32_t crc32;
} flow_calibration_t;

/* ==========================================================================
 * FLOW STATE (output)
 * ========================================================================== */

typedef struct {
    /* Current measurements */
    float flow_rate_lpm;            /* Flow rate (liters per minute) */
    float flow_rate_gpm;            /* Flow rate (gallons per minute) */
    float velocity_mps;             /* Flow velocity (m/s) */
    float signal_uv;                /* Raw signal level (µV) after sync detect */
    
    /* Totalization */
    float total_volume_l;           /* Total volume (liters) */
    float total_volume_gal;         /* Total volume (gallons) */
    
    /* Statistics (for current reporting period) */
    float min_flow_lpm;
    float max_flow_lpm;
    float avg_flow_lpm;
    uint32_t sample_count;
    
    /* Status flags */
    bool reverse_flow;              /* Negative flow detected */
    bool signal_low;                /* Signal below minimum threshold */
    bool signal_high;               /* Signal above maximum (saturation) */
    bool coil_fault;                /* Coil current out of range */
    bool calibration_valid;         /* Valid calibration loaded */
    
    /* Diagnostics */
    float coil_current_ma;          /* Measured coil current (from ADC ch1) */
    float temperature_c;            /* Board temperature */
    float noise_uv;                 /* Estimated noise level (stddev) */
    uint8_t signal_quality;         /* 0-100% quality indicator */
} flow_state_t;

/* ==========================================================================
 * SYNCHRONOUS DETECTOR STATE (internal)
 * ========================================================================== */

typedef struct {
    /* Sample accumulators for ON and OFF phases */
    int64_t sum_on;
    int64_t sum_off;
    uint32_t count_on;
    uint32_t count_off;
    
    /* Current excitation phase tracking */
    bool coil_on;
    uint32_t phase_sample_count;
    
    /* Output averaging buffer (circular) */
    float cycle_results[FLOW_AVG_CYCLES];
    uint32_t cycle_index;
    uint32_t cycles_valid;
    
    /* Coil current accumulator (for fault detection) */
    int64_t sum_coil_current;
    uint32_t count_coil;
    
    /* Noise estimation (sum of squares for variance calc) */
    float sum_sq_diff;
    float last_result;
} sync_detector_t;

/* ==========================================================================
 * FLOW CALCULATOR CONTEXT
 * ========================================================================== */

/* Auto-zero configuration */
#define AUTO_ZERO_STABLE_TIME_MS    10000   /* 10 seconds of stable signal */
#define AUTO_ZERO_MAX_SIGNAL_UV     20.0f   /* Max signal to consider "zero" */
#define AUTO_ZERO_MAX_NOISE_UV      5.0f    /* Max noise during stable period */
#define AUTO_ZERO_MIN_INTERVAL_MS   300000  /* Min 5 minutes between auto-zeros */

typedef struct {
    /* ADC context (external, must be initialized) */
    ads131m0x_ctx_t *adc;
    
    /* Calibration data */
    flow_calibration_t calibration;
    
    /* Synchronous detector state */
    sync_detector_t detector;
    
    /* Output state */
    flow_state_t state;
    
    /* Timing */
    uint32_t last_update_tick;
    uint32_t period_start_tick;
    
    /* Configuration */
    ads131m0x_gain_t adc_gain;      /* Current PGA gain setting */
    bool auto_gain;                 /* Enable auto-ranging */
    
    /* Auto-zero state */
    bool auto_zero_enabled;
    uint32_t stable_start_tick;     /* When signal became stable */
    uint32_t last_auto_zero_tick;   /* Last auto-zero time */
    float stable_signal_sum;        /* Sum for averaging during stable period */
    uint32_t stable_sample_count;
    
    /* Status */
    bool initialized;
    bool running;
} flow_calc_ctx_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize flow calculator
 * @param ctx Flow calculator context
 * @param adc ADC context (must be initialized)
 * @return true on success
 */
bool flow_calc_init(flow_calc_ctx_t *ctx, ads131m0x_ctx_t *adc);

/**
 * @brief Load calibration data from FRAM
 * @param ctx Flow calculator context
 * @return true if valid calibration loaded
 */
bool flow_calc_load_calibration(flow_calc_ctx_t *ctx);

/**
 * @brief Save calibration data to FRAM
 * @param ctx Flow calculator context
 * @return true on success
 */
bool flow_calc_save_calibration(flow_calc_ctx_t *ctx);

/**
 * @brief Set default calibration for pipe size
 * @param ctx Flow calculator context
 * @param pipe_size Pipe size enum
 */
void flow_calc_set_defaults(flow_calc_ctx_t *ctx, flow_pipe_size_t pipe_size);

/**
 * @brief Detect board tier from TIER_ID ADC reading
 * @param tier_id_mv TIER_ID voltage in millivolts
 * @return Detected tier or FLOW_TIER_UNKNOWN
 */
flow_tier_t flow_calc_detect_tier(uint32_t tier_id_mv);

/**
 * @brief Start flow measurement
 * @param ctx Flow calculator context
 * @return true on success
 */
bool flow_calc_start(flow_calc_ctx_t *ctx);

/**
 * @brief Stop flow measurement
 * @param ctx Flow calculator context
 */
void flow_calc_stop(flow_calc_ctx_t *ctx);

/**
 * @brief Process ADC sample (called from ADC ISR or task at 16kHz)
 * 
 * This is the main signal processing function. Must be called at ADC sample rate.
 * Performs synchronous detection and updates flow state.
 * 
 * @param ctx Flow calculator context
 * @param sample ADC sample (ch0 = electrode signal, ch1 = coil current)
 * @param coil_on Current coil excitation state (true = field on)
 */
void flow_calc_process_sample(flow_calc_ctx_t *ctx, 
                               const ads131m0x_sample_t *sample,
                               bool coil_on);

/**
 * @brief Get current flow state (thread-safe copy)
 * @param ctx Flow calculator context
 * @param state Output: copy of current state
 */
void flow_calc_get_state(flow_calc_ctx_t *ctx, flow_state_t *state);

/**
 * @brief Reset totalization counters
 * @param ctx Flow calculator context
 */
void flow_calc_reset_total(flow_calc_ctx_t *ctx);

/**
 * @brief Reset statistics (min/max/avg) for new reporting period
 * @param ctx Flow calculator context
 */
void flow_calc_reset_stats(flow_calc_ctx_t *ctx);

/**
 * @brief Perform zero calibration (call with no flow)
 * @param ctx Flow calculator context
 * @return true if zero calibration successful
 */
bool flow_calc_zero_calibrate(flow_calc_ctx_t *ctx);

/**
 * @brief Set span calibration from known flow rate
 * @param ctx Flow calculator context
 * @param known_flow_lpm Known flow rate in L/min
 * @return true if span calibration successful
 */
bool flow_calc_span_calibrate(flow_calc_ctx_t *ctx, float known_flow_lpm);

/**
 * @brief Check if auto-zero conditions are met and perform if so
 * 
 * Auto-zero triggers when:
 *   - Signal is stable (low noise) for >10 seconds
 *   - Signal magnitude is below threshold (near zero)
 *   - No recent flow detected
 * 
 * Call this periodically (e.g., every second) from main loop.
 * 
 * @param ctx Flow calculator context
 * @return true if auto-zero was performed
 */
bool flow_calc_auto_zero_check(flow_calc_ctx_t *ctx);

/**
 * @brief Enable/disable auto-zero feature
 * @param ctx Flow calculator context
 * @param enable true to enable auto-zero
 */
void flow_calc_set_auto_zero(flow_calc_ctx_t *ctx, bool enable);

/**
 * @brief Convert raw ADC value to voltage (µV)
 * @param raw Raw 24-bit ADC value (sign-extended)
 * @param gain Current PGA gain setting
 * @return Voltage in microvolts
 */
float flow_calc_raw_to_uv(int32_t raw, ads131m0x_gain_t gain);

/**
 * @brief Get pipe inner diameter for pipe size
 * @param pipe_size Pipe size enum
 * @return Inner diameter in meters
 */
float flow_calc_get_pipe_diameter(flow_pipe_size_t pipe_size);

/**
 * @brief Check if calibration is valid (has been performed)
 * 
 * Returns true only if:
 *   - Calibration was loaded from FRAM with valid CRC
 *   - cal_date is non-zero (has been calibrated at least once)
 * 
 * @param ctx Flow calculator context
 * @return true if device has valid calibration
 */
bool flow_calc_is_calibrated(flow_calc_ctx_t *ctx);

/**
 * @brief Auto-detect coil resistance by measuring current
 * 
 * Turns on coil at known PWM duty cycle and measures resulting current
 * via I_SENSE ADC channel. Calculates coil resistance and stores in
 * calibration data.
 * 
 * Should be called:
 *   - On first boot (no calibration)
 *   - When tier changes
 *   - During factory calibration
 * 
 * @param ctx Flow calculator context
 * @return Measured coil resistance in milliohms, or 0 on failure
 */
uint16_t flow_calc_measure_coil_resistance(flow_calc_ctx_t *ctx);

/**
 * @brief Apply tier-specific defaults based on detected tier
 * 
 * Sets coil parameters (resistance, target current) based on tier:
 *   - MM-S: 1.5"-2" pipes, 0.1Ω sense resistor
 *   - MM-M: 2.5"-3" pipes, different coil specs
 *   - MM-L: 4"+ pipes, different coil specs
 * 
 * @param ctx Flow calculator context
 * @param tier Detected tier
 */
void flow_calc_apply_tier_defaults(flow_calc_ctx_t *ctx, flow_tier_t tier);

/* ==========================================================================
 * ADC CALIBRATION FUNCTIONS
 * ========================================================================== */

/**
 * @brief ADC calibration data (stored in FRAM alongside flow calibration)
 */
typedef struct {
    uint32_t magic;                 /* 0x41444343 = "ADCC" */
    uint8_t  version;
    uint8_t  reserved[3];
    
    /* Channel 0 (electrode signal) calibration */
    int32_t  ch0_offset;            /* Offset calibration value */
    uint32_t ch0_gain;              /* Gain calibration value (0x800000 = 1.0) */
    
    /* Channel 1 (coil current sense) calibration */
    int32_t  ch1_offset;            /* Offset calibration value */
    uint32_t ch1_gain;              /* Gain calibration value (0x800000 = 1.0) */
    
    /* Calibration metadata */
    uint32_t cal_timestamp;         /* Unix timestamp of last calibration */
    float    cal_temperature_c;     /* Temperature at calibration */
    
    uint32_t crc32;
} flow_adc_cal_t;

#define FLOW_ADC_CAL_MAGIC          0x41444343  /* "ADCC" */
#define FLOW_ADC_CAL_VERSION        1

/**
 * @brief Perform full ADC calibration (offset + gain for both channels)
 * 
 * This function:
 *   1. Enables global-chop mode for offset drift reduction
 *   2. Performs automatic offset calibration on both channels
 *   3. Optionally performs gain calibration if reference is available
 *   4. Saves calibration to FRAM
 * 
 * Should be called:
 *   - On first boot (no ADC calibration stored)
 *   - Periodically (e.g., daily) to compensate for drift
 *   - When temperature changes significantly (>10°C from cal temp)
 * 
 * @param ctx Flow calculator context
 * @return true on success
 */
bool flow_calc_adc_calibrate(flow_calc_ctx_t *ctx);

/**
 * @brief Load ADC calibration from FRAM and apply to ADC
 * @param ctx Flow calculator context
 * @return true if valid calibration loaded and applied
 */
bool flow_calc_adc_load_calibration(flow_calc_ctx_t *ctx);

/**
 * @brief Save current ADC calibration to FRAM
 * @param ctx Flow calculator context
 * @return true on success
 */
bool flow_calc_adc_save_calibration(flow_calc_ctx_t *ctx);

/**
 * @brief Check if ADC calibration is needed
 * 
 * Returns true if:
 *   - No calibration stored in FRAM
 *   - Calibration is older than 24 hours
 *   - Temperature has changed >10°C since calibration
 * 
 * @param ctx Flow calculator context
 * @param current_temp_c Current temperature in Celsius
 * @return true if calibration is recommended
 */
bool flow_calc_adc_needs_calibration(flow_calc_ctx_t *ctx, float current_temp_c);

/**
 * @brief Perform pre-measurement ADC setup
 * 
 * Call this before starting flow measurements. It:
 *   1. Loads ADC calibration from FRAM (or performs calibration if needed)
 *   2. Enables global-chop mode for offset drift reduction
 *   3. Verifies ADC is responding correctly
 * 
 * @param ctx Flow calculator context
 * @return true if ADC is ready for measurement
 */
bool flow_calc_adc_prepare(flow_calc_ctx_t *ctx);

/**
 * @brief Quick offset recalibration (faster than full calibration)
 * 
 * Performs offset-only calibration on both channels. Useful for:
 *   - Periodic drift correction during operation
 *   - Temperature compensation
 * 
 * @param ctx Flow calculator context
 * @return true on success
 */
bool flow_calc_adc_quick_offset_cal(flow_calc_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* FLOW_CALC_H */
