/**
 * @file main.cpp
 * @brief Main Application for Soil Moisture Sensor IoT Device
 * 
 * Target: Nordic nRF52832 Microcontroller (Arduino Framework)
 * LoRa Module: HOPERF RFM95C
 * BLE: Built-in for OTA firmware updates
 * 
 * Operation:
 * 1. Wake from deep sleep (RTC triggered)
 * 2. Check for OTA button press - if pressed, enable BLE DFU mode
 * 3. Read soil moisture sensor
 * 4. Read battery voltage
 * 5. Transmit data via LoRa to leader
 * 6. Wait for ACK (optional)
 * 7. Log data locally if transmission fails
 * 8. Return to deep sleep
 */

// Skip this file in test modes (test files provide their own setup/loop)
#if defined(TEST_MODE_CYCLE_READINGS) || defined(TEST_MODE_POWER_ALL) || defined(TEST_MODE_FAILBACK_GOOD) || defined(TEST_MODE_FAILBACK_BAD)
// Empty - test mode files handle everything
#else

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <bluefruit.h>

#include "config.h"
#include "protocol.h"
#include "nvram.h"
#include "capacitance.h"
#include "ota_lora.h"
#include "security.h"
#include "firmware_backup.h"
#include "debug_log.h"

// BLE DFU Service
BLEDfu bleDfu;

// Global objects
NVRAM nvram(PIN_NVRAM_CS);
Protocol protocol;

// Device state
static uint8_t deviceUUID[16];
static bool firstBoot = true;
static uint32_t bootCount = 0;
static uint32_t uptimeSeconds = 0;

// Sensor readings
static uint16_t moistureRaw = 0;
static uint8_t moisturePercent = 0;
static uint16_t batteryMv = 0;
static int16_t temperature = 0;

// OTA mode state
static bool otaModeActive = false;
static unsigned long otaModeStartTime = 0;

// Function prototypes
void systemInit();
void initBLE();
void checkOTAButton();
void enterOTAMode();
bool loadOrGenerateUUID();
void readSensors();
bool transmitData();
void logDataLocally(bool txSuccess);
void enterDeepSleep();
void handlePendingLogs();
void onWakeup();

// ADC helpers
uint16_t readBatteryVoltage();
uint16_t readMoistureRaw();
uint8_t moistureToPercent(uint16_t raw);

// LED helpers
void ledStatusBlink();
void ledSpiOn();
void ledSpiOff();

// BLE callbacks
void bleConnectCallback(uint16_t conn_handle);
void bleDisconnectCallback(uint16_t conn_handle, uint8_t reason);

/**
 * @brief Arduino setup function
 */
void setup() {
    // Initialize security first - enables APPROTECT in release builds
    // In release builds, this may reset the device on first boot
    security_init();
    
    #if DEBUG_MODE
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Wait up to 3 seconds for serial
    DEBUG_PRINTLN("\n=== Soil Moisture Sensor v1.1 (nRF52832) ===");
    DEBUG_PRINTF("Device ID: %016llX\n", security_getDeviceId());
    #endif
    
    systemInit();
    
    // Initialize firmware backup system early
    if (fw_backup_init()) {
        // Check if previous firmware failed validation (triggers rollback if needed)
        if (fw_backup_check_rollback()) {
            // Rollback was triggered - fw_backup_restore() does not return
            // If we get here, rollback failed
            DEBUG_PRINTLN("FW Backup: Rollback failed!");
        }
        
        // If validation is pending from a previous OTA, start the timer
        if (fw_backup_is_validation_pending()) {
            DEBUG_PRINTLN("FW Backup: Resuming validation timer");
            fw_backup_start_validation_timer();
        }
    }
    
    // Initialize debug log (also increments boot count)
    debugLog_init();
}

/**
 * @brief Arduino main loop
 */
void loop() {
    // Check firmware validation timeout (triggers rollback if expired)
    fw_backup_check_validation_timeout();
    
    // Check if OTA button is pressed
    checkOTAButton();
    
    // If in OTA mode, handle BLE and wait for update
    if (otaModeActive) {
        // Check if OTA window has expired
        if (millis() - otaModeStartTime > BLE_OTA_WINDOW_MS) {
            DEBUG_PRINTLN("OTA: Window expired, resuming normal operation");
            otaModeActive = false;
            Bluefruit.Advertising.stop();
            digitalWrite(PIN_LED_CONN, LOW);
        }
        // Stay awake and let BLE handle DFU
        delay(100);
        return;
    }
    
    DEBUG_PRINTLN("\n--- Wake cycle start ---");
    
    // Blink green LED to indicate system is functional
    ledStatusBlink();
    
    // Read all sensors
    readSensors();
    
    DEBUG_PRINT("Moisture: ");
    DEBUG_PRINT(moisturePercent);
    DEBUG_PRINT("% (raw: ");
    DEBUG_PRINT(moistureRaw);
    DEBUG_PRINTLN(")");
    
    DEBUG_PRINT("Battery: ");
    DEBUG_PRINT(batteryMv);
    DEBUG_PRINTLN(" mV");
    
    // Attempt to transmit data
    bool txSuccess = transmitData();
    
    if (txSuccess) {
        DEBUG_PRINTLN("TX: Success");
        
        // Successful TX proves firmware is working - validate it
        // This stops the rollback timer if it was running
        if (fw_backup_is_validation_pending()) {
            DEBUG_PRINTLN("FW Backup: Firmware validated (successful TX)");
            fw_backup_validate();
        }
        
        // Wait for ACK
        uint8_t rxBuffer[64];
        int packetSize = 0;
        unsigned long startTime = millis();
        
        while (millis() - startTime < LORA_RX_TIMEOUT_MS) {
            packetSize = LoRa.parsePacket();
            if (packetSize > 0) {
                break;
            }
        }
        
        if (packetSize > 0) {
            // Read packet
            uint8_t rxLen = 0;
            while (LoRa.available() && rxLen < sizeof(rxBuffer)) {
                rxBuffer[rxLen++] = LoRa.read();
            }
            
            // Parse received packet
            PacketHeader header;
            uint8_t payload[48];
            if (protocol.parse(rxBuffer, rxLen, &header, payload)) {
                // Check for OTA messages first
                if (header.msgType >= MSG_TYPE_OTA_ANNOUNCE && 
                    header.msgType <= MSG_TYPE_OTA_STATUS) {
                    otaLora.processMessage(header.msgType, payload, header.payloadLen);
                }
                else if (header.msgType == MSG_TYPE_ACK) {
                    DEBUG_PRINTLN("RX: ACK received");
                    AckPayload* ack = (AckPayload*)payload;
                    
                    // Check if controller wants pending logs
                    if (ack->flags & ACK_FLAG_SEND_LOGS) {
                        handlePendingLogs();
                    }
                    
                    // Log success
                    logDataLocally(true);
                }
            }
        } else {
            DEBUG_PRINTLN("RX: No ACK (timeout)");
            logDataLocally(false);
        }
    } else {
        DEBUG_PRINTLN("TX: Failed");
        logDataLocally(false);
    }
    
    // Check for LoRa OTA updates in progress
    otaLora.update();
    
    // If OTA firmware is ready to apply, do it now
    if (otaLora.isReadyToApply()) {
        DEBUG_PRINTLN("OTA: Applying firmware update...");
        
        // Create backup of current firmware before applying update
        // Use a conservative estimate of firmware size (256KB max app size)
        // The actual backup will only store used pages
        uint32_t fwSize = 256 * 1024;  // 256KB - max application size
        
        if (fw_backup_create(fwSize)) {
            DEBUG_PRINTLN("OTA: Backup created successfully");
            // Start validation timer - new firmware must validate within timeout
            fw_backup_start_validation_timer();
        } else {
            DEBUG_PRINTLN("OTA: WARNING - Backup failed, proceeding anyway");
        }
        
        otaLora.applyUpdate();  // Does not return
    }
    
    // Update uptime
    uptimeSeconds += SLEEP_INTERVAL_MS / 1000;
    protocol.updateUptime(uptimeSeconds);
    
    // Enter deep sleep
    enterDeepSleep();
    
    // Execution continues here after wake
    firstBoot = false;
}

/**
 * @brief Initialize all system components
 */
void systemInit() {
    // Enable DC-DC converter for lower power consumption
    // Reduces active current by ~20% (from ~5mA to ~4mA)
    NRF_POWER->DCDCEN = 1;
    
    // Configure LED pins (active LOW - HIGH = off)
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, HIGH);  // Green status LED off
    
    pinMode(PIN_LED_SPI, OUTPUT);
    digitalWrite(PIN_LED_SPI, HIGH);     // Yellow SPI activity LED off
    
    pinMode(PIN_LED_CONN, OUTPUT);
    digitalWrite(PIN_LED_CONN, HIGH);    // Blue BLE connection LED off
    
    pinMode(PIN_MOISTURE_POWER, OUTPUT);
    digitalWrite(PIN_MOISTURE_POWER, LOW);
    
    pinMode(PIN_OTA_BUTTON, INPUT_PULLUP);
    
    // Set ADC resolution (nRF52 supports up to 14-bit, using 12-bit)
    analogReadResolution(ADC_RESOLUTION_BITS);
    
    // Initialize capacitance measurement hardware (H-bridge, Timer, PPI)
    capacitanceInit();
    DEBUG_PRINTLN("Capacitance: H-bridge initialized");
    
    // Initialize SPI (each device driver manages its own speed via beginTransaction)
    SPI.begin();
    
    // Initialize NVRAM
    if (!nvram.begin()) {
        DEBUG_PRINTLN("NVRAM: Init failed (continuing without logging)");
    }
    
    // Load or generate device UUID
    if (!loadOrGenerateUUID()) {
        DEBUG_PRINTLN("UUID: Using default");
        memset(deviceUUID, 0, sizeof(deviceUUID));
        deviceUUID[0] = DEVICE_TYPE_SOIL_MOISTURE;
    }
    
    // Initialize protocol
    protocol.init(deviceUUID);
    
    // Initialize LoRa OTA system
    otaLora.init(deviceUUID);
    DEBUG_PRINTLN("OTA: LoRa OTA initialized");
    
    DEBUG_PRINTF("Device ID: %016llX\n", security_getDeviceId());
    
    // Initialize LoRa
    LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
    
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("LoRa: Init failed!");
        // Blink LED to indicate error
        for (int i = 0; i < 10; i++) {
            digitalWrite(PIN_LED_STATUS, HIGH);
            delay(100);
            digitalWrite(PIN_LED_STATUS, LOW);
            delay(100);
        }
    } else {
        DEBUG_PRINTLN("LoRa: Init OK");
        
        // Configure LoRa parameters
        LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
        LoRa.setSignalBandwidth(LORA_BANDWIDTH);
        LoRa.setCodingRate4(LORA_CODING_RATE);
        LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
        LoRa.setTxPower(LORA_TX_POWER_DBM);
        LoRa.setSyncWord(LORA_SYNC_WORD);
        LoRa.enableCrc();
    }
    
    // Initialize BLE for OTA updates
    initBLE();
    
    DEBUG_PRINTLN("System initialized");
}

/**
 * @brief BLE security callbacks
 */
bool blePairingPasskeyCallback(uint16_t conn_handle, uint8_t const passkey[6], bool match_request) {
    DEBUG_PRINTF("BLE: Passkey displayed: %c%c%c%c%c%c\n", 
                 passkey[0], passkey[1], passkey[2], 
                 passkey[3], passkey[4], passkey[5]);
    
    // For passkey display, always return true
    // User must enter this passkey on the central device
    return true;
}

void bleSecurityCallback(uint16_t conn_handle, ble_gap_sec_params_t* params) {
    DEBUG_PRINTLN("BLE: Security request received");
}

void bleSecuredCallback(uint16_t conn_handle) {
    DEBUG_PRINTLN("BLE: Connection secured (encrypted)");
}

void blePairingCompleteCallback(uint16_t conn_handle, uint8_t auth_status) {
    if (auth_status == BLE_GAP_SEC_STATUS_SUCCESS) {
        DEBUG_PRINTLN("BLE: Pairing successful");
    } else {
        DEBUG_PRINTF("BLE: Pairing failed, status=0x%02X\n", auth_status);
    }
}

/**
 * @brief Initialize BLE for OTA DFU with LESC security
 */
void initBLE() {
    Bluefruit.begin();
    Bluefruit.setTxPower(4);  // Max power for reliable OTA
    Bluefruit.setName(BLE_DEVICE_NAME);
    
    // Configure BLE Security (LESC with passkey display)
    // This provides strong encryption with MITM protection
    Bluefruit.Security.setIOCaps(true, false, false);  // Display only (show passkey)
    Bluefruit.Security.setMITM(true);                  // Require MITM protection
    Bluefruit.Security.setPairPasskeyCallback(blePairingPasskeyCallback);
    Bluefruit.Security.setSecuredCallback(bleSecuredCallback);
    Bluefruit.Security.setPairCompleteCallback(blePairingCompleteCallback);
    
    // Set connection callbacks
    Bluefruit.Periph.setConnectCallback(bleConnectCallback);
    Bluefruit.Periph.setDisconnectCallback(bleDisconnectCallback);
    
    // Add DFU service for OTA updates
    bleDfu.begin();
    
    // Configure advertising (but don't start yet)
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.addService(bleDfu);
    
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(160, 160);  // 100ms interval
    Bluefruit.Advertising.setFastTimeout(30);
    
    DEBUG_PRINTLN("BLE: Initialized with LESC security (OTA ready)");
}

/**
 * @brief Check if OTA button is pressed and enter OTA mode
 */
void checkOTAButton() {
    if (digitalRead(PIN_OTA_BUTTON) == LOW) {
        // Debounce
        delay(50);
        if (digitalRead(PIN_OTA_BUTTON) == LOW) {
            enterOTAMode();
        }
    }
}

/**
 * @brief Enter BLE OTA mode
 */
void enterOTAMode() {
    if (otaModeActive) return;
    
    DEBUG_PRINTLN("OTA: Entering OTA mode");
    otaModeActive = true;
    otaModeStartTime = millis();
    
    // Visual indicator - blink then solid
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_LED_CONN, HIGH);
        delay(100);
        digitalWrite(PIN_LED_CONN, LOW);
        delay(100);
    }
    digitalWrite(PIN_LED_CONN, HIGH);
    
    // Start BLE advertising
    Bluefruit.Advertising.start(0);  // Advertise indefinitely until connected
    
    DEBUG_PRINTLN("OTA: BLE advertising started");
    DEBUG_PRINT("OTA: Window open for ");
    DEBUG_PRINT(BLE_OTA_WINDOW_MS / 1000);
    DEBUG_PRINTLN(" seconds");
}

/**
 * @brief BLE connect callback
 */
void bleConnectCallback(uint16_t conn_handle) {
    DEBUG_PRINTLN("BLE: Connected");
    // Keep LED on solid during connection
    digitalWrite(PIN_LED_CONN, HIGH);
}

/**
 * @brief BLE disconnect callback
 */
void bleDisconnectCallback(uint16_t conn_handle, uint8_t reason) {
    DEBUG_PRINT("BLE: Disconnected, reason: ");
    DEBUG_PRINTLN(reason);
    
    // If still in OTA mode, keep advertising
    if (otaModeActive) {
        digitalWrite(PIN_LED_CONN, HIGH);
    } else {
        digitalWrite(PIN_LED_CONN, LOW);
    }
}

/**
 * @brief Load UUID from NVRAM or generate a new one
 */
bool loadOrGenerateUUID() {
    // Device identity now comes from FICR - no UUID generation needed
    // Copy FICR device ID to deviceUUID for backward compatibility with protocol
    security_getDeviceIdBytes(deviceUUID);
    // Pad remaining bytes with device type
    for (int i = 8; i < 16; i++) {
        deviceUUID[i] = (i == 8) ? DEVICE_TYPE_SOIL_MOISTURE : 0;
    }
    DEBUG_PRINTLN("Device ID loaded from FICR");
    return true;
}

/**
 * @brief Read all sensors
 */
void readSensors() {
    // Read soil moisture
    moistureRaw = readMoistureRaw();
    moisturePercent = moistureToPercent(moistureRaw);
    
    // Read battery voltage
    batteryMv = readBatteryVoltage();
    
    // Temperature - use internal sensor if available, otherwise 0
    temperature = 0;  // TODO: Implement if needed
}

/**
 * @brief Read battery voltage
 */
uint16_t readBatteryVoltage() {
    // On Feather M0, A7 is connected to VBAT through a voltage divider
    // The divider gives VBAT/2
    uint16_t raw = analogRead(PIN_BATTERY_ANALOG);
    
    // Convert to millivolts
    // raw / 4095 * 3300 * 2 (for divider)
    uint32_t mv = ((uint32_t)raw * ADC_REFERENCE_MV * BATTERY_DIVIDER_RATIO) / ADC_MAX_VALUE;
    
    return (uint16_t)mv;
}

/**
 * @brief Read moisture sensor using AC capacitance method
 * 
 * Uses 100kHz H-bridge drive with 1-second averaging for high fidelity.
 */
uint16_t readMoistureRaw() {
    DEBUG_PRINTLN("Moisture: Starting AC capacitance measurement...");
    
    // Use the capacitance module for measurement
    uint16_t raw = readCapacitance();
    
    DEBUG_PRINT("Moisture: Raw ADC = ");
    DEBUG_PRINTLN(raw);
    
    return raw;
}

/**
 * @brief Convert raw moisture reading to percentage
 */
uint8_t moistureToPercent(uint16_t raw) {
    // Use capacitance module's conversion function
    return capacitanceToMoisturePercent(raw);
}

/* ==========================================================================
 * LED FUNCTIONS
 * LEDs are active LOW (GPIO LOW = LED ON, GPIO HIGH = LED OFF)
 * ========================================================================== */

/**
 * @brief Blink green status LED to indicate system is functional
 */
void ledStatusBlink() {
    digitalWrite(PIN_LED_STATUS, LOW);   // LED ON
    delay(50);
    digitalWrite(PIN_LED_STATUS, HIGH);  // LED OFF
}

/**
 * @brief Turn on SPI activity LED
 */
void ledSpiOn() {
    digitalWrite(PIN_LED_SPI, LOW);
}

/**
 * @brief Turn off SPI activity LED
 */
void ledSpiOff() {
    digitalWrite(PIN_LED_SPI, HIGH);
}

/**
 * @brief Transmit sensor data via LoRa
 */
bool transmitData() {
    uint8_t txBuffer[64];
    uint8_t flags = 0;
    
    // Set status flags
    if (batteryMv < BATTERY_LOW_THRESHOLD_MV) {
        flags |= REPORT_FLAG_LOW_BATTERY;
    }
    if (firstBoot) {
        flags |= REPORT_FLAG_FIRST_BOOT;
    }
    if (nvram.logPendingCount() > 0) {
        flags |= REPORT_FLAG_HAS_PENDING;
    }
    
    // Build sensor report packet
    uint8_t packetLen = protocol.buildSensorReport(
        txBuffer,
        sizeof(txBuffer),
        moistureRaw,
        moisturePercent,
        batteryMv,
        temperature,
        nvram.logPendingCount(),
        flags
    );
    
    if (packetLen == 0) {
        DEBUG_PRINTLN("TX: Packet build failed");
        return false;
    }
    
    // Turn on SPI activity LED during transmission
    ledSpiOn();
    
    // Attempt transmission with retries
    for (uint8_t retry = 0; retry < LORA_MAX_RETRIES; retry++) {
        DEBUG_PRINT("TX: Attempt ");
        DEBUG_PRINTLN(retry + 1);
        
        LoRa.beginPacket();
        LoRa.write(txBuffer, packetLen);
        
        if (LoRa.endPacket()) {
            ledSpiOff();
            return true;
        }
        
        delay(LORA_RETRY_DELAY_MS);
    }
    
    ledSpiOff();
    return false;
}

/**
 * @brief Log sensor data to local NVRAM
 */
void logDataLocally(bool txSuccess) {
    LogEntry entry;
    
    entry.timestamp = uptimeSeconds;
    entry.moistureRaw = moistureRaw;
    entry.moisturePercent = moisturePercent;
    entry.batteryMv = batteryMv;
    entry.flags = 0;
    
    if (txSuccess) {
        entry.flags |= LOG_FLAG_TX_SUCCESS;
    } else {
        entry.flags |= LOG_FLAG_TX_PENDING;
    }
    
    if (batteryMv < BATTERY_LOW_THRESHOLD_MV) {
        entry.flags |= LOG_FLAG_LOW_BATTERY;
    }
    
    memset(entry.reserved, 0, sizeof(entry.reserved));
    
    if (nvram.logAppend(&entry)) {
        DEBUG_PRINT("Log: Entry saved, pending = ");
        DEBUG_PRINTLN(nvram.logPendingCount());
    }
}

/**
 * @brief Send pending log entries to leader
 */
void handlePendingLogs() {
    uint16_t pending = nvram.logPendingCount();
    
    if (pending == 0) {
        return;
    }
    
    DEBUG_PRINT("Logs: Sending ");
    DEBUG_PRINT(pending);
    DEBUG_PRINTLN(" pending entries");
    
    // Send up to 4 entries per batch
    uint16_t batchSize = (pending > 4) ? 4 : pending;
    
    // TODO: Implement log batch transmission
    // For now, just mark as transmitted
    nvram.logMarkTransmitted(batchSize);
}

/**
 * @brief Enter deep sleep mode (nRF52 System ON sleep with RTC wake)
 */
void enterDeepSleep() {
    DEBUG_PRINTLN("Entering deep sleep...");
    
    #if DEBUG_MODE
    Serial.flush();
    #endif
    
    // Put LoRa module to sleep
    LoRa.sleep();
    
    // Put NVRAM to sleep
    nvram.sleep();
    
    // Turn off LEDs
    digitalWrite(PIN_LED_STATUS, LOW);
    digitalWrite(PIN_LED_CONN, LOW);
    
    // Turn off moisture sensor power
    digitalWrite(PIN_MOISTURE_POWER, LOW);
    
    // Calculate sleep duration
    uint32_t sleepMs = SLEEP_INTERVAL_MS;
    
    // Extend sleep if battery is critical
    if (batteryMv < BATTERY_CRITICAL_MV) {
        sleepMs *= CRITICAL_SLEEP_MULTIPLIER;
        DEBUG_PRINTLN("Battery critical - extended sleep");
    }
    
    // nRF52 uses SoftDevice for sleep management
    // suspendLoop() + delay() puts device in System ON sleep mode
    // which draws ~1.9µA with RTC running
    suspendLoop();
    delay(sleepMs);
    resumeLoop();
    
    // === Execution resumes here after wake ===
    
    // Wake NVRAM
    nvram.wake();
    
    // LoRa will be re-initialized on next TX
    LoRa.idle();
}

/**
 * @brief Callback for wake interrupt (button or RTC)
 */
void onWakeup() {
    // This is called in interrupt context
    // Keep it minimal - just set a flag if needed
}

#endif // !TEST_MODE_CYCLE_READINGS
