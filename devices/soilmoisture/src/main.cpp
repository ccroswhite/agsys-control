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
#if defined(TEST_MODE_CYCLE_READINGS) || defined(TEST_MODE_POWER_ALL) || defined(TEST_MODE_FAILBACK_GOOD) || defined(TEST_MODE_FAILBACK_BAD) || defined(TEST_MODE_FREQUENCY)
// Empty - test mode files handle everything
#else

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_FRAM_SPI.h>

#include "config.h"
#include "protocol.h"
#include "nvram.h"
#include "moisture_probe.h"
#include "moisture_cal.h"
#include "auto_calibration.h"
#include "ota_lora.h"
#include "security.h"
#include "firmware_backup.h"
#include "debug_log.h"
#include "agsys_ble.h"

// Global objects
NVRAM nvram(PIN_NVRAM_CS);
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(PIN_NVRAM_CS);  // For shared BLE library
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

// BLE/Pairing mode state
static bool bleInitialized = false;
static bool pairingModeActive = false;
static unsigned long pairingModeStartTime = 0;

// Wake source tracking
static volatile bool buttonWakeFlag = false;

// Function prototypes
void systemInit();
void initBLE();
bool checkPairingButton();
void enterPairingMode();
void exitPairingMode();
void initBLEIfNeeded();
bool loadOrGenerateUUID();
void readSensors();
bool transmitData();
void logDataLocally(bool txSuccess);
void enterDeepSleep(uint32_t remainingSleepMs = 0);
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
void onBleCalCommand(AgsysBleCalCmd_t* cmd);
bool isButtonHeld(uint32_t holdTimeMs);
void buttonWakeISR();

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
    
    // If in pairing mode, handle BLE and wait for connection
    if (pairingModeActive) {
        // Check if pairing window has expired
        if (millis() - pairingModeStartTime > BLE_PAIRING_TIMEOUT_MS) {
            DEBUG_PRINTLN("Pairing: Window expired, entering sleep");
            exitPairingMode();
            return;
        }
        
        // Check if button pressed to manually exit pairing mode
        if (digitalRead(PIN_PAIRING_BUTTON) == LOW) {
            delay(50);  // Debounce
            if (digitalRead(PIN_PAIRING_BUTTON) == LOW) {
                DEBUG_PRINTLN("Pairing: Button pressed, exiting pairing mode");
                // Wait for button release to avoid re-triggering
                while (digitalRead(PIN_PAIRING_BUTTON) == LOW) {
                    delay(10);
                }
                exitPairingMode();
                return;
            }
        }
        
        // Stay awake and let BLE handle connections
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
    digitalWrite(PIN_LED_STATUS, LOW);  // Green status LED off
    
    // Probe power is controlled by moisture_probe module
    pinMode(PIN_PROBE_POWER, OUTPUT);
    digitalWrite(PIN_PROBE_POWER, HIGH);  // P-FET off (active low)
    
    pinMode(PIN_OTA_BUTTON, INPUT_PULLUP);
    
    // Set ADC resolution (nRF52 supports up to 14-bit, using 12-bit)
    analogReadResolution(ADC_RESOLUTION_BITS);
    
    // Initialize moisture probe hardware (oscillator frequency measurement)
    moistureProbe_init();
    moistureCal_init();
    autoCal_init();
    DEBUG_PRINTLN("MoistureProbe: Initialized");
    
    // Check if first boot calibration is needed
    if (autoCal_needed()) {
        DEBUG_PRINTLN("MoistureProbe: First boot - running f_air calibration");
        autoCal_runAll();
    }
    
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
    
    // BLE is NOT initialized here - only when entering pairing mode
    // This saves power during normal sensor operation
    
    // Check if button is held at boot (2 seconds) to enter pairing mode
    if (isButtonHeld(PAIRING_BUTTON_HOLD_MS)) {
        DEBUG_PRINTLN("Button held at boot - entering pairing mode");
        enterPairingMode();
    }
    // Check if first boot calibration is needed
    else if (autoCal_needed()) {
        DEBUG_PRINTLN("First boot - running f_air calibration with BLE");
        // Initialize BLE so user can review calibration values
        initBLEIfNeeded();
        autoCal_runAll();
        enterPairingMode();  // Stay in pairing mode for field calibration
    }
    
    DEBUG_PRINTLN("System initialized");
}


/**
 * @brief Initialize BLE using shared agsys_ble library
 */
void initBLE() {
    // Initialize unified BLE service
    agsys_ble_init(AGSYS_BLE_DEVICE_NAME, AGSYS_DEVICE_TYPE_SOIL_MOISTURE, AGSYS_BLE_FRAM_PIN_ADDR,
                   FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
    agsys_ble_set_cal_callback(onBleCalCommand);
    
    DEBUG_PRINTLN("BLE: Initialized with shared agsys_ble library");
}

/**
 * @brief Check if button is held for specified duration
 * @param holdTimeMs Time button must be held (milliseconds)
 * @return true if button was held for the full duration
 */
bool isButtonHeld(uint32_t holdTimeMs) {
    // Check if button is currently pressed
    if (digitalRead(PIN_PAIRING_BUTTON) != LOW) {
        return false;
    }
    
    // Debounce
    delay(50);
    if (digitalRead(PIN_PAIRING_BUTTON) != LOW) {
        return false;
    }
    
    // Wait for hold duration, checking button state
    uint32_t startTime = millis();
    while (millis() - startTime < holdTimeMs) {
        if (digitalRead(PIN_PAIRING_BUTTON) != LOW) {
            return false;  // Button released early
        }
        // Blink LED to indicate button is being held
        digitalWrite(PIN_LED_STATUS, ((millis() / 250) % 2) ? HIGH : LOW);
        delay(10);
    }
    
    digitalWrite(PIN_LED_STATUS, LOW);
    return true;
}

/**
 * @brief Check if pairing button is pressed (called during wake)
 * @return true if button held for required duration
 */
bool checkPairingButton() {
    return isButtonHeld(PAIRING_BUTTON_HOLD_MS);
}

/**
 * @brief Initialize BLE stack if not already initialized
 */
void initBLEIfNeeded() {
    if (bleInitialized) return;
    
    initBLE();
    bleInitialized = true;
}

/**
 * @brief Enter BLE pairing mode
 */
void enterPairingMode() {
    if (pairingModeActive) return;
    
    DEBUG_PRINTLN("Pairing: Entering pairing mode");
    
    // Initialize BLE if not already done
    initBLEIfNeeded();
    
    pairingModeActive = true;
    pairingModeStartTime = millis();
    
    // Visual indicator - blink then solid
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_LED_STATUS, HIGH);
        delay(100);
        digitalWrite(PIN_LED_STATUS, LOW);
        delay(100);
    }
    digitalWrite(PIN_LED_STATUS, HIGH);
        
    // Start BLE advertising using shared library
    agsys_ble_start_advertising();
        
    DEBUG_PRINTLN("Pairing: BLE advertising started");
    DEBUG_PRINT("Pairing: Window open for ");
    DEBUG_PRINT(BLE_PAIRING_TIMEOUT_MS / 1000);
    DEBUG_PRINTLN(" seconds");
}

/**
 * @brief Exit BLE pairing mode and enter deep sleep
 */
void exitPairingMode() {
    pairingModeActive = false;
    
    if (bleInitialized) {
        agsys_ble_stop_advertising();
    }
    
    digitalWrite(PIN_LED_STATUS, LOW);
    DEBUG_PRINTLN("Pairing: Exited pairing mode");
    
    enterDeepSleep();
}

/**
 * @brief BLE calibration command callback
 */
void onBleCalCommand(AgsysBleCalCmd_t* cmd) {
    DEBUG_PRINTF("BLE: Cal command %d, probe %d\n", cmd->command, cmd->probeIndex);
    
    // Get current reading for the specified probe (500ms measurement)
    uint32_t freq = moistureProbe_measureFrequency(cmd->probeIndex, 500);
    
    switch (cmd->command) {
        case AGSYS_CAL_CMD_CAPTURE_AIR:
            // Capture air reading (probe in air)
            if (moistureCal_setAir(cmd->probeIndex, freq)) {
                DEBUG_PRINTF("Calibration: Air captured f_air=%lu Hz\n", freq);
            }
            break;
            
        case AGSYS_CAL_CMD_CAPTURE_DRY:
            // Capture dry soil reading
            if (moistureCal_setDry(cmd->probeIndex, freq)) {
                DEBUG_PRINTF("Calibration: Dry captured f_dry=%lu Hz\n", freq);
            }
            break;
            
        case AGSYS_CAL_CMD_CAPTURE_WET:
            // Capture wet soil reading
            if (moistureCal_setWet(cmd->probeIndex, freq)) {
                DEBUG_PRINTF("Calibration: Wet captured f_wet=%lu Hz\n", freq);
            }
            break;
            
        case AGSYS_CAL_CMD_RESET:
            // Reset calibration for this probe
            if (moistureCal_clear(cmd->probeIndex)) {
                DEBUG_PRINTLN("Calibration: Probe calibration cleared");
            }
            break;
    }
    
    // Reset pairing timeout on activity
    pairingModeStartTime = millis();
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
 * @brief Read moisture sensor using oscillator frequency shift method
 * 
 * Reads all probes and returns the first probe's frequency as raw value.
 * For full multi-probe support, use moistureProbe_readAll() directly.
 */
uint16_t readMoistureRaw() {
    DEBUG_PRINTLN("Moisture: Starting oscillator frequency measurement...");
    
    // Read first probe for backward compatibility
    ProbeReading reading;
    moistureProbe_readSingle(0, &reading);
    
    DEBUG_PRINTF("Moisture: Probe 0 freq=%lu Hz, moisture=%d%%\n", 
                 reading.frequency, reading.moisturePercent);
    
    // Return frequency scaled to 16-bit range for compatibility
    // (actual frequency is in reading.frequency)
    return (uint16_t)(reading.frequency / 100);
}

/**
 * @brief Convert raw moisture reading to percentage
 */
uint8_t moistureToPercent(uint16_t raw) {
    // New oscillator approach uses per-probe calibration
    // This function is for backward compatibility only
    // Use moistureProbe_frequencyToPercent() for proper conversion
    (void)raw;
    return 0;
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
    // SPI LED removed - single status LED only
}

/**
 * @brief Turn off SPI activity LED
 */
void ledSpiOff() {
    // SPI LED removed - single status LED only
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
 * @brief Button wake interrupt service routine
 * Called when button is pressed during sleep
 */
void buttonWakeISR() {
    // Set flag - will be checked after wake
    buttonWakeFlag = true;
}

/**
 * @brief Enter deep sleep mode (nRF52 System ON sleep with RTC or GPIO wake)
 * @param remainingSleepMs If non-zero, sleep for this duration instead of full interval
 */
void enterDeepSleep(uint32_t remainingSleepMs) {
    DEBUG_PRINTLN("Entering deep sleep...");
    
    #if DEBUG_MODE
    Serial.flush();
    #endif
    
    // Put LoRa module to sleep
    LoRa.sleep();
    
    // Put NVRAM to sleep
    nvram.sleep();
    
    // Turn off LED
    digitalWrite(PIN_LED_STATUS, LOW);
    
    // Turn off moisture sensor power (P-FET active low, so HIGH = off)
    digitalWrite(PIN_PROBE_POWER, HIGH);
    
    // Calculate sleep duration
    uint32_t sleepMs;
    if (remainingSleepMs > 0) {
        // Resume previous sleep with remaining time
        sleepMs = remainingSleepMs;
        DEBUG_PRINT("Resuming sleep with ");
        DEBUG_PRINT(sleepMs / 1000);
        DEBUG_PRINTLN(" seconds remaining");
    } else {
        // Start fresh sleep interval
        sleepMs = SLEEP_INTERVAL_MS;
        
        // Extend sleep if battery is critical
        if (batteryMv < BATTERY_CRITICAL_MV) {
            sleepMs *= CRITICAL_SLEEP_MULTIPLIER;
            DEBUG_PRINTLN("Battery critical - extended sleep");
        }
    }
    
    // Track sleep start time to calculate remaining time on early wake
    uint32_t sleepStartTime = millis();
    
    // Clear button wake flag before sleep
    buttonWakeFlag = false;
    
    // Attach interrupt to wake on button press (falling edge = button pressed)
    attachInterrupt(digitalPinToInterrupt(PIN_PAIRING_BUTTON), buttonWakeISR, FALLING);
    
    // nRF52 uses SoftDevice for sleep management
    // suspendLoop() + delay() puts device in System ON sleep mode
    // The delay() will be interrupted early if button ISR fires
    suspendLoop();
    delay(sleepMs);
    resumeLoop();
    
    // Detach interrupt after wake
    detachInterrupt(digitalPinToInterrupt(PIN_PAIRING_BUTTON));
    
    // Calculate elapsed sleep time
    uint32_t elapsedMs = millis() - sleepStartTime;
    uint32_t remainingMs = (elapsedMs < sleepMs) ? (sleepMs - elapsedMs) : 0;
    
    // === Execution resumes here after wake (RTC timeout or button press) ===
    
    // Wake NVRAM
    nvram.wake();
    
    // LoRa will be re-initialized on next TX
    LoRa.idle();
    
    // Check if woken by button press
    if (buttonWakeFlag) {
        DEBUG_PRINTLN("Woke from button press");
        DEBUG_PRINTF("Sleep elapsed: %lu ms, remaining: %lu ms\n", elapsedMs, remainingMs);
        buttonWakeFlag = false;
        
        // Check if button is still held (2 second hold required)
        if (isButtonHeld(PAIRING_BUTTON_HOLD_MS)) {
            DEBUG_PRINTLN("Button held - entering pairing mode");
            enterPairingMode();
        } else {
            DEBUG_PRINTLN("Button released early - going back to sleep");
            // Go back to sleep with remaining time
            if (remainingMs > 1000) {  // Only if more than 1 second remains
                enterDeepSleep(remainingMs);
            } else {
                // Less than 1 second remaining, just do normal wake cycle
                DEBUG_PRINTLN("Less than 1s remaining - proceeding with wake cycle");
            }
        }
    } else {
        DEBUG_PRINTLN("Woke from RTC timer");
    }
}

/**
 * @brief Callback for wake interrupt (legacy - not used)
 */
void onWakeup() {
    // This is called in interrupt context
    // Keep it minimal - just set a flag if needed
}

#endif // !TEST_MODE_CYCLE_READINGS
