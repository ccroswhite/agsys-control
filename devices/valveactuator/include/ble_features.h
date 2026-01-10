/**
 * @file ble_features.h
 * @brief BLE Feature Flags for Valve Actuator
 * 
 * The valve actuator is a minimal device (nRF52810) that can ONLY be
 * updated via BLE DFU. It has limited features compared to other devices.
 */

#ifndef BLE_FEATURES_H
#define BLE_FEATURES_H

// Device Configuration
#define AGSYS_BLE_DEVICE_NAME       "AgSys Actuator"
#define AGSYS_BLE_FRAM_PIN_ADDR     0x0010  // FRAM address for PIN storage

// Feature Flags - Minimal set for actuator
#define AGSYS_BLE_FEATURE_AUTH          1   // PIN required for DFU
#define AGSYS_BLE_FEATURE_DEVICE_INFO   1   // Device identification
#define AGSYS_BLE_FEATURE_SETTINGS      0
#define AGSYS_BLE_FEATURE_LIVE_DATA     0
#define AGSYS_BLE_FEATURE_VALVE         0   // Valve control is via CAN, not BLE
#define AGSYS_BLE_FEATURE_CAN_DISCOVERY 0
#define AGSYS_BLE_FEATURE_CALIBRATION   0
#define AGSYS_BLE_FEATURE_DIAGNOSTICS   1   // Basic diagnostics
#define AGSYS_BLE_FEATURE_DFU           1   // Critical - only way to update firmware

#endif // BLE_FEATURES_H
