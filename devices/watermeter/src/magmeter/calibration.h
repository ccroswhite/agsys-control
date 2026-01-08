/**
 * @file calibration.h
 * @brief Calibration manager for Mag Meter
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "ui_types.h"

// Initialize calibration (loads from FRAM)
void calibration_init(void);

// Get pointer to calibration data
CalibrationData_t* calibration_get(void);

// Capture zero offset (call with no flow)
void calibration_captureZero(void);

// Set span factor
void calibration_setSpan(float span);

// Save calibration to FRAM
void calibration_save(void);

// Reset calibration to defaults
void calibration_reset(void);

// Apply calibration to raw ADC value
int32_t calibration_applyZero(int32_t rawValue);
float calibration_applySpan(float value);

#endif // CALIBRATION_H
