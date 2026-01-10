/**
 * @file ble_features.h
 * @brief BLE Feature Flags for Water Meter (Mag Meter)
 */

#ifndef BLE_FEATURES_H
#define BLE_FEATURES_H

// Device Configuration
#define AGSYS_BLE_DEVICE_NAME       "AgSys MagMeter"
#define AGSYS_BLE_FRAM_PIN_ADDR     0x00F0

// Feature Flags
#define AGSYS_BLE_FEATURE_AUTH          1
#define AGSYS_BLE_FEATURE_DEVICE_INFO   1
#define AGSYS_BLE_FEATURE_SETTINGS      1
#define AGSYS_BLE_FEATURE_LIVE_DATA     1
#define AGSYS_BLE_FEATURE_VALVE         0
#define AGSYS_BLE_FEATURE_CAN_DISCOVERY 0
#define AGSYS_BLE_FEATURE_CALIBRATION   1
#define AGSYS_BLE_FEATURE_DIAGNOSTICS   1
#define AGSYS_BLE_FEATURE_DFU           1

#endif // BLE_FEATURES_H
