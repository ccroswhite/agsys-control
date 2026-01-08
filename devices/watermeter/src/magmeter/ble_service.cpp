/**
 * @file ble_service.cpp
 * @brief BLE service implementation for Mag Meter
 * 
 * Uses Bluefruit library for nRF52840 BLE.
 */

#include "ble_service.h"
#include "magmeter_config.h"
#include "settings.h"
#include "calibration.h"
#include <bluefruit.h>

// BLE Services and Characteristics
static BLEService magmeterService(BLE_UUID_MAGMETER_SERVICE);
static BLECharacteristic deviceInfoChar(BLE_UUID_DEVICE_INFO);
static BLECharacteristic secretSaltChar(BLE_UUID_SECRET_SALT);
static BLECharacteristic settingsChar(BLE_UUID_SETTINGS);
static BLECharacteristic liveDataChar(BLE_UUID_LIVE_DATA);
static BLECharacteristic calibrationChar(BLE_UUID_CALIBRATION);
static BLECharacteristic calCommandChar(BLE_UUID_CAL_COMMAND);
static BLECharacteristic diagnosticsChar(BLE_UUID_DIAGNOSTICS);

// State
static BleState_t currentState = BLE_STATE_IDLE;
static bool notifyEnabled = false;

// Callbacks
static BleSettingsCallback_t settingsCallback = NULL;
static BleCalCommandCallback_t calCommandCallback = NULL;
static BleSaltCallback_t saltCallback = NULL;

// Device info (set during init)
static BleDeviceInfo_t deviceInfo;

// Forward declarations
static void connectCallback(uint16_t conn_handle);
static void disconnectCallback(uint16_t conn_handle, uint8_t reason);
static void settingsWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
static void saltWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
static void calCommandWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
static void cccdCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t cccd_value);

void ble_init(void) {
    DEBUG_PRINTLN("Initializing BLE...");
    
    // Initialize Bluefruit
    Bluefruit.begin();
    Bluefruit.setTxPower(4);  // 4 dBm
    Bluefruit.setName("AgSys MagMeter");
    
    // Set callbacks
    Bluefruit.Periph.setConnectCallback(connectCallback);
    Bluefruit.Periph.setDisconnectCallback(disconnectCallback);
    
    // Setup Mag Meter service
    magmeterService.begin();
    
    // Device Info characteristic (read-only)
    deviceInfoChar.setProperties(CHR_PROPS_READ);
    deviceInfoChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    deviceInfoChar.setFixedLen(sizeof(BleDeviceInfo_t));
    deviceInfoChar.begin();
    
    // Populate device info
    extern uint8_t deviceUid[8];
    extern uint8_t currentTier;
    memcpy(deviceInfo.deviceUid, deviceUid, 8);
    deviceInfo.deviceType = DEVICE_TYPE;
    deviceInfo.tier = currentTier;
    deviceInfo.firmwareMajor = FIRMWARE_VERSION_MAJOR;
    deviceInfo.firmwareMinor = FIRMWARE_VERSION_MINOR;
    deviceInfo.firmwarePatch = FIRMWARE_VERSION_PATCH;
    deviceInfoChar.write(&deviceInfo, sizeof(deviceInfo));
    
    // Secret Salt characteristic (write-only, encrypted)
    secretSaltChar.setProperties(CHR_PROPS_WRITE);
    secretSaltChar.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
    secretSaltChar.setMaxLen(32);  // Max 32 bytes for salt
    secretSaltChar.setWriteCallback(saltWriteCallback);
    secretSaltChar.begin();
    
    // Settings characteristic (read/write)
    settingsChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    settingsChar.setPermission(SECMODE_OPEN, SECMODE_ENC_NO_MITM);
    settingsChar.setFixedLen(sizeof(BleSettings_t));
    settingsChar.setWriteCallback(settingsWriteCallback);
    settingsChar.begin();
    
    // Update settings from current values
    UserSettings_t* settings = settings_get();
    BleSettings_t bleSettings;
    bleSettings.unitSystem = (uint8_t)settings->unitSystem;
    bleSettings.trendPeriodMin = settings->trendPeriodMin;
    bleSettings.avgPeriodMin = settings->avgPeriodMin;
    bleSettings.maxFlowLPM = (uint16_t)settings->maxFlowLPM;
    bleSettings.backlightOn = settings->backlightOn;
    settingsChar.write(&bleSettings, sizeof(bleSettings));
    
    // Live Data characteristic (read/notify)
    liveDataChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    liveDataChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    liveDataChar.setFixedLen(sizeof(BleLiveData_t));
    liveDataChar.setCccdWriteCallback(cccdCallback);
    liveDataChar.begin();
    
    // Calibration characteristic (read-only)
    calibrationChar.setProperties(CHR_PROPS_READ);
    calibrationChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    calibrationChar.setFixedLen(sizeof(BleCalibration_t));
    calibrationChar.begin();
    
    // Update calibration from current values
    CalibrationData_t* cal = calibration_get();
    BleCalibration_t bleCal;
    bleCal.zeroOffset = cal->zeroOffset;
    bleCal.spanFactor = cal->spanFactor;
    bleCal.kFactor = cal->kFactor;
    bleCal.calDate = cal->calDate;
    calibrationChar.write(&bleCal, sizeof(bleCal));
    
    // Calibration Command characteristic (write-only)
    calCommandChar.setProperties(CHR_PROPS_WRITE);
    calCommandChar.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
    calCommandChar.setFixedLen(sizeof(BleCalCommand_t));
    calCommandChar.setWriteCallback(calCommandWriteCallback);
    calCommandChar.begin();
    
    // Diagnostics characteristic (read-only)
    diagnosticsChar.setProperties(CHR_PROPS_READ);
    diagnosticsChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    diagnosticsChar.setFixedLen(sizeof(BleDiagnostics_t));
    diagnosticsChar.begin();
    
    currentState = BLE_STATE_IDLE;
    DEBUG_PRINTLN("BLE initialized");
}

void ble_startAdvertising(void) {
    if (currentState == BLE_STATE_CONNECTED) {
        return;  // Already connected
    }
    
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(magmeterService);
    Bluefruit.Advertising.addName();
    
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(160, 244);  // 100-152.5ms
    Bluefruit.Advertising.setFastTimeout(30);     // 30 seconds fast advertising
    Bluefruit.Advertising.start(0);               // Advertise forever
    
    currentState = BLE_STATE_ADVERTISING;
    DEBUG_PRINTLN("BLE advertising started");
}

void ble_stopAdvertising(void) {
    Bluefruit.Advertising.stop();
    currentState = BLE_STATE_IDLE;
    DEBUG_PRINTLN("BLE advertising stopped");
}

bool ble_isConnected(void) {
    return currentState == BLE_STATE_CONNECTED;
}

BleState_t ble_getState(void) {
    return currentState;
}

void ble_updateLiveData(float flowRate, float totalVol, float trend, float avg, bool reverse) {
    if (!ble_isConnected() || !notifyEnabled) {
        return;
    }
    
    BleLiveData_t data;
    data.flowRate_LPM = flowRate;
    data.totalVolume_L = totalVol;
    data.trendVolume_L = trend;
    data.avgVolume_L = avg;
    
    if (fabs(flowRate) < 0.1f) {
        data.flowDirection = 0;  // None
    } else if (reverse) {
        data.flowDirection = 2;  // Reverse
    } else {
        data.flowDirection = 1;  // Forward
    }
    data.statusFlags = 0;  // TODO: Add actual status flags
    
    liveDataChar.notify(&data, sizeof(data));
}

void ble_updateDiagnostics(int32_t adcElec, int32_t adcCur, uint16_t coilFreq, uint16_t coilCur_mA) {
    extern uint8_t currentTier;
    
    BleDiagnostics_t diag;
    diag.adcElectrode = adcElec;
    diag.adcCurrent = adcCur;
    diag.coilFrequency = coilFreq;
    diag.coilCurrent_mA = coilCur_mA;
    diag.tier = currentTier;
    diag.errorFlags = 0;  // TODO: Add actual error flags
    
    diagnosticsChar.write(&diag, sizeof(diag));
}

void ble_process(void) {
    // Bluefruit handles events internally via SoftDevice
    // Nothing to do here for basic operation
}

void ble_setSettingsCallback(BleSettingsCallback_t callback) {
    settingsCallback = callback;
}

void ble_setCalCommandCallback(BleCalCommandCallback_t callback) {
    calCommandCallback = callback;
}

void ble_setSaltCallback(BleSaltCallback_t callback) {
    saltCallback = callback;
}

// Connection callback
static void connectCallback(uint16_t conn_handle) {
    (void)conn_handle;
    currentState = BLE_STATE_CONNECTED;
    DEBUG_PRINTLN("BLE connected");
    
    // Update characteristics with current values
    UserSettings_t* settings = settings_get();
    BleSettings_t bleSettings;
    bleSettings.unitSystem = (uint8_t)settings->unitSystem;
    bleSettings.trendPeriodMin = settings->trendPeriodMin;
    bleSettings.avgPeriodMin = settings->avgPeriodMin;
    bleSettings.maxFlowLPM = (uint16_t)settings->maxFlowLPM;
    bleSettings.backlightOn = settings->backlightOn;
    settingsChar.write(&bleSettings, sizeof(bleSettings));
    
    CalibrationData_t* cal = calibration_get();
    BleCalibration_t bleCal;
    bleCal.zeroOffset = cal->zeroOffset;
    bleCal.spanFactor = cal->spanFactor;
    bleCal.kFactor = cal->kFactor;
    bleCal.calDate = cal->calDate;
    calibrationChar.write(&bleCal, sizeof(bleCal));
}

// Disconnection callback
static void disconnectCallback(uint16_t conn_handle, uint8_t reason) {
    (void)conn_handle;
    (void)reason;
    currentState = BLE_STATE_ADVERTISING;  // Auto-restart advertising
    notifyEnabled = false;
    DEBUG_PRINTLN("BLE disconnected");
}

// Settings write callback
static void settingsWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (len != sizeof(BleSettings_t)) {
        DEBUG_PRINTLN("Invalid settings length");
        return;
    }
    
    BleSettings_t* bleSettings = (BleSettings_t*)data;
    
    // Update local settings
    UserSettings_t* settings = settings_get();
    settings->unitSystem = (UnitSystem_t)bleSettings->unitSystem;
    settings->trendPeriodMin = bleSettings->trendPeriodMin;
    settings->avgPeriodMin = bleSettings->avgPeriodMin;
    settings->maxFlowLPM = (float)bleSettings->maxFlowLPM;
    settings->backlightOn = bleSettings->backlightOn;
    
    // Save to FRAM
    settings_save();
    
    DEBUG_PRINTLN("Settings updated via BLE");
    
    // Notify callback if set
    if (settingsCallback) {
        settingsCallback(bleSettings);
    }
}

// Secret salt write callback
static void saltWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (len < 8 || len > 32) {
        DEBUG_PRINTLN("Invalid salt length");
        return;
    }
    
    DEBUG_PRINTLN("Secret salt received via BLE");
    
    // Notify callback to handle salt storage
    if (saltCallback) {
        saltCallback(data, len);
    }
}

// Calibration command write callback
static void calCommandWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (len != sizeof(BleCalCommand_t)) {
        DEBUG_PRINTLN("Invalid cal command length");
        return;
    }
    
    BleCalCommand_t* cmd = (BleCalCommand_t*)data;
    
    switch (cmd->command) {
        case CAL_CMD_CAPTURE_ZERO:
            DEBUG_PRINTLN("BLE: Capture zero command");
            calibration_captureZero();
            break;
            
        case CAL_CMD_SET_SPAN:
            DEBUG_PRINTF("BLE: Set span to %.3f\n", cmd->value);
            calibration_setSpan(cmd->value);
            break;
            
        case CAL_CMD_RESET:
            DEBUG_PRINTLN("BLE: Reset calibration");
            calibration_reset();
            calibration_save();
            break;
            
        default:
            DEBUG_PRINTF("Unknown cal command: %d\n", cmd->command);
            break;
    }
    
    // Update calibration characteristic
    CalibrationData_t* cal = calibration_get();
    BleCalibration_t bleCal;
    bleCal.zeroOffset = cal->zeroOffset;
    bleCal.spanFactor = cal->spanFactor;
    bleCal.kFactor = cal->kFactor;
    bleCal.calDate = cal->calDate;
    calibrationChar.write(&bleCal, sizeof(bleCal));
    
    // Notify callback if set
    if (calCommandCallback) {
        calCommandCallback(cmd);
    }
}

// CCCD callback for notifications
static void cccdCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t cccd_value) {
    (void)conn_hdl;
    (void)chr;
    
    notifyEnabled = (cccd_value & BLE_GATT_HVX_NOTIFICATION);
    DEBUG_PRINTF("Live data notifications %s\n", notifyEnabled ? "enabled" : "disabled");
}
