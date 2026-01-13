/**
 * @file coil_driver.h
 * @brief Coil Excitation Driver for Electromagnetic Flow Meter
 * 
 * Uses nRF52840 TIMER peripheral with PPI to generate precise 2kHz
 * square wave for coil excitation. Hardware-based timing ensures
 * jitter-free synchronization with ADC sampling.
 * 
 * Features:
 *   - 2kHz excitation frequency (250µs half-period)
 *   - Hardware timer + PPI for precise GPIO toggle
 *   - Synchronized coil state tracking for ADC processing
 *   - Soft-start capability to limit inrush current
 */

#ifndef COIL_DRIVER_H
#define COIL_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

/* Excitation frequency */
#define COIL_FREQ_HZ            2000
#define COIL_PERIOD_US          (1000000 / COIL_FREQ_HZ)     /* 500µs */
#define COIL_HALF_PERIOD_US     (COIL_PERIOD_US / 2)         /* 250µs */

/* Duty cycle defaults (thermal management)
 * Default: 1.1s on (includes 100ms soft-start), 13.9s off
 * Cycle: 15 seconds, ~7.3% duty cycle
 * Gives 62 averaged readings per measurement window
 */
#define COIL_DEFAULT_ON_TIME_MS     1100    /* 1.1s measurement (includes soft-start) */
#define COIL_DEFAULT_OFF_TIME_MS    13900   /* 13.9s sleep */
#define COIL_MIN_ON_TIME_MS         500     /* Minimum for stable reading */
#define COIL_MAX_ON_TIME_MS         10000   /* Maximum 10s */
#define COIL_MIN_OFF_TIME_MS        0       /* 0 = continuous mode */
#define COIL_MAX_OFF_TIME_MS        60000   /* Maximum 60s off */

/* PWM current control
 * PWM frequency for current limiting (much faster than 2kHz excitation)
 * Open-loop control: PWM_duty = (I_target * R_coil) / V_supply
 */
#define COIL_PWM_FREQ_HZ            50000   /* 50kHz PWM for current control */
#define COIL_PWM_INSTANCE           0       /* PWM0 instance */

/* Default electrical parameters (can be overridden per tier) */
#define COIL_DEFAULT_TARGET_MA      1000    /* 1A default target */
#define COIL_DEFAULT_SUPPLY_MV      24000   /* 24V default supply */
#define COIL_DEFAULT_RESISTANCE_MO  4300    /* 4.3 ohm default (2" MM-S) */

/* Timer instance (TIMER2 - not used by SoftDevice or FreeRTOS) */
#define COIL_TIMER_INSTANCE     2

/* PPI channels for hardware GPIO toggle */
#define COIL_PPI_CH_SET         0
#define COIL_PPI_CH_CLR         1

/* ==========================================================================
 * TYPES
 * ========================================================================== */

typedef enum {
    COIL_STATE_OFF = 0,         /* Coil disabled */
    COIL_STATE_SOFT_START,      /* Ramping up (100ms) */
    COIL_STATE_MEASURING,       /* Active measurement */
    COIL_STATE_SLEEPING         /* Thermal sleep period */
} coil_state_t;

typedef struct {
    uint32_t gpio_pin;          /* Coil gate GPIO pin */
    bool     initialized;
    bool     running;
    volatile bool coil_on;      /* Current coil state (for ADC sync) */
    uint32_t cycle_count;       /* Total excitation cycles since start */
    
    /* Duty cycle configuration (thermal management) */
    uint32_t on_time_ms;        /* Measurement duration */
    uint32_t off_time_ms;       /* Sleep duration (0 = continuous) */
    
    /* Duty cycle state */
    coil_state_t state;         /* Current duty cycle state */
    uint32_t state_start_tick;  /* When current state started */
    uint32_t measurement_count; /* Measurements since boot */
    
    /* PWM current control (open-loop) */
    uint32_t target_current_ma; /* Target coil current in mA */
    uint32_t supply_voltage_mv; /* Supply voltage in mV */
    uint32_t coil_resistance_mo;/* Coil resistance in milliohms */
    uint16_t pwm_duty;          /* Calculated PWM duty (0-1000 = 0-100%) */
    
    /* I_SENSE verification (optional, sampled once per cycle) */
    uint16_t last_isense_mv;    /* Last measured current sense voltage */
    bool     coil_fault;        /* Fault detected (open/short) */
} coil_driver_ctx_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize coil driver
 * @param ctx Coil driver context
 * @param gpio_pin GPIO pin for MOSFET gate
 * @return true on success
 */
bool coil_driver_init(coil_driver_ctx_t *ctx, uint32_t gpio_pin);

/**
 * @brief Start coil excitation
 * @param ctx Coil driver context
 * @return true on success
 */
bool coil_driver_start(coil_driver_ctx_t *ctx);

/**
 * @brief Stop coil excitation
 * @param ctx Coil driver context
 */
void coil_driver_stop(coil_driver_ctx_t *ctx);

/**
 * @brief Get current coil state
 * @param ctx Coil driver context
 * @return true if coil is energized (field ON)
 */
bool coil_driver_get_state(coil_driver_ctx_t *ctx);

/**
 * @brief Get cycle count since start
 * @param ctx Coil driver context
 * @return Number of complete ON/OFF cycles
 */
uint32_t coil_driver_get_cycle_count(coil_driver_ctx_t *ctx);

/**
 * @brief Soft-start coil (ramp up duty cycle)
 * 
 * Gradually increases duty cycle over ~100ms to limit inrush current.
 * Call this instead of coil_driver_start() for initial power-on.
 * 
 * @param ctx Coil driver context
 * @return true on success
 */
bool coil_driver_soft_start(coil_driver_ctx_t *ctx);

/**
 * @brief Set duty cycle timing
 * @param ctx Coil driver context
 * @param on_time_ms Measurement duration in ms (500-10000)
 * @param off_time_ms Sleep duration in ms (0-60000, 0=continuous)
 */
void coil_driver_set_duty_cycle(coil_driver_ctx_t *ctx, 
                                 uint32_t on_time_ms, 
                                 uint32_t off_time_ms);

/**
 * @brief Process duty cycle state machine
 * 
 * Call this periodically (e.g., every 100ms) to manage on/off transitions.
 * Returns true when in measurement state (coil active).
 * 
 * @param ctx Coil driver context
 * @return true if currently measuring (coil active)
 */
bool coil_driver_tick(coil_driver_ctx_t *ctx);

/**
 * @brief Check if currently in measurement window
 * @param ctx Coil driver context
 * @return true if coil is active and measuring
 */
bool coil_driver_is_measuring(coil_driver_ctx_t *ctx);

/**
 * @brief Set target current (open-loop PWM control)
 * @param ctx Coil driver context
 * @param target_ma Target current in milliamps
 */
void coil_driver_set_target_current(coil_driver_ctx_t *ctx, uint32_t target_ma);

/**
 * @brief Set electrical parameters for PWM calculation
 * @param ctx Coil driver context
 * @param supply_mv Supply voltage in millivolts
 * @param resistance_mo Coil resistance in milliohms
 */
void coil_driver_set_electrical_params(coil_driver_ctx_t *ctx,
                                        uint32_t supply_mv,
                                        uint32_t resistance_mo);

/**
 * @brief Update I_SENSE reading for fault detection
 * 
 * Call this once per measurement cycle with the ADC reading from I_SENSE.
 * Used for fault detection, not real-time control.
 * 
 * @param ctx Coil driver context
 * @param isense_mv Current sense voltage in millivolts
 */
void coil_driver_update_isense(coil_driver_ctx_t *ctx, uint16_t isense_mv);

/**
 * @brief Check for coil fault
 * @param ctx Coil driver context
 * @return true if fault detected (open or short circuit)
 */
bool coil_driver_has_fault(coil_driver_ctx_t *ctx);

/**
 * @brief Get current PWM duty cycle
 * @param ctx Coil driver context
 * @return PWM duty in 0.1% units (0-1000 = 0-100%)
 */
uint16_t coil_driver_get_pwm_duty(coil_driver_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* COIL_DRIVER_H */
