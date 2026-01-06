/**
 * @file capacitance.cpp
 * @brief AC Capacitance Soil Moisture Measurement
 * 
 * Uses a discrete MOSFET H-bridge to generate 100kHz bipolar AC signal
 * for capacitive soil moisture sensing. True AC prevents soil polarization
 * enabling 10+ year probe life.
 * 
 * Hardware: 2× SSM6P15FU (P-ch) + 2× 2SK2009 (N-ch) H-bridge
 * Drive: nRF52832 Timer + PPI + GPIOTE (hardware, zero CPU)
 * Measurement: Envelope detector → ADC with 1-second averaging
 */

#include <Arduino.h>
#include <nrf_timer.h>
#include <nrf_ppi.h>
#include <nrf_gpiote.h>
#include "config.h"
#include "capacitance.h"

// Use TIMER2 for H-bridge drive (TIMER0 used by SoftDevice, TIMER1 by Arduino)
#define HBRIDGE_TIMER       NRF_TIMER2
#define HBRIDGE_TIMER_IRQn  TIMER2_IRQn

// GPIOTE channels for H-bridge outputs
#define GPIOTE_CH_A         0
#define GPIOTE_CH_B         1

// PPI channels
#define PPI_CH_TOGGLE       0

// Module state
static bool hbridgeRunning = false;
static volatile uint32_t adcSampleCount = 0;
static volatile uint64_t adcSampleSum = 0;

/**
 * @brief Initialize H-bridge hardware (GPIO, GPIOTE, Timer, PPI)
 */
void capacitanceInit() {
    // Configure H-bridge GPIO pins as outputs, initially LOW (H-bridge off)
    pinMode(PIN_HBRIDGE_A, OUTPUT);
    pinMode(PIN_HBRIDGE_B, OUTPUT);
    digitalWrite(PIN_HBRIDGE_A, LOW);
    digitalWrite(PIN_HBRIDGE_B, LOW);
    
    // Configure power enable pin if used
    pinMode(PIN_MOISTURE_POWER, OUTPUT);
    digitalWrite(PIN_MOISTURE_POWER, LOW);
    
    // Configure ADC input
    pinMode(PIN_MOISTURE_ADC, INPUT);
    
    // Set up GPIOTE for hardware GPIO toggling
    // Channel A: Toggle PIN_HBRIDGE_A
    NRF_GPIOTE->CONFIG[GPIOTE_CH_A] = 
        (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
        (PIN_HBRIDGE_A << GPIOTE_CONFIG_PSEL_Pos) |
        (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
        (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);
    
    // Channel B: Toggle PIN_HBRIDGE_B
    NRF_GPIOTE->CONFIG[GPIOTE_CH_B] = 
        (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
        (PIN_HBRIDGE_B << GPIOTE_CONFIG_PSEL_Pos) |
        (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
        (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);  // Start opposite to A
    
    // Configure Timer2 for 100kHz toggle rate
    // 16MHz base clock, need to toggle every 5µs for 100kHz full cycle (10µs period)
    // Actually: 100kHz = 10µs period, so toggle every 5µs = 80 ticks at 16MHz
    HBRIDGE_TIMER->MODE = TIMER_MODE_MODE_Timer;
    HBRIDGE_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
    HBRIDGE_TIMER->PRESCALER = 0;  // 16 MHz
    HBRIDGE_TIMER->CC[0] = 80;     // 16MHz / 80 = 200kHz toggle = 100kHz full cycle
    HBRIDGE_TIMER->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Msk;
    
    // Set up PPI to connect Timer compare event to GPIOTE toggle tasks
    // Use PPI channel 0 with fork to toggle both GPIOs simultaneously
    NRF_PPI->CH[PPI_CH_TOGGLE].EEP = (uint32_t)&HBRIDGE_TIMER->EVENTS_COMPARE[0];
    NRF_PPI->CH[PPI_CH_TOGGLE].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[GPIOTE_CH_A];
    NRF_PPI->FORK[PPI_CH_TOGGLE].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[GPIOTE_CH_B];
    
    hbridgeRunning = false;
}

/**
 * @brief Start H-bridge 100kHz AC drive
 */
void hbridgeStart() {
    if (hbridgeRunning) return;
    
    // Enable power to H-bridge circuit (if using power gating)
    digitalWrite(PIN_MOISTURE_POWER, HIGH);
    
    // Small delay for power to stabilize
    delayMicroseconds(100);
    
    // Set initial GPIO states: A=LOW, B=HIGH for proper complementary drive
    digitalWrite(PIN_HBRIDGE_A, LOW);
    digitalWrite(PIN_HBRIDGE_B, HIGH);
    
    // Re-initialize GPIOTE with correct initial states
    NRF_GPIOTE->CONFIG[GPIOTE_CH_A] = 
        (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
        (PIN_HBRIDGE_A << GPIOTE_CONFIG_PSEL_Pos) |
        (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
        (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);
    
    NRF_GPIOTE->CONFIG[GPIOTE_CH_B] = 
        (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) |
        (PIN_HBRIDGE_B << GPIOTE_CONFIG_PSEL_Pos) |
        (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
        (GPIOTE_CONFIG_OUTINIT_High << GPIOTE_CONFIG_OUTINIT_Pos);
    
    // Enable PPI channel
    NRF_PPI->CHENSET = (1 << PPI_CH_TOGGLE);
    
    // Clear and start timer
    HBRIDGE_TIMER->TASKS_CLEAR = 1;
    HBRIDGE_TIMER->TASKS_START = 1;
    
    hbridgeRunning = true;
}

/**
 * @brief Stop H-bridge AC drive
 */
void hbridgeStop() {
    if (!hbridgeRunning) return;
    
    // Stop timer
    HBRIDGE_TIMER->TASKS_STOP = 1;
    
    // Disable PPI channel
    NRF_PPI->CHENCLR = (1 << PPI_CH_TOGGLE);
    
    // Disable GPIOTE channels
    NRF_GPIOTE->CONFIG[GPIOTE_CH_A] = 0;
    NRF_GPIOTE->CONFIG[GPIOTE_CH_B] = 0;
    
    // Set both GPIOs LOW (H-bridge off, no current flow)
    digitalWrite(PIN_HBRIDGE_A, LOW);
    digitalWrite(PIN_HBRIDGE_B, LOW);
    
    // Disable power to H-bridge
    digitalWrite(PIN_MOISTURE_POWER, LOW);
    
    hbridgeRunning = false;
}

/**
 * @brief Read envelope detector with high-fidelity averaging
 * 
 * Takes multiple ADC samples over the measurement period and returns
 * the average value for high accuracy and noise rejection.
 * 
 * @param durationMs Measurement duration in milliseconds
 * @param numSamples Number of ADC samples to take
 * @return Average ADC value (0-4095 for 12-bit)
 */
uint16_t readEnvelopeAverage(uint32_t durationMs, uint32_t numSamples) {
    if (numSamples == 0) numSamples = 1;
    
    uint64_t sum = 0;
    uint32_t sampleInterval = (durationMs * 1000) / numSamples;  // Interval in microseconds
    
    // Ensure minimum interval for ADC conversion time (~10µs on nRF52)
    if (sampleInterval < 20) sampleInterval = 20;
    
    uint32_t actualSamples = 0;
    uint32_t startTime = micros();
    uint32_t endTime = startTime + (durationMs * 1000);
    
    while (micros() < endTime && actualSamples < numSamples) {
        sum += analogRead(PIN_MOISTURE_ADC);
        actualSamples++;
        
        // Wait for next sample interval
        if (actualSamples < numSamples) {
            delayMicroseconds(sampleInterval - 10);  // Account for ADC time
        }
    }
    
    if (actualSamples == 0) return 0;
    
    return (uint16_t)(sum / actualSamples);
}

/**
 * @brief Perform complete capacitance measurement
 * 
 * Starts H-bridge, waits for settle time, takes averaged ADC readings
 * over 1 second, then stops H-bridge.
 * 
 * @return Raw ADC value representing probe capacitance (higher = more moisture)
 */
uint16_t readCapacitance() {
    // Start H-bridge AC drive
    hbridgeStart();
    
    // Wait for envelope detector to settle
    delay(SENSOR_STABILIZE_MS);
    
    // Take averaged ADC readings over measurement period
    uint16_t rawValue = readEnvelopeAverage(
        MOISTURE_MEASUREMENT_MS, 
        ADC_SAMPLES_PER_MEASUREMENT
    );
    
    // Stop H-bridge
    hbridgeStop();
    
    return rawValue;
}

/**
 * @brief Convert raw capacitance reading to moisture percentage
 * 
 * Uses linear interpolation between dry and wet calibration values.
 * 
 * @param raw Raw ADC value from readCapacitance()
 * @return Moisture percentage (0-100)
 */
uint8_t capacitanceToMoisturePercent(uint16_t raw) {
    // Higher capacitance = higher ADC value = more moisture
    // (opposite of some resistive sensors)
    
    if (raw <= MOISTURE_DRY_VALUE) {
        return 0;
    }
    if (raw >= MOISTURE_WET_VALUE) {
        return 100;
    }
    
    // Linear interpolation
    uint32_t range = MOISTURE_WET_VALUE - MOISTURE_DRY_VALUE;
    uint32_t offset = raw - MOISTURE_DRY_VALUE;
    
    return (uint8_t)((offset * 100) / range);
}

/**
 * @brief Check if H-bridge is currently running
 * @return true if AC drive is active
 */
bool isHbridgeRunning() {
    return hbridgeRunning;
}
