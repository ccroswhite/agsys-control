/**
 * @file agsys_ble_auth.cpp
 * @brief Shared BLE PIN Authentication implementation
 */

#include "agsys_ble_auth.h"
#include <Adafruit_FRAM_SPI.h>

// External FRAM instance (must be defined by the device's main.cpp)
extern Adafruit_FRAM_SPI fram;

// BLE Characteristics
static BLECharacteristic pinAuthChar(BLE_UUID_PIN_AUTH);
static BLECharacteristic pinChangeChar(BLE_UUID_PIN_CHANGE);

// Authentication state
static char storedPin[AGSYS_PIN_LENGTH + 1] = AGSYS_DEFAULT_PIN;
static bool isAuthenticated = false;
static uint32_t authTime = 0;
static uint8_t failedAttempts = 0;
static uint32_t lockoutStartTime = 0;
static uint16_t framPinAddr = 0;
static AgsysAuthCallback_t authCallback = NULL;

// Forward declarations
static void onPinAuthWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
static void onPinChangeWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
static void updateAuthStatus(void);

void agsys_ble_auth_init(uint16_t framAddr, AgsysAuthCallback_t callback) {
    framPinAddr = framAddr;
    authCallback = callback;
    
    // Load PIN from FRAM
    agsys_ble_auth_load_pin();
}

void agsys_ble_auth_begin(void) {
    // PIN Authentication Characteristic (Read + Write)
    // Write: [PIN 6 bytes] - authenticate with PIN
    // Read: [Status] - authentication status
    pinAuthChar.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
    pinAuthChar.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    pinAuthChar.setWriteCallback(onPinAuthWrite);
    pinAuthChar.setMaxLen(AGSYS_PIN_LENGTH);
    pinAuthChar.begin();
    
    // Set initial auth status
    uint8_t authStatus = AGSYS_AUTH_NOT_AUTHENTICATED;
    pinAuthChar.write(&authStatus, 1);
    
    // PIN Change Characteristic (Write only, requires auth)
    // Write: [Old PIN 6 bytes][New PIN 6 bytes]
    pinChangeChar.setProperties(CHR_PROPS_WRITE);
    pinChangeChar.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
    pinChangeChar.setWriteCallback(onPinChangeWrite);
    pinChangeChar.setMaxLen(AGSYS_PIN_LENGTH * 2);
    pinChangeChar.begin();
}

bool agsys_ble_auth_check(void) {
    if (!isAuthenticated) {
        return false;
    }
    
    // Check for session timeout
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

void agsys_ble_auth_clear(void) {
    isAuthenticated = false;
    updateAuthStatus();
    if (authCallback) {
        authCallback(false);
    }
}

void agsys_ble_auth_set_pin(const char* pin) {
    if (strlen(pin) != AGSYS_PIN_LENGTH) {
        return;
    }
    
    memcpy(storedPin, pin, AGSYS_PIN_LENGTH);
    storedPin[AGSYS_PIN_LENGTH] = '\0';
    
    // Save to FRAM
    fram.write(framPinAddr, (uint8_t*)storedPin, AGSYS_PIN_LENGTH);
}

void agsys_ble_auth_load_pin(void) {
    char loadedPin[AGSYS_PIN_LENGTH + 1];
    fram.read(framPinAddr, (uint8_t*)loadedPin, AGSYS_PIN_LENGTH);
    loadedPin[AGSYS_PIN_LENGTH] = '\0';
    
    // Check if PIN is valid (all digits, not all 0xFF)
    bool valid = true;
    bool allFF = true;
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (loadedPin[i] != 0xFF) {
            allFF = false;
        }
        if (loadedPin[i] < '0' || loadedPin[i] > '9') {
            valid = false;
        }
    }
    
    if (valid && !allFF) {
        memcpy(storedPin, loadedPin, AGSYS_PIN_LENGTH);
        storedPin[AGSYS_PIN_LENGTH] = '\0';
    } else {
        // Use default PIN and save it
        agsys_ble_auth_set_pin(AGSYS_DEFAULT_PIN);
    }
}

uint8_t agsys_ble_auth_get_status(void) {
    // Check lockout
    if (failedAttempts >= AGSYS_PIN_MAX_ATTEMPTS) {
        if (millis() - lockoutStartTime < AGSYS_PIN_LOCKOUT_MS) {
            return AGSYS_AUTH_LOCKED_OUT;
        }
    }
    
    if (agsys_ble_auth_check()) {
        return AGSYS_AUTH_AUTHENTICATED;
    }
    
    return AGSYS_AUTH_NOT_AUTHENTICATED;
}

void agsys_ble_auth_reset_lockout(void) {
    failedAttempts = 0;
    lockoutStartTime = 0;
}

// Callback: PIN authentication attempt
static void onPinAuthWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    // Check for lockout
    if (failedAttempts >= AGSYS_PIN_MAX_ATTEMPTS) {
        if (millis() - lockoutStartTime < AGSYS_PIN_LOCKOUT_MS) {
            uint8_t status = AGSYS_AUTH_LOCKED_OUT;
            pinAuthChar.write(&status, 1);
            return;
        } else {
            // Lockout expired
            failedAttempts = 0;
        }
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

// Callback: PIN change request
static void onPinChangeWrite(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    (void)conn_hdl;
    (void)chr;
    
    // Require authentication to change PIN
    if (!agsys_ble_auth_check()) {
        return;
    }
    
    if (len != AGSYS_PIN_LENGTH * 2) {
        return;
    }
    
    // Verify old PIN
    bool oldMatch = true;
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (data[i] != storedPin[i]) {
            oldMatch = false;
            break;
        }
    }
    
    if (!oldMatch) {
        return;
    }
    
    // Validate new PIN (all digits)
    for (int i = 0; i < AGSYS_PIN_LENGTH; i++) {
        if (data[AGSYS_PIN_LENGTH + i] < '0' || data[AGSYS_PIN_LENGTH + i] > '9') {
            return;
        }
    }
    
    // Set new PIN
    char newPin[AGSYS_PIN_LENGTH + 1];
    memcpy(newPin, data + AGSYS_PIN_LENGTH, AGSYS_PIN_LENGTH);
    newPin[AGSYS_PIN_LENGTH] = '\0';
    agsys_ble_auth_set_pin(newPin);
    
    // Notify success
    uint8_t status = AGSYS_AUTH_PIN_CHANGED;
    pinAuthChar.write(&status, 1);
}

// Update authentication status characteristic
static void updateAuthStatus(void) {
    uint8_t status = isAuthenticated ? AGSYS_AUTH_AUTHENTICATED : AGSYS_AUTH_NOT_AUTHENTICATED;
    pinAuthChar.write(&status, 1);
}
