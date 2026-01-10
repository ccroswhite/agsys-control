/**
 * @file agsys_ble.cpp
 * @brief Unified BLE Service Implementation
 */

#include "agsys_ble.h"
#include <Adafruit_FRAM_SPI.h>
#include <bluefruit.h>

// External FRAM instance (must be defined by device's main.cpp)
extern Adafruit_FRAM_SPI fram;

// ============================================================================
// BLE Service and Characteristics
// ============================================================================

static BLEService agsysService(AGSYS_BLE_UUID_SERVICE);

#if AGSYS_BLE_FEATURE_DFU
static BLEDfu bleDfu;
#endif

// Core characteristics (always enabled)
static BLECharacteristic deviceInfoChar(AGSYS_BLE_UUID_DEVICE_INFO);
static BLECharacteristic pinAuthChar(AGSYS_BLE_UUID_PIN_AUTH);
static BLECharacteristic pinChangeChar(AGSYS_BLE_UUID_PIN_CHANGE);

#if AGSYS_BLE_FEATURE_SETTINGS
static BLECharacteristic settingsChar(AGSYS_BLE_UUID_SETTINGS);
#endif

#if AGSYS_BLE_FEATURE_LIVE_DATA
static BLECharacteristic liveDataChar(AGSYS_BLE_UUID_LIVE_DATA);
#endif

#if AGSYS_BLE_FEATURE_VALVE
static BLECharacteristic valveCmdChar(AGSYS_BLE_UUID_VALVE_CMD);
static BLECharacteristic valveStatusChar(AGSYS_BLE_UUID_VALVE_STATUS);
#endif

#if AGSYS_BLE_FEATURE_CAN_DISCOVERY
static BLECharacteristic canDiscoveryChar(AGSYS_BLE_UUID_CAN_DISCOVERY);
static BLECharacteristic actuatorListChar(AGSYS_BLE_UUID_ACTUATOR_LIST);
#endif

#if AGSYS_BLE_FEATURE_CALIBRATION
static BLECharacteristic calibrationChar(AGSYS_BLE_UUID_CALIBRATION);
static BLECharacteristic calCommandChar(AGSYS_BLE_UUID_CAL_COMMAND);
#endif

#if AGSYS_BLE_FEATURE_DIAGNOSTICS
static BLECharacteristic diagnosticsChar(AGSYS_BLE_UUID_DIAGNOSTICS);
#endif

// ============================================================================
// State Variables
// ============================================================================

static char storedPin[AGSYS_PIN_LENGTH + 1] = AGSYS_DEFAULT_PIN;
static bool isAuthenticated = false;
static uint32_t authTime = 0;
static uint8_t failedAttempts = 0;
static uint32_t lockoutStartTime = 0;
static uint16_t framPinAddr = 0;
static uint8_t currentDeviceType = 0;
static const char* deviceName = "AgSys";

#if AGSYS_BLE_FEATURE_CAN_DISCOVERY
static volatile bool discoveryRequested = false;
static uint8_t discoveryStatus = AGSYS_DISCOVERY_IDLE;
#endif

// Callbacks
static AgsysBleAuthCallback_t authCallback = NULL;
static AgsysBleSettingsCallback_t settingsCallback = NULL;
static AgsysBleValveCallback_t valveCallback = NULL;
static AgsysBleCalCallback_t calCallback = NULL;
static AgsysBleDiscoveryCallback_t discoveryCallback = NULL;

// ============================================================================
// Forward Declarations
// ============================================================================

static void onConnect(uint16_t conn_handle);
static void onDisconnect(uint16_t conn_handle, uint8_t reason);
static void onPinAuthWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
static void onPinChangeWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
static bool checkAuth(void);
static void updateAuthStatus(void);
static void loadPin(void);
static void savePin(void);
static uint16_t getFeatureMask(void);

#if AGSYS_BLE_FEATURE_SETTINGS
static void onSettingsWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
#endif

#if AGSYS_BLE_FEATURE_VALVE
static void onValveCmdWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
#endif

#if AGSYS_BLE_FEATURE_CAN_DISCOVERY
static void onCanDiscoveryWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
#endif

#if AGSYS_BLE_FEATURE_CALIBRATION
static void onCalCommandWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
#endif

// ============================================================================
// Initialization
// ============================================================================

// Firmware version storage
static uint8_t fwVersionMajor = 0;
static uint8_t fwVersionMinor = 0;
static uint8_t fwVersionPatch = 0;

void agsys_ble_init(const char* name, uint8_t deviceType, uint16_t pinAddr,
                    uint8_t fwMajor, uint8_t fwMinor, uint8_t fwPatch) {
    deviceName = name;
    currentDeviceType = deviceType;
    framPinAddr = pinAddr;
    fwVersionMajor = fwMajor;
    fwVersionMinor = fwMinor;
    fwVersionPatch = fwPatch;
    
    // Load PIN from FRAM
    loadPin();
    
    // Initialize Bluefruit
    Bluefruit.begin();
    Bluefruit.setTxPower(4);
    Bluefruit.setName(deviceName);
    
    // Set callbacks
    Bluefruit.Periph.setConnectCallback(onConnect);
    Bluefruit.Periph.setDisconnectCallback(onDisconnect);
    
    // Start service
    agsysService.begin();
    
#if AGSYS_BLE_FEATURE_DFU
    // Initialize DFU service for OTA firmware updates
    bleDfu.begin();
#endif
    
    // ---- Device Info (Read) ----
    deviceInfoChar.setProperties(CHR_PROPS_READ);
    deviceInfoChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    deviceInfoChar.setFixedLen(sizeof(AgsysBleDeviceInfo_t));
    deviceInfoChar.begin();
    
    // Populate device info
    AgsysBleDeviceInfo_t info;
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    uint32_t id1 = NRF_FICR->DEVICEID[1];
    info.uid[0] = (id0 >> 0) & 0xFF;
    info.uid[1] = (id0 >> 8) & 0xFF;
    info.uid[2] = (id0 >> 16) & 0xFF;
    info.uid[3] = (id0 >> 24) & 0xFF;
    info.uid[4] = (id1 >> 0) & 0xFF;
    info.uid[5] = (id1 >> 8) & 0xFF;
    info.uid[6] = (id1 >> 16) & 0xFF;
    info.uid[7] = (id1 >> 24) & 0xFF;
    info.deviceType = deviceType;
    info.fwMajor = fwVersionMajor;
    info.fwMinor = fwVersionMinor;
    info.fwPatch = fwVersionPatch;
    info.features = getFeatureMask();
    deviceInfoChar.write(&info, sizeof(info));
    
    // ---- PIN Auth (Read/Write) ----
    pinAuthChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    pinAuthChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    pinAuthChar.setWriteCallback(onPinAuthWrite);
    pinAuthChar.setMaxLen(AGSYS_PIN_LENGTH);
    pinAuthChar.begin();
    uint8_t authStatus = AGSYS_AUTH_NOT_AUTHENTICATED;
    pinAuthChar.write(&authStatus, 1);
    
    // ---- PIN Change (Write) ----
    pinChangeChar.setProperties(CHR_PROPS_WRITE);
    pinChangeChar.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
    pinChangeChar.setWriteCallback(onPinChangeWrite);
    pinChangeChar.setMaxLen(AGSYS_PIN_LENGTH * 2);
    pinChangeChar.begin();
    
#if AGSYS_BLE_FEATURE_SETTINGS
    // ---- Settings (Read/Write) ----
    settingsChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    settingsChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    settingsChar.setWriteCallback(onSettingsWrite);
    settingsChar.setFixedLen(sizeof(AgsysBleSettings_t));
    settingsChar.begin();
#endif

#if AGSYS_BLE_FEATURE_LIVE_DATA
    // ---- Live Data (Read/Notify) ----
    liveDataChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    liveDataChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    liveDataChar.setFixedLen(sizeof(AgsysBleLiveData_t));
    liveDataChar.begin();
#endif

#if AGSYS_BLE_FEATURE_VALVE
    // ---- Valve Command (Write) ----
    valveCmdChar.setProperties(CHR_PROPS_WRITE);
    valveCmdChar.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
    valveCmdChar.setWriteCallback(onValveCmdWrite);
    valveCmdChar.setFixedLen(sizeof(AgsysBleValveCmd_t));
    valveCmdChar.begin();
    
    // ---- Valve Status (Read/Notify) ----
    valveStatusChar.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    valveStatusChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    valveStatusChar.setMaxLen(sizeof(AgsysBleValveStatus_t));
    valveStatusChar.begin();
#endif

#if AGSYS_BLE_FEATURE_CAN_DISCOVERY
    // ---- CAN Discovery (Read/Write/Notify) ----
    canDiscoveryChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_NOTIFY);
    canDiscoveryChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    canDiscoveryChar.setWriteCallback(onCanDiscoveryWrite);
    canDiscoveryChar.setMaxLen(2);
    canDiscoveryChar.begin();
    uint8_t discStatus[2] = {AGSYS_DISCOVERY_IDLE, 0};
    canDiscoveryChar.write(discStatus, 2);
    
    // ---- Actuator List (Read) ----
    actuatorListChar.setProperties(CHR_PROPS_READ);
    actuatorListChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    actuatorListChar.setMaxLen(200);
    actuatorListChar.begin();
#endif

#if AGSYS_BLE_FEATURE_CALIBRATION
    // ---- Calibration (Read/Write) ----
    calibrationChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    calibrationChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    calibrationChar.setMaxLen(16);
    calibrationChar.begin();
    
    // ---- Cal Command (Write) ----
    calCommandChar.setProperties(CHR_PROPS_WRITE);
    calCommandChar.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
    calCommandChar.setWriteCallback(onCalCommandWrite);
    calCommandChar.setFixedLen(sizeof(AgsysBleCalCmd_t));
    calCommandChar.begin();
#endif

#if AGSYS_BLE_FEATURE_DIAGNOSTICS
    // ---- Diagnostics (Read) ----
    diagnosticsChar.setProperties(CHR_PROPS_READ);
    diagnosticsChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    diagnosticsChar.setFixedLen(sizeof(AgsysBleDiagnostics_t));
    diagnosticsChar.begin();
#endif
}

void agsys_ble_start_advertising(void) {
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(agsysService);
    
    // Add manufacturer data with device type and UID for pre-connect identification
    // Format: [Company ID 2 bytes][Device Type 1 byte][UID 8 bytes]
    // Company ID: 0xFFFF (reserved for testing/development)
    uint8_t mfgData[11];
    mfgData[0] = 0xFF;  // Company ID low byte
    mfgData[1] = 0xFF;  // Company ID high byte
    mfgData[2] = currentDeviceType;
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    uint32_t id1 = NRF_FICR->DEVICEID[1];
    mfgData[3] = (id0 >> 0) & 0xFF;
    mfgData[4] = (id0 >> 8) & 0xFF;
    mfgData[5] = (id0 >> 16) & 0xFF;
    mfgData[6] = (id0 >> 24) & 0xFF;
    mfgData[7] = (id1 >> 0) & 0xFF;
    mfgData[8] = (id1 >> 8) & 0xFF;
    mfgData[9] = (id1 >> 16) & 0xFF;
    mfgData[10] = (id1 >> 24) & 0xFF;
    Bluefruit.Advertising.addManufacturerData(mfgData, sizeof(mfgData));
    
#if AGSYS_BLE_FEATURE_DFU
    // Add DFU service to advertising for OTA update discovery
    Bluefruit.Advertising.addService(bleDfu);
#endif
    
    Bluefruit.Advertising.addName();
    
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(160, 244);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
}

void agsys_ble_stop_advertising(void) {
    Bluefruit.Advertising.stop();
}

bool agsys_ble_is_connected(void) {
    return Bluefruit.connected();
}

bool agsys_ble_is_authenticated(void) {
    return checkAuth();
}

void agsys_ble_process(void) {
#if AGSYS_BLE_FEATURE_CAN_DISCOVERY
    if (discoveryRequested && discoveryCallback) {
        discoveryRequested = false;
        discoveryStatus = AGSYS_DISCOVERY_IN_PROGRESS;
        
        uint8_t status[2] = {AGSYS_DISCOVERY_IN_PROGRESS, 0};
        canDiscoveryChar.write(status, 2);
        if (Bluefruit.connected()) {
            canDiscoveryChar.notify(status, 2);
        }
        
        // Call device-specific discovery handler
        discoveryCallback();
        
        discoveryStatus = AGSYS_DISCOVERY_COMPLETE;
    }
#endif
}

void agsys_ble_clear_auth(void) {
    isAuthenticated = false;
    updateAuthStatus();
    if (authCallback) {
        authCallback(false);
    }
}

// ============================================================================
// Callback Setters
// ============================================================================

void agsys_ble_set_auth_callback(AgsysBleAuthCallback_t callback) {
    authCallback = callback;
}

void agsys_ble_set_settings_callback(AgsysBleSettingsCallback_t callback) {
    settingsCallback = callback;
}

void agsys_ble_set_valve_callback(AgsysBleValveCallback_t callback) {
    valveCallback = callback;
}

void agsys_ble_set_cal_callback(AgsysBleCalCallback_t callback) {
    calCallback = callback;
}

void agsys_ble_set_discovery_callback(AgsysBleDiscoveryCallback_t callback) {
    discoveryCallback = callback;
}

// ============================================================================
// Data Update Functions
// ============================================================================

#if AGSYS_BLE_FEATURE_LIVE_DATA
void agsys_ble_update_live_data(AgsysBleLiveData_t* data) {
    liveDataChar.write(data, sizeof(AgsysBleLiveData_t));
    if (Bluefruit.connected()) {
        liveDataChar.notify(data, sizeof(AgsysBleLiveData_t));
    }
}
#endif

#if AGSYS_BLE_FEATURE_VALVE
void agsys_ble_update_valve_status(AgsysBleValveStatus_t* status) {
    valveStatusChar.write(status, sizeof(AgsysBleValveStatus_t));
    if (Bluefruit.connected()) {
        valveStatusChar.notify(status, sizeof(AgsysBleValveStatus_t));
    }
}

void agsys_ble_set_discovery_results(uint8_t count, AgsysBleActuatorInfo_t* actuators) {
    uint8_t buffer[200];
    buffer[0] = count;
    
    size_t len = 1;
    for (uint8_t i = 0; i < count && i < 18; i++) {
        memcpy(buffer + len, &actuators[i], sizeof(AgsysBleActuatorInfo_t));
        len += sizeof(AgsysBleActuatorInfo_t);
    }
    
    actuatorListChar.write(buffer, len);
    
    uint8_t status[2] = {AGSYS_DISCOVERY_COMPLETE, count};
    canDiscoveryChar.write(status, 2);
    if (Bluefruit.connected()) {
        canDiscoveryChar.notify(status, 2);
    }
}
#endif

#if AGSYS_BLE_FEATURE_CALIBRATION
void agsys_ble_update_calibration_meter(AgsysBleCalMeter_t* cal) {
    calibrationChar.write(cal, sizeof(AgsysBleCalMeter_t));
}

void agsys_ble_update_calibration_soil(AgsysBleCalSoil_t* cal) {
    calibrationChar.write(cal, sizeof(AgsysBleCalSoil_t));
}
#endif

#if AGSYS_BLE_FEATURE_DIAGNOSTICS
void agsys_ble_update_diagnostics(AgsysBleDiagnostics_t* diag) {
    diagnosticsChar.write(diag, sizeof(AgsysBleDiagnostics_t));
}
#endif

// ============================================================================
// Internal Callbacks
// ============================================================================

static void onConnect(uint16_t conn_handle) {
    (void)conn_handle;
    isAuthenticated = false;
    updateAuthStatus();
}

static void onDisconnect(uint16_t conn_handle, uint8_t reason) {
    (void)conn_handle;
    (void)reason;
    isAuthenticated = false;
}

static void onPinAuthWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    // Check lockout
    if (failedAttempts >= AGSYS_PIN_MAX_ATTEMPTS) {
        if (millis() - lockoutStartTime < AGSYS_PIN_LOCKOUT_MS) {
            uint8_t status = AGSYS_AUTH_LOCKED_OUT;
            pinAuthChar.write(&status, 1);
            return;
        }
        failedAttempts = 0;
    }
    
    if (len != AGSYS_PIN_LENGTH) {
        uint8_t status = AGSYS_AUTH_FAILED;
        pinAuthChar.write(&status, 1);
        return;
    }
    
    // Compare PIN
    bool match = true;
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (data[i] != storedPin[i]) {
            match = false;
            break;
        }
    }
    
    if (match) {
        isAuthenticated = true;
        authTime = millis();
        failedAttempts = 0;
        updateAuthStatus();
        if (authCallback) {
            authCallback(true);
        }
    } else {
        failedAttempts++;
        if (failedAttempts >= AGSYS_PIN_MAX_ATTEMPTS) {
            lockoutStartTime = millis();
            uint8_t status = AGSYS_AUTH_LOCKED_OUT;
            pinAuthChar.write(&status, 1);
        } else {
            uint8_t status = AGSYS_AUTH_FAILED;
            pinAuthChar.write(&status, 1);
        }
    }
}

static void onPinChangeWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (!checkAuth()) return;
    if (len != AGSYS_PIN_LENGTH * 2) return;
    
    // Verify old PIN
    bool match = true;
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (data[i] != storedPin[i]) {
            match = false;
            break;
        }
    }
    if (!match) return;
    
    // Validate new PIN (all digits)
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (data[AGSYS_PIN_LENGTH + i] < '0' || data[AGSYS_PIN_LENGTH + i] > '9') {
            return;
        }
    }
    
    // Set new PIN
    memcpy(storedPin, data + AGSYS_PIN_LENGTH, AGSYS_PIN_LENGTH);
    storedPin[AGSYS_PIN_LENGTH] = '\0';
    savePin();
    
    uint8_t status = AGSYS_AUTH_PIN_CHANGED;
    pinAuthChar.write(&status, 1);
}

#if AGSYS_BLE_FEATURE_SETTINGS
static void onSettingsWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (!checkAuth()) return;
    if (len != sizeof(AgsysBleSettings_t)) return;
    
    if (settingsCallback) {
        settingsCallback((AgsysBleSettings_t*)data);
    }
}
#endif

#if AGSYS_BLE_FEATURE_VALVE
static void onValveCmdWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (!checkAuth()) return;
    if (len != sizeof(AgsysBleValveCmd_t)) return;
    
    if (valveCallback) {
        valveCallback((AgsysBleValveCmd_t*)data);
    }
}
#endif

#if AGSYS_BLE_FEATURE_CAN_DISCOVERY
static void onCanDiscoveryWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (!checkAuth()) return;
    if (len < 1) return;
    
    if (data[0] == 0x01 && discoveryStatus != AGSYS_DISCOVERY_IN_PROGRESS) {
        discoveryRequested = true;
    }
}
#endif

#if AGSYS_BLE_FEATURE_CALIBRATION
static void onCalCommandWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    if (!checkAuth()) return;
    if (len != sizeof(AgsysBleCalCmd_t)) return;
    
    if (calCallback) {
        calCallback((AgsysBleCalCmd_t*)data);
    }
}
#endif

// ============================================================================
// Helper Functions
// ============================================================================

static bool checkAuth(void) {
    if (!isAuthenticated) return false;
    
    if (millis() - authTime > AGSYS_AUTH_TIMEOUT_MS) {
        isAuthenticated = false;
        updateAuthStatus();
        if (authCallback) {
            authCallback(false);
        }
        return false;
    }
    
    return true;
}

static void updateAuthStatus(void) {
    uint8_t status = isAuthenticated ? AGSYS_AUTH_AUTHENTICATED : AGSYS_AUTH_NOT_AUTHENTICATED;
    pinAuthChar.write(&status, 1);
}

static void loadPin(void) {
    char loaded[AGSYS_PIN_LENGTH + 1];
    fram.read(framPinAddr, (uint8_t*)loaded, AGSYS_PIN_LENGTH);
    loaded[AGSYS_PIN_LENGTH] = '\0';
    
    // Validate (all digits, not all 0xFF)
    bool valid = true;
    bool allFF = true;
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (loaded[i] != 0xFF) allFF = false;
        if (loaded[i] < '0' || loaded[i] > '9') valid = false;
    }
    
    if (valid && !allFF) {
        memcpy(storedPin, loaded, AGSYS_PIN_LENGTH);
        storedPin[AGSYS_PIN_LENGTH] = '\0';
    } else {
        // Use default and save
        memcpy(storedPin, AGSYS_DEFAULT_PIN, AGSYS_PIN_LENGTH);
        storedPin[AGSYS_PIN_LENGTH] = '\0';
        savePin();
    }
}

static void savePin(void) {
    fram.write(framPinAddr, (uint8_t*)storedPin, AGSYS_PIN_LENGTH);
}

static uint16_t getFeatureMask(void) {
    uint16_t mask = 0;
    
#if AGSYS_BLE_FEATURE_AUTH
    mask |= AGSYS_FEATURE_AUTH;
#endif
#if AGSYS_BLE_FEATURE_SETTINGS
    mask |= AGSYS_FEATURE_SETTINGS;
#endif
#if AGSYS_BLE_FEATURE_LIVE_DATA
    mask |= AGSYS_FEATURE_LIVE_DATA;
#endif
#if AGSYS_BLE_FEATURE_VALVE
    mask |= AGSYS_FEATURE_VALVE;
#endif
#if AGSYS_BLE_FEATURE_CAN_DISCOVERY
    mask |= AGSYS_FEATURE_CAN_DISCOVERY;
#endif
#if AGSYS_BLE_FEATURE_CALIBRATION
    mask |= AGSYS_FEATURE_CALIBRATION;
#endif
#if AGSYS_BLE_FEATURE_DIAGNOSTICS
    mask |= AGSYS_FEATURE_DIAGNOSTICS;
#endif
#if AGSYS_BLE_FEATURE_DFU
    mask |= AGSYS_FEATURE_DFU;
#endif
    
    return mask;
}
