/**
 * @file ble_service.h
 * @brief BLE service for Mag Meter mobile app configuration
 * 
 * Provides BLE characteristics for:
 * - Initial configuration (SECRET_SALT, device registration)
 * - Settings (units, periods, max flow)
 * - Live data (flow rate, volume, trend, avg)
 * - Calibration (zero capture, span factor)
 * - Diagnostics (ADC values, status)
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <Arduino.h>
#include "ui_types.h"

// BLE Service UUIDs (custom 128-bit UUIDs)
// Base: AGSYS-xxxx-4167-5379-732D4D616700
#define BLE_UUID_MAGMETER_SERVICE       "AGSYS001-4167-5379-732D-4D6167000000"
#define BLE_UUID_DEVICE_INFO            "AGSYS002-4167-5379-732D-4D6167000000"
#define BLE_UUID_SECRET_SALT            "AGSYS003-4167-5379-732D-4D6167000000"
#define BLE_UUID_SETTINGS               "AGSYS004-4167-5379-732D-4D6167000000"
#define BLE_UUID_LIVE_DATA              "AGSYS005-4167-5379-732D-4D6167000000"
#define BLE_UUID_CALIBRATION            "AGSYS006-4167-5379-732D-4D6167000000"
#define BLE_UUID_CAL_COMMAND            "AGSYS007-4167-5379-732D-4D6167000000"
#define BLE_UUID_DIAGNOSTICS            "AGSYS008-4167-5379-732D-4D6167000000"

// BLE connection state
typedef enum {
    BLE_STATE_IDLE = 0,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
    BLE_STATE_PAIRING
} BleState_t;

// Device info structure (read-only)
typedef struct __attribute__((packed)) {
    uint8_t deviceUid[8];
    uint8_t deviceType;
    uint8_t tier;
    uint8_t firmwareMajor;
    uint8_t firmwareMinor;
    uint8_t firmwarePatch;
    uint8_t reserved[3];
} BleDeviceInfo_t;

// Live data structure (notify)
typedef struct __attribute__((packed)) {
    float flowRate_LPM;
    float totalVolume_L;
    float trendVolume_L;
    float avgVolume_L;
    uint8_t flowDirection;  // 0=none, 1=forward, 2=reverse
    uint8_t statusFlags;
} BleLiveData_t;

// Settings structure (read/write)
typedef struct __attribute__((packed)) {
    uint8_t unitSystem;
    uint16_t trendPeriodMin;
    uint16_t avgPeriodMin;
    uint16_t maxFlowLPM;
    uint8_t backlightOn;
} BleSettings_t;

// Calibration data structure (read)
typedef struct __attribute__((packed)) {
    int32_t zeroOffset;
    float spanFactor;
    float kFactor;
    uint32_t calDate;
} BleCalibration_t;

// Calibration command (write)
typedef enum {
    CAL_CMD_CAPTURE_ZERO = 1,
    CAL_CMD_SET_SPAN = 2,
    CAL_CMD_RESET = 3
} CalCommand_t;

typedef struct __attribute__((packed)) {
    uint8_t command;
    float value;  // For SET_SPAN
} BleCalCommand_t;

// Diagnostics structure (read)
typedef struct __attribute__((packed)) {
    int32_t adcElectrode;
    int32_t adcCurrent;
    uint16_t coilFrequency;
    uint16_t coilCurrent_mA;
    uint8_t tier;
    uint8_t errorFlags;
} BleDiagnostics_t;

// Initialize BLE service
void ble_init(void);

// Start advertising
void ble_startAdvertising(void);

// Stop advertising
void ble_stopAdvertising(void);

// Check if connected
bool ble_isConnected(void);

// Get current state
BleState_t ble_getState(void);

// Update live data (call periodically when connected)
void ble_updateLiveData(float flowRate, float totalVol, float trend, float avg, bool reverse);

// Update diagnostics
void ble_updateDiagnostics(int32_t adcElec, int32_t adcCur, uint16_t coilFreq, uint16_t coilCur_mA);

// Process BLE events (call from main loop)
void ble_process(void);

// Set callbacks for settings/calibration changes
typedef void (*BleSettingsCallback_t)(BleSettings_t* settings);
typedef void (*BleCalCommandCallback_t)(BleCalCommand_t* cmd);
typedef void (*BleSaltCallback_t)(uint8_t* salt, size_t len);

void ble_setSettingsCallback(BleSettingsCallback_t callback);
void ble_setCalCommandCallback(BleCalCommandCallback_t callback);
void ble_setSaltCallback(BleSaltCallback_t callback);

#endif // BLE_SERVICE_H
