/**
 * @file test_frequency.cpp
 * @brief H-Bridge Frequency Test Build
 * 
 * Test build for evaluating different AC excitation frequencies
 * for soil moisture sensing. Provides BLE API to:
 * - Set frequency (100kHz, 500kHz, 1MHz, or custom)
 * - Trigger measurement
 * - Read results
 * 
 * Build: pio run -e test-frequency
 */

#ifdef TEST_MODE_FREQUENCY

#include <Arduino.h>
#include <bluefruit.h>
#include <SPI.h>

#include "config.h"
#include "security.h"
#include "capacitance.h"
#include "ble_sensor_test.h"

// BLE DFU Service for OTA
BLEDfu bleDfu;

// Forward declarations
void testFrequencyInit();
void testFrequencyLoop();

void setup() {
    security_init();
    testFrequencyInit();
}

void loop() {
    testFrequencyLoop();
}

void testFrequencyInit() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    
    Serial.println("\n========================================");
    Serial.println("  TEST MODE: H-Bridge Frequency Test");
    Serial.println("========================================");
    Serial.printf("Device ID: %016llX\n", security_getDeviceId());
    Serial.println("========================================\n");
    
    // Initialize SPI
    SPI.begin();
    
    // Initialize capacitance measurement hardware
    capacitanceInit();
    
    Serial.printf("Initial frequency: %lu Hz\n", hbridgeGetFrequency());
    
    // Initialize BLE
    Bluefruit.begin();
    Bluefruit.setName("AgSys-FreqTest");
    Bluefruit.setTxPower(4);
    
    // Add DFU service for OTA updates
    bleDfu.begin();
    
    // Add sensor test service
    bleSensorTest.begin();
    
    // Set up advertising
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(bleSensorTest);
    Bluefruit.Advertising.addName();
    
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(160, 320);  // 100-200ms
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
    
    Serial.println("BLE advertising started.");
    Serial.println("\nBLE Characteristics:");
    Serial.println("  - Frequency (R/W): Set H-bridge frequency in Hz");
    Serial.println("  - Trigger (W): Write 0x01 to take measurement");
    Serial.println("  - Result (R/N): Raw ADC value");
    Serial.println("  - Moisture (R/N): Moisture percentage");
    Serial.println("\nPreset frequencies to test:");
    Serial.println("  - 100000 Hz (100 kHz) - default");
    Serial.println("  - 500000 Hz (500 kHz)");
    Serial.println("  - 1000000 Hz (1 MHz)");
    Serial.println("  - 2000000 Hz (2 MHz)");
    Serial.println("\nReady for BLE connection.\n");
}

void testFrequencyLoop() {
    // Nothing to do - all interaction via BLE callbacks
    delay(100);
}

#endif // TEST_MODE_FREQUENCY
