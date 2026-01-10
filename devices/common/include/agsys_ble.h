/**
 * @file agsys_ble.h
 * @brief Unified BLE Service for AgSys IoT Devices
 * 
 * Provides a common BLE service with feature-based characteristics.
 * Each device enables only the features it needs via ble_features.h.
 * 
 * Features:
 * - 6-digit PIN authentication (all devices)
 * - Device identification
 * - Settings management
 * - Live data streaming
 * - Valve control (valve controller)
 * - CAN bus discovery (valve controller)
 * - Calibration (water meter, soil moisture)
 * - Diagnostics
 */

#ifndef AGSYS_BLE_H
#define AGSYS_BLE_H

#include <Arduino.h>
#include <bluefruit.h>

// Include device-specific feature flags
// Each device must provide this file in their include/ directory
#include "ble_features.h"

// ============================================================================
// UUID Definitions
// ============================================================================

// Base UUID: AGSYS-xxxx-4167-5379-732D-4D6167000000
#define AGSYS_BLE_UUID_BASE             "AGSYS%03X-4167-5379-732D-4D6167000000"

// Service UUID
#define AGSYS_BLE_UUID_SERVICE          "AGSYS001-4167-5379-732D-4D6167000000"

// Core characteristics (all devices)
#define AGSYS_BLE_UUID_DEVICE_INFO      "AGSYS010-4167-5379-732D-4D6167000000"
#define AGSYS_BLE_UUID_PIN_AUTH         "AGSYS011-4167-5379-732D-4D6167000000"
#define AGSYS_BLE_UUID_PIN_CHANGE       "AGSYS012-4167-5379-732D-4D6167000000"

// Settings (water meter, valve controller)
#define AGSYS_BLE_UUID_SETTINGS         "AGSYS020-4167-5379-732D-4D6167000000"
#define AGSYS_BLE_UUID_LIVE_DATA        "AGSYS021-4167-5379-732D-4D6167000000"

// Valve control (valve controller)
#define AGSYS_BLE_UUID_VALVE_CMD        "AGSYS030-4167-5379-732D-4D6167000000"
#define AGSYS_BLE_UUID_VALVE_STATUS     "AGSYS031-4167-5379-732D-4D6167000000"
#define AGSYS_BLE_UUID_CAN_DISCOVERY    "AGSYS032-4167-5379-732D-4D6167000000"
#define AGSYS_BLE_UUID_ACTUATOR_LIST    "AGSYS033-4167-5379-732D-4D6167000000"

// Calibration (water meter, soil moisture)
#define AGSYS_BLE_UUID_CALIBRATION      "AGSYS040-4167-5379-732D-4D6167000000"
#define AGSYS_BLE_UUID_CAL_COMMAND      "AGSYS041-4167-5379-732D-4D6167000000"

// Diagnostics (all devices)
#define AGSYS_BLE_UUID_DIAGNOSTICS      "AGSYS050-4167-5379-732D-4D6167000000"
#define AGSYS_BLE_UUID_DEBUG_LOG        "AGSYS051-4167-5379-732D-4D6167000000"

// DFU uses Nordic's standard DFU service (handled by BLEDfu class)

// ============================================================================
// Constants
// ============================================================================

// PIN Configuration
#define AGSYS_PIN_LENGTH                6
#define AGSYS_PIN_MAX_ATTEMPTS          3
#define AGSYS_PIN_LOCKOUT_MS            300000      // 5 minutes
#define AGSYS_AUTH_TIMEOUT_MS           300000      // 5 minutes
#define AGSYS_DEFAULT_PIN               "123456"

// Authentication status
#define AGSYS_AUTH_NOT_AUTHENTICATED    0x00
#define AGSYS_AUTH_AUTHENTICATED        0x01
#define AGSYS_AUTH_FAILED               0x02
#define AGSYS_AUTH_LOCKED_OUT           0x03
#define AGSYS_AUTH_PIN_CHANGED          0x04

// Device types
#define AGSYS_DEVICE_TYPE_SOIL_MOISTURE 0x01
#define AGSYS_DEVICE_TYPE_VALVE_CTRL    0x02
#define AGSYS_DEVICE_TYPE_WATER_METER   0x03
#define AGSYS_DEVICE_TYPE_VALVE_ACTUATOR 0x04

// Feature flags (bitmask)
#define AGSYS_FEATURE_AUTH              (1 << 0)
#define AGSYS_FEATURE_SETTINGS          (1 << 1)
#define AGSYS_FEATURE_LIVE_DATA         (1 << 2)
#define AGSYS_FEATURE_VALVE             (1 << 3)
#define AGSYS_FEATURE_CAN_DISCOVERY     (1 << 4)
#define AGSYS_FEATURE_CALIBRATION       (1 << 5)
#define AGSYS_FEATURE_DIAGNOSTICS       (1 << 6)
#define AGSYS_FEATURE_DFU               (1 << 7)

// Valve commands - use values from agsys_protocol.h if included, otherwise define here
#ifndef AGSYS_VALVE_CMD_OPEN
#define AGSYS_VALVE_CMD_OPEN            0x01
#endif
#ifndef AGSYS_VALVE_CMD_CLOSE
#define AGSYS_VALVE_CMD_CLOSE           0x02
#endif
#ifndef AGSYS_VALVE_CMD_STOP
#define AGSYS_VALVE_CMD_STOP            0x03
#endif
#ifndef AGSYS_VALVE_CMD_QUERY
#define AGSYS_VALVE_CMD_QUERY           0x04
#endif
#ifndef AGSYS_VALVE_CMD_EMERGENCY_CLOSE
#define AGSYS_VALVE_CMD_EMERGENCY_CLOSE 0x0F
#endif

// Calibration commands
#define AGSYS_CAL_CMD_CAPTURE_ZERO      0x01
#define AGSYS_CAL_CMD_SET_SPAN          0x02
#define AGSYS_CAL_CMD_RESET             0x03
#define AGSYS_CAL_CMD_CAPTURE_AIR       0x11  // Soil moisture
#define AGSYS_CAL_CMD_CAPTURE_DRY       0x12
#define AGSYS_CAL_CMD_CAPTURE_WET       0x13

// Discovery status
#define AGSYS_DISCOVERY_IDLE            0x00
#define AGSYS_DISCOVERY_IN_PROGRESS     0x01
#define AGSYS_DISCOVERY_COMPLETE        0x02

// ============================================================================
// Data Structures
// ============================================================================

// Device info (14 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  uid[8];
    uint8_t  deviceType;
    uint8_t  fwMajor;
    uint8_t  fwMinor;
    uint8_t  fwPatch;
    uint16_t features;
} AgsysBleDeviceInfo_t;

// Settings - Water Meter (8 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  unitSystem;
    uint16_t trendPeriodMin;
    uint16_t avgPeriodMin;
    uint16_t maxFlowLPM;
    uint8_t  backlightOn;
} AgsysBleSettings_t;

// Live data - Water Meter (18 bytes)
typedef struct __attribute__((packed)) {
    float    flowRate;
    float    totalVolume;
    float    trendVolume;
    float    avgVolume;
    uint8_t  direction;
    uint8_t  flags;
} AgsysBleLiveData_t;

// Valve command (4 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  command;
    uint8_t  address;
    uint16_t durationSec;
} AgsysBleValveCmd_t;

// Valve status (5 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  address;
    uint8_t  state;
    uint16_t currentMa;
    uint8_t  flags;
} AgsysBleValveStatus_t;

// Actuator info (11 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  address;
    uint8_t  uid[8];
    uint8_t  state;
    uint8_t  flags;
} AgsysBleActuatorInfo_t;

// Calibration - Water Meter (16 bytes)
typedef struct __attribute__((packed)) {
    int32_t  zeroOffset;
    float    spanFactor;
    float    kFactor;
    uint32_t calDate;
} AgsysBleCalMeter_t;

// Calibration - Soil Moisture (13 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  probeIndex;
    uint32_t fAir;
    uint32_t fDry;
    uint32_t fWet;
} AgsysBleCalSoil_t;

// Calibration command (5 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  command;
    uint8_t  probeIndex;  // For soil moisture: which probe (0-3)
    float    value;
} AgsysBleCalCmd_t;

// Diagnostics (12 bytes)
typedef struct __attribute__((packed)) {
    uint32_t bootCount;
    uint32_t uptime;
    uint16_t batteryMv;
    uint8_t  errorCode;
    uint8_t  flags;
} AgsysBleDiagnostics_t;

// ============================================================================
// Callbacks
// ============================================================================

typedef void (*AgsysBleAuthCallback_t)(bool authenticated);
typedef void (*AgsysBleSettingsCallback_t)(AgsysBleSettings_t* settings);
typedef void (*AgsysBleValveCallback_t)(AgsysBleValveCmd_t* cmd);
typedef void (*AgsysBleCalCallback_t)(AgsysBleCalCmd_t* cmd);
typedef void (*AgsysBleDiscoveryCallback_t)(void);

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Initialize the unified BLE service
 * @param deviceName BLE device name for advertising
 * @param deviceType Device type (AGSYS_DEVICE_TYPE_*)
 * @param framPinAddr FRAM address for PIN storage (6 bytes)
 * @param fwMajor Firmware major version
 * @param fwMinor Firmware minor version
 * @param fwPatch Firmware patch version
 */
void agsys_ble_init(const char* deviceName, uint8_t deviceType, uint16_t framPinAddr,
                    uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch);

/**
 * @brief Start BLE advertising
 */
void agsys_ble_start_advertising(void);

/**
 * @brief Stop BLE advertising
 */
void agsys_ble_stop_advertising(void);

/**
 * @brief Check if BLE is connected
 */
bool agsys_ble_is_connected(void);

/**
 * @brief Check if current session is authenticated
 */
bool agsys_ble_is_authenticated(void);

/**
 * @brief Process BLE events (call from main loop)
 */
void agsys_ble_process(void);

/**
 * @brief Clear authentication (call on disconnect)
 */
void agsys_ble_clear_auth(void);

// Callback setters
void agsys_ble_set_auth_callback(AgsysBleAuthCallback_t callback);
void agsys_ble_set_settings_callback(AgsysBleSettingsCallback_t callback);
void agsys_ble_set_valve_callback(AgsysBleValveCallback_t callback);
void agsys_ble_set_cal_callback(AgsysBleCalCallback_t callback);
void agsys_ble_set_discovery_callback(AgsysBleDiscoveryCallback_t callback);

// Data update functions (for notify characteristics)
#if AGSYS_BLE_FEATURE_LIVE_DATA
void agsys_ble_update_live_data(AgsysBleLiveData_t* data);
#endif

#if AGSYS_BLE_FEATURE_VALVE
void agsys_ble_update_valve_status(AgsysBleValveStatus_t* status);
void agsys_ble_set_discovery_results(uint8_t count, AgsysBleActuatorInfo_t* actuators);
#endif

#if AGSYS_BLE_FEATURE_CALIBRATION
void agsys_ble_update_calibration_meter(AgsysBleCalMeter_t* cal);
void agsys_ble_update_calibration_soil(AgsysBleCalSoil_t* cal);
#endif

#if AGSYS_BLE_FEATURE_DIAGNOSTICS
void agsys_ble_update_diagnostics(AgsysBleDiagnostics_t* diag);
#endif

#endif // AGSYS_BLE_H
