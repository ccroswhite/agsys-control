/**
 * @file coil_driver.c
 * @brief Coil Excitation Driver Implementation
 * 
 * Uses TIMER2 with GPIOTE and PPI for hardware-based 2kHz square wave.
 * This approach provides sub-microsecond jitter, critical for synchronous
 * detection accuracy.
 * 
 * Timer Configuration:
 *   - 16MHz clock (1MHz after prescaler)
 *   - CC[0] = 250 (250µs = half period)
 *   - CC[1] = 500 (500µs = full period, triggers clear)
 *   - GPIOTE toggles GPIO on CC[0] match
 */

#include "coil_driver.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_timer.h"
#include "nrf_gpiote.h"
#include "nrf_ppi.h"
#include "nrf_delay.h"
#include "SEGGER_RTT.h"

/* ==========================================================================
 * HARDWARE DEFINITIONS
 * ========================================================================== */

/* Timer peripheral base address */
#if COIL_TIMER_INSTANCE == 0
    #define COIL_TIMER      NRF_TIMER0
#elif COIL_TIMER_INSTANCE == 1
    #define COIL_TIMER      NRF_TIMER1
#elif COIL_TIMER_INSTANCE == 2
    #define COIL_TIMER      NRF_TIMER2
#elif COIL_TIMER_INSTANCE == 3
    #define COIL_TIMER      NRF_TIMER3
#elif COIL_TIMER_INSTANCE == 4
    #define COIL_TIMER      NRF_TIMER4
#else
    #error "Invalid COIL_TIMER_INSTANCE"
#endif

/* Timer clock: 16MHz / 2^4 = 1MHz (1µs resolution) */
#define TIMER_PRESCALER     4
#define TIMER_FREQ_HZ       (16000000 >> TIMER_PRESCALER)
#define TICKS_PER_US        (TIMER_FREQ_HZ / 1000000)

/* Compare values for 2kHz (500µs period) */
#define CC_HALF_PERIOD      (COIL_HALF_PERIOD_US * TICKS_PER_US)  /* 250 ticks */
#define CC_FULL_PERIOD      (COIL_PERIOD_US * TICKS_PER_US)       /* 500 ticks */

/* GPIOTE channel for coil GPIO */
#define GPIOTE_CHANNEL      0

/* ==========================================================================
 * STATIC VARIABLES
 * ========================================================================== */

static coil_driver_ctx_t *s_ctx = NULL;

/* ==========================================================================
 * TIMER INTERRUPT HANDLER
 * ========================================================================== */

/* Timer IRQ handler - updates coil state tracking */
void TIMER2_IRQHandler(void)
{
    if (COIL_TIMER->EVENTS_COMPARE[0]) {
        COIL_TIMER->EVENTS_COMPARE[0] = 0;
        
        if (s_ctx != NULL) {
            /* Toggle state tracking (GPIO is toggled by hardware) */
            s_ctx->coil_on = !s_ctx->coil_on;
            
            /* Count complete cycles (every other toggle) */
            if (!s_ctx->coil_on) {
                s_ctx->cycle_count++;
            }
        }
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool coil_driver_init(coil_driver_ctx_t *ctx, uint32_t gpio_pin)
{
    if (ctx == NULL) {
        return false;
    }
    
    /* Initialize context */
    ctx->gpio_pin = gpio_pin;
    ctx->initialized = false;
    ctx->running = false;
    ctx->coil_on = false;
    ctx->cycle_count = 0;
    
    /* Set default duty cycle (thermal management) */
    ctx->on_time_ms = COIL_DEFAULT_ON_TIME_MS;
    ctx->off_time_ms = COIL_DEFAULT_OFF_TIME_MS;
    ctx->state = COIL_STATE_OFF;
    ctx->state_start_tick = 0;
    ctx->measurement_count = 0;
    
    /* Set default PWM current control parameters */
    ctx->target_current_ma = COIL_DEFAULT_TARGET_MA;
    ctx->supply_voltage_mv = COIL_DEFAULT_SUPPLY_MV;
    ctx->coil_resistance_mo = COIL_DEFAULT_RESISTANCE_MO;
    ctx->pwm_duty = 0;
    ctx->last_isense_mv = 0;
    ctx->coil_fault = false;
    
    /* Store context for IRQ handler */
    s_ctx = ctx;
    
    /* Configure GPIO as output, initially low */
    nrf_gpio_cfg_output(gpio_pin);
    nrf_gpio_pin_clear(gpio_pin);
    
    /* Stop timer if running */
    nrf_timer_task_trigger(COIL_TIMER, NRF_TIMER_TASK_STOP);
    nrf_timer_task_trigger(COIL_TIMER, NRF_TIMER_TASK_CLEAR);
    
    /* Configure timer */
    nrf_timer_mode_set(COIL_TIMER, NRF_TIMER_MODE_TIMER);
    nrf_timer_bit_width_set(COIL_TIMER, NRF_TIMER_BIT_WIDTH_16);
    nrf_timer_frequency_set(COIL_TIMER, NRF_TIMER_FREQ_1MHz);
    
    /* Set compare values */
    nrf_timer_cc_set(COIL_TIMER, NRF_TIMER_CC_CHANNEL0, CC_HALF_PERIOD);  /* Toggle at 250µs */
    nrf_timer_cc_set(COIL_TIMER, NRF_TIMER_CC_CHANNEL1, CC_FULL_PERIOD);  /* Clear at 500µs */
    
    /* Enable shorts: CC[1] -> CLEAR (auto-reload) */
    nrf_timer_shorts_enable(COIL_TIMER, NRF_TIMER_SHORT_COMPARE1_CLEAR_MASK);
    
    /* Configure GPIOTE for toggle on event */
    nrf_gpiote_task_configure(NRF_GPIOTE, 
                               GPIOTE_CHANNEL,
                               gpio_pin,
                               NRF_GPIOTE_POLARITY_TOGGLE,
                               NRF_GPIOTE_INITIAL_VALUE_LOW);
    
    /* Configure PPI: TIMER CC[0] -> GPIOTE TOGGLE */
    nrf_ppi_channel_endpoint_setup(NRF_PPI,
                                    (nrf_ppi_channel_t)COIL_PPI_CH_SET,
                                    (uint32_t)&COIL_TIMER->EVENTS_COMPARE[0],
                                    nrf_gpiote_task_address_get(NRF_GPIOTE, 
                                        nrf_gpiote_out_task_get(GPIOTE_CHANNEL)));
    
    /* Enable interrupt for state tracking */
    nrf_timer_int_enable(COIL_TIMER, NRF_TIMER_INT_COMPARE0_MASK);
    NVIC_SetPriority(TIMER2_IRQn, 6);  /* Lower priority than ADC */
    NVIC_EnableIRQ(TIMER2_IRQn);
    
    ctx->initialized = true;
    
    SEGGER_RTT_printf(0, "COIL: Initialized (pin=%d, freq=%dHz, half=%dus)\n",
                      gpio_pin, COIL_FREQ_HZ, COIL_HALF_PERIOD_US);
    
    return true;
}

bool coil_driver_start(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (ctx->running) {
        return true;  /* Already running */
    }
    
    /* Reset state */
    ctx->coil_on = false;
    ctx->cycle_count = 0;
    
    /* Ensure GPIO starts low */
    nrf_gpio_pin_clear(ctx->gpio_pin);
    
    /* Enable GPIOTE task */
    nrf_gpiote_task_enable(NRF_GPIOTE, GPIOTE_CHANNEL);
    
    /* Enable PPI channel */
    nrf_ppi_channel_enable(NRF_PPI, (nrf_ppi_channel_t)COIL_PPI_CH_SET);
    
    /* Clear and start timer */
    nrf_timer_task_trigger(COIL_TIMER, NRF_TIMER_TASK_CLEAR);
    nrf_timer_task_trigger(COIL_TIMER, NRF_TIMER_TASK_START);
    
    ctx->running = true;
    
    SEGGER_RTT_printf(0, "COIL: Started\n");
    return true;
}

void coil_driver_stop(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    
    /* Stop timer */
    nrf_timer_task_trigger(COIL_TIMER, NRF_TIMER_TASK_STOP);
    
    /* Disable PPI channel */
    nrf_ppi_channel_disable(NRF_PPI, (nrf_ppi_channel_t)COIL_PPI_CH_SET);
    
    /* Disable GPIOTE task */
    nrf_gpiote_task_disable(NRF_GPIOTE, GPIOTE_CHANNEL);
    
    /* Ensure coil is off */
    nrf_gpio_pin_clear(ctx->gpio_pin);
    
    ctx->running = false;
    ctx->coil_on = false;
    
    SEGGER_RTT_printf(0, "COIL: Stopped (cycles=%lu)\n", ctx->cycle_count);
}

bool coil_driver_get_state(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    return ctx->coil_on;
}

uint32_t coil_driver_get_cycle_count(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    return ctx->cycle_count;
}

bool coil_driver_soft_start(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* Calculate PWM duty if not already done */
    if (ctx->pwm_duty == 0) {
        calculate_pwm_duty(ctx);
    }
    
    SEGGER_RTT_printf(0, "COIL: Soft-start beginning (target duty=%u.%u%%)\n",
                      ctx->pwm_duty / 10, ctx->pwm_duty % 10);
    
    ctx->state = COIL_STATE_SOFT_START;
    ctx->state_start_tick = xTaskGetTickCount();
    
    /* Soft-start: gradually increase duty cycle over 100ms
     * This limits inrush current through the coil inductance.
     * 
     * We ramp from 10% of target duty to 100% of target duty.
     * The target duty is calculated from I_target * R_coil / V_supply.
     */
    
    /* Calculate target on-time within the 2kHz half-period */
    /* pwm_duty is 0-1000 (0-100%), apply to half-period */
    uint32_t target_on_us = (COIL_HALF_PERIOD_US * ctx->pwm_duty) / 1000;
    if (target_on_us < 10) target_on_us = 10;  /* Minimum pulse width */
    if (target_on_us > COIL_HALF_PERIOD_US) target_on_us = COIL_HALF_PERIOD_US;
    
    /* Ramp from 10% to 100% of target in 10 steps */
    for (int ramp = 10; ramp <= 100; ramp += 10) {
        uint32_t on_time_us = (target_on_us * ramp) / 100;
        uint32_t off_time_us = COIL_PERIOD_US - on_time_us;
        
        /* Run at this duty cycle for ~10ms (20 cycles at 2kHz) */
        for (int i = 0; i < 20; i++) {
            nrf_gpio_pin_set(ctx->gpio_pin);
            nrf_delay_us(on_time_us);
            nrf_gpio_pin_clear(ctx->gpio_pin);
            nrf_delay_us(off_time_us);
        }
    }
    
    SEGGER_RTT_printf(0, "COIL: Soft-start complete (on=%luus per cycle)\n", target_on_us);
    
    /* Now start normal hardware-driven operation at target duty */
    ctx->state = COIL_STATE_MEASURING;
    ctx->state_start_tick = xTaskGetTickCount();
    return coil_driver_start(ctx);
}

void coil_driver_set_duty_cycle(coil_driver_ctx_t *ctx, 
                                 uint32_t on_time_ms, 
                                 uint32_t off_time_ms)
{
    if (ctx == NULL) {
        return;
    }
    
    /* Clamp values to valid range */
    if (on_time_ms < COIL_MIN_ON_TIME_MS) on_time_ms = COIL_MIN_ON_TIME_MS;
    if (on_time_ms > COIL_MAX_ON_TIME_MS) on_time_ms = COIL_MAX_ON_TIME_MS;
    if (off_time_ms > COIL_MAX_OFF_TIME_MS) off_time_ms = COIL_MAX_OFF_TIME_MS;
    
    ctx->on_time_ms = on_time_ms;
    ctx->off_time_ms = off_time_ms;
    
    float duty_pct = (off_time_ms == 0) ? 100.0f : 
                     (100.0f * on_time_ms / (on_time_ms + off_time_ms));
    
    SEGGER_RTT_printf(0, "COIL: Duty cycle set to %lums on / %lums off (%.1f%%)\n",
                      on_time_ms, off_time_ms, duty_pct);
}

bool coil_driver_tick(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized || !ctx->running) {
        return false;
    }
    
    uint32_t now = xTaskGetTickCount();
    uint32_t elapsed_ms = (now - ctx->state_start_tick) * portTICK_PERIOD_MS;
    
    switch (ctx->state) {
        case COIL_STATE_OFF:
            /* Not running */
            return false;
            
        case COIL_STATE_SOFT_START:
            /* Soft-start is blocking, shouldn't get here */
            return false;
            
        case COIL_STATE_MEASURING:
            /* Check if measurement window is complete */
            if (elapsed_ms >= ctx->on_time_ms) {
                if (ctx->off_time_ms == 0) {
                    /* Continuous mode - stay measuring */
                    return true;
                }
                
                /* Transition to sleep */
                coil_driver_stop(ctx);
                ctx->state = COIL_STATE_SLEEPING;
                ctx->state_start_tick = now;
                ctx->measurement_count++;
                
                SEGGER_RTT_printf(0, "COIL: Measurement complete (#%lu), sleeping %lums\n",
                                  ctx->measurement_count, ctx->off_time_ms);
                return false;
            }
            return true;
            
        case COIL_STATE_SLEEPING:
            /* Check if sleep period is complete */
            if (elapsed_ms >= ctx->off_time_ms) {
                /* Start new measurement cycle with soft-start */
                SEGGER_RTT_printf(0, "COIL: Sleep complete, starting measurement\n");
                coil_driver_soft_start(ctx);
                return true;
            }
            return false;
    }
    
    return false;
}

bool coil_driver_is_measuring(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    return (ctx->state == COIL_STATE_MEASURING);
}

/* ==========================================================================
 * PWM CURRENT CONTROL (Open-Loop)
 * ========================================================================== */

/**
 * @brief Calculate PWM duty cycle from electrical parameters
 * 
 * Open-loop formula: PWM_duty = (I_target * R_coil) / V_supply
 * 
 * Example for MM-S 2" pipe:
 *   I_target = 1000 mA
 *   R_coil = 4300 mΩ (4.3Ω)
 *   V_supply = 24000 mV (24V)
 *   PWM_duty = (1000 * 4300) / 24000 = 179 (17.9%)
 */
static void calculate_pwm_duty(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL || ctx->supply_voltage_mv == 0) {
        return;
    }
    
    /* Calculate required voltage: V = I * R (in mV) */
    uint32_t required_mv = (ctx->target_current_ma * ctx->coil_resistance_mo) / 1000;
    
    /* Calculate duty cycle (0-1000 = 0-100%) */
    uint32_t duty = (required_mv * 1000) / ctx->supply_voltage_mv;
    
    /* Clamp to valid range */
    if (duty > 1000) duty = 1000;
    
    ctx->pwm_duty = (uint16_t)duty;
    
    SEGGER_RTT_printf(0, "COIL: PWM calculated: I=%lumA, R=%lumΩ, V=%lumV -> duty=%u.%u%%\n",
                      ctx->target_current_ma, ctx->coil_resistance_mo,
                      ctx->supply_voltage_mv, duty / 10, duty % 10);
}

void coil_driver_set_target_current(coil_driver_ctx_t *ctx, uint32_t target_ma)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->target_current_ma = target_ma;
    calculate_pwm_duty(ctx);
}

void coil_driver_set_electrical_params(coil_driver_ctx_t *ctx,
                                        uint32_t supply_mv,
                                        uint32_t resistance_mo)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->supply_voltage_mv = supply_mv;
    ctx->coil_resistance_mo = resistance_mo;
    calculate_pwm_duty(ctx);
    
    SEGGER_RTT_printf(0, "COIL: Electrical params set: V=%lumV, R=%lumΩ\n",
                      supply_mv, resistance_mo);
}

void coil_driver_update_isense(coil_driver_ctx_t *ctx, uint16_t isense_mv)
{
    if (ctx == NULL) {
        return;
    }
    
    ctx->last_isense_mv = isense_mv;
    
    /* Fault detection thresholds */
    /* Expected I_SENSE voltage based on target current and sense resistor */
    /* MM-S: 0.1Ω shunt, 1A -> 100mV expected */
    /* MM-M/L: 0.02Ω shunt + 20x amp, 2.5A -> 1000mV expected */
    
    /* For now, simple fault detection: */
    /* - Open coil: I_SENSE near zero when PWM > 0 */
    /* - Short circuit: I_SENSE much higher than expected */
    
    bool was_fault = ctx->coil_fault;
    
    if (ctx->running && ctx->pwm_duty > 100) {  /* PWM > 10% */
        if (isense_mv < 10) {
            /* No current flowing - open coil */
            ctx->coil_fault = true;
            if (!was_fault) {
                SEGGER_RTT_printf(0, "COIL: FAULT - Open circuit detected (I_SENSE=%umV)\n", isense_mv);
            }
        } else if (isense_mv > 3000) {
            /* Excessive current - possible short */
            ctx->coil_fault = true;
            if (!was_fault) {
                SEGGER_RTT_printf(0, "COIL: FAULT - Overcurrent detected (I_SENSE=%umV)\n", isense_mv);
            }
        } else {
            ctx->coil_fault = false;
        }
    }
}

bool coil_driver_has_fault(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    return ctx->coil_fault;
}

uint16_t coil_driver_get_pwm_duty(coil_driver_ctx_t *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    return ctx->pwm_duty;
}
