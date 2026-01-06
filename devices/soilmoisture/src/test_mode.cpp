/**
 * @file test_mode.cpp
 * @brief Test mode for cycling through synthetic sensor readings
 * 
 * Build with: pio run -e test-cycle-readings
 * 
 * This replaces the normal sensor reading and sleep cycle with a
 * continuous loop that sends synthetic data for controller testing.
 */

#ifdef TEST_MODE_CYCLE_READINGS

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#include "config.h"
#include "protocol.h"
#include "security.h"
#include "lora_crypto.h"

// Default test parameters (can be overridden in platformio.ini)
#ifndef TEST_TX_INTERVAL_MS
#define TEST_TX_INTERVAL_MS 5000
#endif

#ifndef TEST_MOISTURE_STEP
#define TEST_MOISTURE_STEP 5
#endif

// Test state
static uint8_t testMoisturePercent = 0;
static bool testMoistureIncreasing = true;
static uint32_t testPacketCount = 0;
static uint32_t testUptimeSeconds = 0;

// Protocol instance
static Protocol testProtocol;

// Forward declarations
void testModeInit();
void testModeLoop();
void testTransmitData();
uint16_t percentToRaw(uint8_t percent);

/**
 * @brief Initialize test mode
 */
void testModeInit() {
    // Initialize serial
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    
    Serial.println("\n========================================");
    Serial.println("  TEST MODE: Cycle Readings");
    Serial.println("========================================");
    Serial.printf("Device ID: %016llX\n", security_getDeviceId());
    Serial.printf("TX Interval: %d ms\n", TEST_TX_INTERVAL_MS);
    Serial.printf("Moisture Step: %d%%\n", TEST_MOISTURE_STEP);
    Serial.println("========================================\n");
    
    // Initialize SPI
    SPI.begin();
    
    // Initialize LoRa
    LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
    
    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("ERROR: LoRa init failed!");
        while (1) {
            delay(1000);
        }
    }
    
    // Configure LoRa parameters
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setTxPower(LORA_TX_POWER_DBM);
    LoRa.enableCrc();
    
    Serial.println("LoRa: Initialized");
    
    // Initialize crypto
    lora_crypto_init();
    Serial.println("Crypto: Initialized");
    
    // Initialize protocol with device ID as UUID
    uint8_t deviceId[8];
    security_getDeviceIdBytes(deviceId);
    
    // Create a 16-byte UUID from 8-byte device ID (duplicate it)
    uint8_t uuid[16];
    memcpy(uuid, deviceId, 8);
    memcpy(uuid + 8, deviceId, 8);
    testProtocol.init(uuid);
    
    Serial.println("\nStarting test transmission cycle...\n");
}

/**
 * @brief Main test mode loop
 */
void testModeLoop() {
    static unsigned long lastTxTime = 0;
    
    // Check if it's time to transmit
    if (millis() - lastTxTime >= TEST_TX_INTERVAL_MS) {
        lastTxTime = millis();
        
        // Transmit current test data
        testTransmitData();
        
        // Update moisture value (cycle 0 -> 100 -> 0)
        if (testMoistureIncreasing) {
            testMoisturePercent += TEST_MOISTURE_STEP;
            if (testMoisturePercent >= 100) {
                testMoisturePercent = 100;
                testMoistureIncreasing = false;
            }
        } else {
            if (testMoisturePercent <= TEST_MOISTURE_STEP) {
                testMoisturePercent = 0;
                testMoistureIncreasing = true;
            } else {
                testMoisturePercent -= TEST_MOISTURE_STEP;
            }
        }
        
        // Update simulated uptime
        testUptimeSeconds += TEST_TX_INTERVAL_MS / 1000;
        testProtocol.updateUptime(testUptimeSeconds);
    }
    
    // Small delay to prevent busy-waiting
    delay(10);
}

/**
 * @brief Transmit synthetic sensor data
 */
void testTransmitData() {
    uint8_t txBuffer[64];
    uint8_t encryptedBuffer[LORA_MAX_PACKET];
    size_t encryptedLen;
    
    // Generate synthetic values
    uint16_t moistureRaw = percentToRaw(testMoisturePercent);
    uint16_t batteryMv = 3700 + (testPacketCount % 100);  // Simulate slight variation
    int16_t temperature = 220 + (testPacketCount % 50);   // ~22.0°C with variation
    uint8_t flags = 0;
    
    if (testPacketCount == 0) {
        flags |= REPORT_FLAG_FIRST_BOOT;
    }
    
    // Build sensor report packet
    uint8_t packetLen = testProtocol.buildSensorReport(
        txBuffer,
        sizeof(txBuffer),
        moistureRaw,
        testMoisturePercent,
        batteryMv,
        temperature,
        0,  // No pending logs
        flags
    );
    
    if (packetLen == 0) {
        Serial.println("ERROR: Packet build failed");
        return;
    }
    
    // Encrypt the packet
    if (!lora_crypto_encrypt(txBuffer, packetLen, encryptedBuffer, &encryptedLen)) {
        Serial.println("ERROR: Encryption failed");
        return;
    }
    
    // Transmit
    LoRa.beginPacket();
    LoRa.write(encryptedBuffer, encryptedLen);
    
    if (LoRa.endPacket()) {
        testPacketCount++;
        
        Serial.printf("[%06lu] TX #%lu: Moisture=%3d%% (raw=%4d), Batt=%dmV, Temp=%.1f°C\n",
                      millis() / 1000,
                      testPacketCount,
                      testMoisturePercent,
                      moistureRaw,
                      batteryMv,
                      temperature / 10.0);
    } else {
        Serial.println("ERROR: LoRa transmission failed");
    }
}

/**
 * @brief Convert moisture percentage to simulated raw ADC value
 */
uint16_t percentToRaw(uint8_t percent) {
    // Simulate ADC range based on calibration values
    // Dry = high ADC, Wet = low ADC (typical for capacitive sensors)
    uint16_t dryValue = MOISTURE_DRY_VALUE;
    uint16_t wetValue = MOISTURE_WET_VALUE;
    
    // Linear interpolation
    return dryValue - ((dryValue - wetValue) * percent / 100);
}

// Override Arduino setup/loop for test mode
void setup() {
    security_init();
    testModeInit();
}

void loop() {
    testModeLoop();
}

#endif // TEST_MODE_CYCLE_READINGS
