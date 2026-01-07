/**
 * @file moisture_probe.cpp
 * @brief Oscillator-based Soil Moisture Probe Implementation
 * 
 * Uses nRF52832 Timer + GPIOTE + PPI for hardware frequency counting.
 * Zero CPU overhead during measurement - all done in hardware.
 */

#include <Arduino.h>
#include <nrf_timer.h>
#include <nrf_gpiote.h>
#include <nrf_ppi.h>
#include "config.h"
#include "moisture_probe.h"
#include "moisture_cal.h"

// Use TIMER3 for frequency counting (TIMER0=SoftDevice, TIMER1=Arduino, TIMER2=available)
#define FREQ_TIMER          NRF_TIMER3
#define FREQ_TIMER_IRQn     TIMER3_IRQn

// Use TIMER4 for measurement gate timing
#define GATE_TIMER          NRF_TIMER4
#define GATE_TIMER_IRQn     TIMER4_IRQn

// GPIOTE channel for frequency input (we'll reconfigure per probe)
#define GPIOTE_CH_FREQ      2

// PPI channels
#define PPI_CH_COUNT        10  // Frequency edge -> increment counter
#define PPI_CH_STOP         11  // Gate timer -> stop counter

// Probe pin mapping
static const uint8_t s_probePins[MAX_PROBES] = {
    PIN_PROBE_1_FREQ,
    PIN_PROBE_2_FREQ,
    PIN_PROBE_3_FREQ,
    PIN_PROBE_4_FREQ
};

// Module state
static bool s_initialized = false;
static bool s_powered = false;
static volatile bool s_measurementComplete = false;

/**
 * @brief Initialize moisture probe hardware
 */
void moistureProbe_init(void) {
    if (s_initialized) return;
    
    // Configure probe power pin (P-FET gate, active LOW)
    pinMode(PIN_PROBE_POWER, OUTPUT);
    #if PROBE_POWER_ACTIVE_LOW
    digitalWrite(PIN_PROBE_POWER, HIGH);  // Power off (P-FET off)
    #else
    digitalWrite(PIN_PROBE_POWER, LOW);   // Power off
    #endif
    
    // Configure probe frequency input pins
    for (uint8_t i = 0; i < NUM_MOISTURE_PROBES; i++) {
        pinMode(s_probePins[i], INPUT);
    }
    
    // Configure TIMER3 as counter (counts GPIOTE events)
    FREQ_TIMER->MODE = TIMER_MODE_MODE_Counter;
    FREQ_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    FREQ_TIMER->TASKS_CLEAR = 1;
    
    // Configure TIMER4 as gate timer (measures time window)
    GATE_TIMER->MODE = TIMER_MODE_MODE_Timer;
    GATE_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    GATE_TIMER->PRESCALER = 4;  // 16MHz / 16 = 1MHz (1µs resolution)
    
    s_initialized = true;
    s_powered = false;
    
    DEBUG_PRINTLN("MoistureProbe: Initialized");
}

/**
 * @brief Power on all probes
 */
void moistureProbe_powerOn(void) {
    if (!s_initialized) return;
    if (s_powered) return;
    
    #if PROBE_POWER_ACTIVE_LOW
    digitalWrite(PIN_PROBE_POWER, LOW);   // P-FET on
    #else
    digitalWrite(PIN_PROBE_POWER, HIGH);  // P-FET on
    #endif
    
    s_powered = true;
    
    // Wait for oscillators to stabilize
    delay(PROBE_STABILIZE_MS);
    
    DEBUG_PRINTLN("MoistureProbe: Power ON");
}

/**
 * @brief Power off all probes
 */
void moistureProbe_powerOff(void) {
    if (!s_initialized) return;
    
    #if PROBE_POWER_ACTIVE_LOW
    digitalWrite(PIN_PROBE_POWER, HIGH);  // P-FET off
    #else
    digitalWrite(PIN_PROBE_POWER, LOW);   // P-FET off
    #endif
    
    s_powered = false;
    
    DEBUG_PRINTLN("MoistureProbe: Power OFF");
}

/**
 * @brief Check if probes are powered
 */
bool moistureProbe_isPowered(void) {
    return s_powered;
}

/**
 * @brief Configure GPIOTE to count edges on specified pin
 */
static void configureGpioteForPin(uint8_t pin) {
    // Configure GPIOTE channel to generate event on rising edge
    NRF_GPIOTE->CONFIG[GPIOTE_CH_FREQ] = 
        (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
        (pin << GPIOTE_CONFIG_PSEL_Pos) |
        (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);
}

/**
 * @brief Measure frequency from a single probe using hardware counting
 */
uint32_t moistureProbe_measureFrequency(uint8_t probeIndex, uint32_t measurementMs) {
    if (!s_initialized || probeIndex >= NUM_MOISTURE_PROBES) {
        return 0;
    }
    
    uint8_t pin = s_probePins[probeIndex];
    
    // Configure GPIOTE for this probe's pin
    configureGpioteForPin(pin);
    
    // Set up PPI: GPIOTE event -> Timer3 COUNT task
    NRF_PPI->CH[PPI_CH_COUNT].EEP = (uint32_t)&NRF_GPIOTE->EVENTS_IN[GPIOTE_CH_FREQ];
    NRF_PPI->CH[PPI_CH_COUNT].TEP = (uint32_t)&FREQ_TIMER->TASKS_COUNT;
    
    // Set up gate timer compare value (measurement duration in µs)
    uint32_t gateTimeUs = measurementMs * 1000;
    GATE_TIMER->CC[0] = gateTimeUs;
    
    // Clear both timers
    FREQ_TIMER->TASKS_CLEAR = 1;
    GATE_TIMER->TASKS_CLEAR = 1;
    
    // Clear any pending events
    NRF_GPIOTE->EVENTS_IN[GPIOTE_CH_FREQ] = 0;
    GATE_TIMER->EVENTS_COMPARE[0] = 0;
    
    // Enable PPI channel for counting
    NRF_PPI->CHENSET = (1 << PPI_CH_COUNT);
    
    // Start both timers
    FREQ_TIMER->TASKS_START = 1;
    GATE_TIMER->TASKS_START = 1;
    
    // Wait for gate timer to expire
    while (GATE_TIMER->EVENTS_COMPARE[0] == 0) {
        // Could use interrupt + sleep here for lower power
    }
    
    // Stop timers
    FREQ_TIMER->TASKS_STOP = 1;
    GATE_TIMER->TASKS_STOP = 1;
    
    // Disable PPI
    NRF_PPI->CHENCLR = (1 << PPI_CH_COUNT);
    
    // Disable GPIOTE channel
    NRF_GPIOTE->CONFIG[GPIOTE_CH_FREQ] = 0;
    
    // Capture counter value
    FREQ_TIMER->TASKS_CAPTURE[0] = 1;
    uint32_t edgeCount = FREQ_TIMER->CC[0];
    
    // Calculate frequency: edges / time(seconds)
    // frequency = edgeCount / (measurementMs / 1000) = edgeCount * 1000 / measurementMs
    uint32_t frequency = (edgeCount * 1000) / measurementMs;
    
    DEBUG_PRINTF("MoistureProbe: Probe %d, edges=%lu, freq=%lu Hz\n", 
                 probeIndex, edgeCount, frequency);
    
    return frequency;
}

/**
 * @brief Check if a frequency is within valid range
 */
ProbeStatus moistureProbe_validateFrequency(uint32_t frequency) {
    if (frequency < FREQ_MIN_VALID_HZ) {
        return PROBE_SHORTED;
    }
    if (frequency > FREQ_MAX_VALID_HZ) {
        return PROBE_DISCONNECTED;
    }
    return PROBE_OK;
}

/**
 * @brief Convert frequency to moisture percentage
 */
uint8_t moistureProbe_frequencyToPercent(uint8_t probeIndex, uint32_t frequency) {
    MoistureCalibration cal;
    
    if (!moistureCal_get(probeIndex, &cal)) {
        return 255;  // Not calibrated
    }
    
    // Check if field calibration is complete
    if (cal.f_dry == 0 || cal.f_wet == 0) {
        return 255;  // Not field calibrated
    }
    
    // Moisture % = 100 × (f_dry - f_measured) / (f_dry - f_wet)
    // Higher frequency = drier soil
    // Lower frequency = wetter soil
    
    if (frequency >= cal.f_dry) {
        return 0;  // At or below dry point
    }
    if (frequency <= cal.f_wet) {
        return 100;  // At or above wet point
    }
    
    uint32_t range = cal.f_dry - cal.f_wet;
    uint32_t offset = cal.f_dry - frequency;
    
    return (uint8_t)((offset * 100) / range);
}

/**
 * @brief Read a single probe with status
 */
bool moistureProbe_readSingle(uint8_t probeIndex, ProbeReading* reading) {
    if (!s_initialized || probeIndex >= NUM_MOISTURE_PROBES || reading == NULL) {
        return false;
    }
    
    reading->probeIndex = probeIndex;
    reading->frequency = 0;
    reading->moisturePercent = 255;
    reading->status = PROBE_ERROR;
    
    // Power on if needed
    bool wasPowered = s_powered;
    if (!s_powered) {
        moistureProbe_powerOn();
    }
    
    // Measure frequency
    reading->frequency = moistureProbe_measureFrequency(probeIndex, PROBE_MEASUREMENT_MS);
    
    // Validate frequency
    reading->status = moistureProbe_validateFrequency(reading->frequency);
    
    // Calculate moisture if valid
    if (reading->status == PROBE_OK) {
        reading->moisturePercent = moistureProbe_frequencyToPercent(probeIndex, reading->frequency);
        if (reading->moisturePercent == 255) {
            reading->status = PROBE_NOT_CALIBRATED;
        }
    }
    
    // Power off if we powered on
    if (!wasPowered) {
        moistureProbe_powerOff();
    }
    
    return (reading->status == PROBE_OK);
}

/**
 * @brief Read all probes sequentially
 */
uint8_t moistureProbe_readAll(MoistureReading* reading) {
    if (!s_initialized || reading == NULL) {
        return 0;
    }
    
    reading->numProbes = 0;
    reading->timestamp = millis();
    
    // Power on probes
    moistureProbe_powerOn();
    
    // Read each probe
    uint8_t successCount = 0;
    for (uint8_t i = 0; i < NUM_MOISTURE_PROBES; i++) {
        ProbeReading* probe = &reading->probes[i];
        probe->probeIndex = i;
        
        // Measure frequency
        probe->frequency = moistureProbe_measureFrequency(i, PROBE_MEASUREMENT_MS);
        
        // Validate
        probe->status = moistureProbe_validateFrequency(probe->frequency);
        
        // Calculate moisture
        if (probe->status == PROBE_OK) {
            probe->moisturePercent = moistureProbe_frequencyToPercent(i, probe->frequency);
            if (probe->moisturePercent == 255) {
                probe->status = PROBE_NOT_CALIBRATED;
            } else {
                successCount++;
            }
        } else {
            probe->moisturePercent = 255;
        }
        
        reading->numProbes++;
    }
    
    // Power off probes
    moistureProbe_powerOff();
    
    return successCount;
}

/**
 * @brief Get the GPIO pin for a probe
 */
uint8_t moistureProbe_getPin(uint8_t probeIndex) {
    if (probeIndex >= MAX_PROBES) {
        return 0xFF;
    }
    return s_probePins[probeIndex];
}
