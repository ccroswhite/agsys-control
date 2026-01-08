/**
 * @file calibration.cpp
 * @brief Calibration manager implementation - FRAM persistence
 */

#include "calibration.h"
#include "magmeter_config.h"
#include <Adafruit_FRAM_SPI.h>

// External FRAM instance (defined in main.cpp)
extern Adafruit_FRAM_SPI fram;

// External ADC reading function (defined in main.cpp)
extern int32_t adc_getLastElectrodeReading(void);

// Current calibration data
static CalibrationData_t calData;

// FRAM address for calibration
#define FRAM_CAL_ADDR (FRAM_ADDR_CONFIG + sizeof(UserSettings_t) + 16)

// Calculate checksum
static uint32_t calculateChecksum(CalibrationData_t* data) {
    uint32_t sum = 0;
    uint8_t* bytes = (uint8_t*)data;
    for (size_t i = 0; i < sizeof(CalibrationData_t) - sizeof(uint32_t); i++) {
        sum += bytes[i];
    }
    return sum;
}

void calibration_init(void) {
    // Try to load from FRAM
    fram.read(FRAM_CAL_ADDR, (uint8_t*)&calData, sizeof(CalibrationData_t));
    
    // Validate checksum
    uint32_t expectedChecksum = calculateChecksum(&calData);
    
    if (calData.checksum != expectedChecksum) {
        DEBUG_PRINTLN("Calibration checksum invalid, using defaults");
        calibration_reset();
    } else {
        DEBUG_PRINTLN("Calibration loaded from FRAM");
        DEBUG_PRINTF("  Zero offset: %ld\n", calData.zeroOffset);
        DEBUG_PRINTF("  Span factor: %.3f\n", calData.spanFactor);
        DEBUG_PRINTF("  K factor: %.6f\n", calData.kFactor);
    }
}

CalibrationData_t* calibration_get(void) {
    return &calData;
}

void calibration_captureZero(void) {
    // Average multiple readings for stability
    int64_t sum = 0;
    const int numSamples = 100;
    
    for (int i = 0; i < numSamples; i++) {
        sum += adc_getLastElectrodeReading();
        delay(10);
    }
    
    calData.zeroOffset = (int32_t)(sum / numSamples);
    calData.calDate = millis() / 1000;  // Approximate - would use RTC in production
    
    calibration_save();
    
    DEBUG_PRINTF("Zero offset captured: %ld\n", calData.zeroOffset);
}

void calibration_setSpan(float span) {
    if (span < 0.5f) span = 0.5f;
    if (span > 2.0f) span = 2.0f;
    
    calData.spanFactor = span;
    calData.calDate = millis() / 1000;
    
    calibration_save();
    
    DEBUG_PRINTF("Span factor set: %.3f\n", calData.spanFactor);
}

void calibration_save(void) {
    calData.checksum = calculateChecksum(&calData);
    fram.write(FRAM_CAL_ADDR, (uint8_t*)&calData, sizeof(CalibrationData_t));
    DEBUG_PRINTLN("Calibration saved to FRAM");
}

void calibration_reset(void) {
    calData.zeroOffset = 0;
    calData.spanFactor = 1.0f;
    calData.kFactor = 1.0f;  // Default K factor - needs field calibration
    calData.calDate = 0;
    calData.checksum = calculateChecksum(&calData);
    
    DEBUG_PRINTLN("Calibration reset to defaults");
}

int32_t calibration_applyZero(int32_t rawValue) {
    return rawValue - calData.zeroOffset;
}

float calibration_applySpan(float value) {
    return value * calData.spanFactor;
}
