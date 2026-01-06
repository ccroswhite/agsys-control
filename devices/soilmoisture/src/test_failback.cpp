/**
 * @file test_failback.cpp
 * @brief Firmware rollback test modes
 * 
 * Two builds:
 *   test-failback-good (v99.0.0): Normal firmware, marks itself validated
 *   test-failback-bad (v99.1.0): Broken firmware, never validates, triggers rollback
 * 
 * Test procedure:
 *   1. Flash test-failback-good as baseline
 *   2. OTA update to test-failback-bad
 *   3. Bad firmware boots, times out (60s), rolls back
 *   4. Query version via BLE to confirm rollback to v99.0.0
 */

#if defined(TEST_MODE_FAILBACK_GOOD) || defined(TEST_MODE_FAILBACK_BAD)

#include <Arduino.h>
#include <bluefruit.h>

#include "config.h"
#include "security.h"
#include "nvram.h"
#include "debug_log.h"
#include "ble_diagnostics.h"
#include "firmware_backup.h"

// NVRAM instance (needed by debug_log)
NVRAM nvram(PIN_NVRAM_CS);

// BLE DFU Service for OTA
BLEDfu bleDfu;

// Forward declarations
void testFailbackInit();
void testFailbackLoop();
void blinkLED(int count, int onTime, int offTime);

void testFailbackInit() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    
    #ifdef TEST_MODE_FAILBACK_GOOD
    Serial.println("\n========================================");
    Serial.println("  TEST MODE: Failback GOOD (v99.0.0)");
    Serial.println("========================================");
    Serial.println("This is the BASELINE firmware.");
    Serial.println("It will mark itself as validated.");
    Serial.println("========================================\n");
    #else
    Serial.println("\n========================================");
    Serial.println("  TEST MODE: Failback BAD (v99.1.0)");
    Serial.println("========================================");
    Serial.println("This is the BROKEN firmware.");
    Serial.println("It will NOT mark itself as validated.");
    Serial.println("Rollback will occur after 60s timeout.");
    Serial.println("========================================\n");
    #endif
    
    Serial.printf("Device ID: %016llX\n", security_getDeviceId());
    
    // Initialize LED
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_CONN, OUTPUT);
    digitalWrite(PIN_LED_STATUS, HIGH);  // Off
    digitalWrite(PIN_LED_CONN, HIGH);    // Off
    
    // Initialize SPI (required for NVRAM and Flash)
    SPI.begin();
    
    // Initialize NVRAM
    if (!nvram.begin()) {
        Serial.println("NVRAM: Init failed!");
    }
    
    // Initialize firmware backup system
    if (fw_backup_init()) {
        Serial.println("FW Backup: Initialized");
        
        // Check if rollback is needed (previous firmware failed validation)
        if (fw_backup_check_rollback()) {
            // This won't return if rollback is triggered
            Serial.println("FW Backup: Rollback triggered!");
        }
        
        // Check current backup status
        FwBackupHeader header;
        if (fw_backup_get_status(&header)) {
            Serial.printf("FW Backup: Active slot = %c, Rollbacks = %d\n", 
                         'A' + header.activeSlot, header.rollbackCount);
        }
        
        // If validation is pending, start the timer
        if (fw_backup_is_validation_pending()) {
            Serial.println("FW Backup: Validation pending - starting timer");
            fw_backup_start_validation_timer();
        }
    } else {
        Serial.println("FW Backup: Init failed!");
    }
    
    // Initialize debug log (tracks boot count, version, etc.)
    debugLog_init();
    
    Serial.printf("Boot count: %lu\n", debugLog_getBootCount());
    
    char versionStr[16];
    Serial.printf("Firmware version: %s\n", debugLog_getVersionString(versionStr));
    
    char buildStr[24];
    Serial.printf("Build type: %s\n", debugLog_getBuildTypeString(buildStr));
    
    char resetStr[24];
    Serial.printf("Last reset: %s\n", debugLog_getResetReasonString(0, resetStr));
    
    // Initialize BLE
    Bluefruit.begin();
    Bluefruit.setTxPower(4);
    Bluefruit.setName("SM-FAILBACK-TEST");
    
    // Add DFU service
    bleDfu.begin();
    
    // Add diagnostics service (for version query)
    bleDiagnostics.begin();
    
    // Configure advertising
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.addService(bleDfu);
    Bluefruit.Advertising.addService(bleDiagnostics);
    
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(160, 160);
    Bluefruit.Advertising.setFastTimeout(30);
    
    // Start advertising
    Bluefruit.Advertising.start(0);
    
    Serial.println("\nBLE advertising started.");
    Serial.println("Connect to read version via Diagnostics service.\n");
    
    #ifdef TEST_MODE_FAILBACK_GOOD
    // GOOD firmware: Mark as validated immediately
    Serial.println(">>> Marking firmware as VALIDATED <<<");
    fw_backup_validate();  // Stop rollback timer
    debugLog_markValidated();
    Serial.println("Firmware validated. This build will persist.\n");
    #else
    // BAD firmware: Do NOT mark as validated
    Serial.println(">>> NOT marking firmware as validated <<<");
    Serial.println("Rollback will trigger after 60 second timeout...");
    Serial.println("Watch for automatic restore to previous firmware.\n");
    #endif
}

void testFailbackLoop() {
    static unsigned long lastBlinkTime = 0;
    static unsigned long lastStatusTime = 0;
    
    #ifdef TEST_MODE_FAILBACK_BAD
    // BAD firmware: Check validation timeout (will trigger rollback after 60s)
    fw_backup_check_validation_timeout();
    #endif
    
    // Blink pattern every 2 seconds
    if (millis() - lastBlinkTime >= 2000) {
        lastBlinkTime = millis();
        
        #ifdef TEST_MODE_FAILBACK_GOOD
        // GOOD: 1 blink
        blinkLED(1, 200, 0);
        #else
        // BAD: 2 blinks with 500ms gap
        blinkLED(2, 200, 500);
        #endif
    }
    
    // Print status every 10 seconds
    if (millis() - lastStatusTime >= 10000) {
        lastStatusTime = millis();
        
        #ifdef TEST_MODE_FAILBACK_GOOD
        Serial.printf("[%lu] GOOD firmware running (v99.0.0) - validated\n", millis() / 1000);
        #else
        uint32_t elapsed = millis() / 1000;
        uint32_t remaining = (FW_VALIDATION_TIMEOUT_MS / 1000) - elapsed;
        if (remaining > 0 && remaining < 1000) {
            Serial.printf("[%lu] BAD firmware (v99.1.0) - rollback in %lu seconds\n", elapsed, remaining);
        } else {
            Serial.printf("[%lu] BAD firmware (v99.1.0) - waiting for rollback\n", elapsed);
        }
        #endif
        
        // Update BLE diagnostics
        bleDiagnostics.update();
    }
    
    delay(10);
}

void blinkLED(int count, int onTime, int offTime) {
    for (int i = 0; i < count; i++) {
        digitalWrite(PIN_LED_STATUS, LOW);   // On (active low)
        delay(onTime);
        digitalWrite(PIN_LED_STATUS, HIGH);  // Off
        if (i < count - 1 && offTime > 0) {
            delay(offTime);
        }
    }
}

// Arduino entry points
void setup() {
    security_init();
    testFailbackInit();
}

void loop() {
    testFailbackLoop();
}

#endif // TEST_MODE_FAILBACK_GOOD || TEST_MODE_FAILBACK_BAD
