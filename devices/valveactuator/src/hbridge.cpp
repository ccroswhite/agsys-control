/**
 * @file hbridge.cpp
 * @brief H-Bridge motor control implementation
 * 
 * Discrete H-Bridge using:
 * - Q1, Q2: AO3401A P-channel MOSFETs (high-side)
 * - Q3, Q4: AO3400A N-channel MOSFETs (low-side)
 * - Current sensing via 0.1Ω shunt resistor
 */

#include "hbridge.h"

static MotorDirection currentDirection = MOTOR_STOP;
static uint16_t lastCurrentMA = 0;

void hbridge_init(void) {
    // Configure H-Bridge control pins as outputs
    pinMode(PIN_HBRIDGE_A, OUTPUT);
    pinMode(PIN_HBRIDGE_B, OUTPUT);
    pinMode(PIN_HBRIDGE_EN_A, OUTPUT);
    pinMode(PIN_HBRIDGE_EN_B, OUTPUT);
    
    // Start with motor stopped
    hbridge_stop();
    
    // Configure current sense as analog input
    pinMode(PIN_CURRENT_SENSE, INPUT);
    
    DEBUG_PRINTLN("H-Bridge: Initialized");
}

void hbridge_open(void) {
    // Open direction: Q1 (high-side A) + Q3 (low-side A enable)
    // Current flows: +24V -> Q1 -> Motor -> Q4 -> Shunt -> GND
    
    // First, ensure opposite side is off
    digitalWrite(PIN_HBRIDGE_B, LOW);
    digitalWrite(PIN_HBRIDGE_EN_B, LOW);
    
    // Small delay to prevent shoot-through
    delayMicroseconds(10);
    
    // Enable open direction
    // P-channel: LOW to turn ON (inverted logic)
    // N-channel: HIGH to turn ON
    digitalWrite(PIN_HBRIDGE_A, LOW);   // Turn ON Q1 (P-ch, active low)
    digitalWrite(PIN_HBRIDGE_EN_B, HIGH); // Turn ON Q4 (N-ch, active high)
    
    currentDirection = MOTOR_OPEN;
    DEBUG_PRINTLN("H-Bridge: OPENING");
}

void hbridge_close(void) {
    // Close direction: Q2 (high-side B) + Q4 (low-side B enable)
    // Current flows: +24V -> Q2 -> Motor -> Q3 -> Shunt -> GND
    
    // First, ensure opposite side is off
    digitalWrite(PIN_HBRIDGE_A, HIGH);  // Turn OFF Q1 (P-ch)
    digitalWrite(PIN_HBRIDGE_EN_B, LOW);
    
    // Small delay to prevent shoot-through
    delayMicroseconds(10);
    
    // Enable close direction
    digitalWrite(PIN_HBRIDGE_B, LOW);    // Turn ON Q2 (P-ch, active low)
    digitalWrite(PIN_HBRIDGE_EN_A, HIGH); // Turn ON Q3 (N-ch, active high)
    
    currentDirection = MOTOR_CLOSE;
    DEBUG_PRINTLN("H-Bridge: CLOSING");
}

void hbridge_stop(void) {
    // All MOSFETs OFF - motor coasts to stop
    digitalWrite(PIN_HBRIDGE_A, HIGH);   // Turn OFF Q1 (P-ch)
    digitalWrite(PIN_HBRIDGE_B, HIGH);   // Turn OFF Q2 (P-ch)
    digitalWrite(PIN_HBRIDGE_EN_A, LOW); // Turn OFF Q3 (N-ch)
    digitalWrite(PIN_HBRIDGE_EN_B, LOW); // Turn OFF Q4 (N-ch)
    
    currentDirection = MOTOR_STOP;
    DEBUG_PRINTLN("H-Bridge: STOPPED");
}

void hbridge_brake(void) {
    // Both low-side MOSFETs ON - motor brakes (short circuit)
    digitalWrite(PIN_HBRIDGE_A, HIGH);   // Turn OFF Q1 (P-ch)
    digitalWrite(PIN_HBRIDGE_B, HIGH);   // Turn OFF Q2 (P-ch)
    digitalWrite(PIN_HBRIDGE_EN_A, HIGH); // Turn ON Q3 (N-ch)
    digitalWrite(PIN_HBRIDGE_EN_B, HIGH); // Turn ON Q4 (N-ch)
    
    currentDirection = MOTOR_BRAKE;
    DEBUG_PRINTLN("H-Bridge: BRAKE");
}

MotorDirection hbridge_get_direction(void) {
    return currentDirection;
}

uint16_t hbridge_read_current_ma(void) {
    // Read ADC value
    // nRF52810 has 10-bit ADC (0-1023) with 3.3V reference
    int adcValue = analogRead(PIN_CURRENT_SENSE);
    
    // Convert to voltage
    // ADC = (Vin / Vref) * 1023
    // Vin = (ADC / 1023) * 3.3V
    float voltage = (adcValue / 1023.0f) * 3.3f;
    
    // Convert to current using shunt resistor
    // V = I * R, so I = V / R
    // With 0.1Ω shunt: 1A = 100mV, 2A = 200mV, 3A = 300mV
    float currentA = voltage / CURRENT_SENSE_RESISTOR;
    
    lastCurrentMA = (uint16_t)(currentA * 1000.0f);
    return lastCurrentMA;
}

bool hbridge_is_overcurrent(void) {
    return (lastCurrentMA > CURRENT_OVERCURRENT_MA);
}

bool hbridge_is_stalled(void) {
    // Stall detection: high current for extended period
    // This is a simplified check - real implementation would
    // track current over time
    return (lastCurrentMA > CURRENT_STALL_MA);
}
