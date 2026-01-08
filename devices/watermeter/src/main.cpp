/**
 * @file main.cpp
 * @brief Water Meter Main Application
 * 
 * Monitors water flow using a pulse-based flow sensor and reports
 * readings to the property controller via LoRa.
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <bluefruit.h>
#include <Adafruit_FRAM_SPI.h>
#include "config.h"
#include "agsys_protocol.h"
#include "agsys_lora.h"
#include "agsys_crypto.h"

/* ==========================================================================
 * GLOBAL STATE
 * ========================================================================== */

// FRAM instance
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(PIN_FRAM_CS);

// Flow counting (volatile for ISR access)
volatile uint32_t pulseCount = 0;
volatile uint32_t lastPulseTime = 0;

// Persistent counters (saved to FRAM)
uint32_t totalPulses = 0;
uint32_t totalLiters = 0;

// Flow rate calculation
uint32_t lastFlowCalcTime = 0;
uint32_t pulsesAtLastCalc = 0;
uint16_t currentFlowRateLPM10 = 0;  // Flow rate * 10

// Timing
uint32_t lastReportTime = 0;
uint32_t flowStartTime = 0;
bool isFlowing = false;

// Device state
uint8_t deviceUid[AGSYS_DEVICE_UID_SIZE];
bool pairingModeActive = false;
uint32_t pairingModeStartTime = 0;

// Status flags
uint8_t statusFlags = 0;

/* ==========================================================================
 * FUNCTION PROTOTYPES
 * ========================================================================== */

void initPins(void);
void initSPI(void);
void initLoRa(void);
void initFRAM(void);
void loadCounters(void);
void saveCounters(void);
void getDeviceUid(uint8_t* uid);

void flowPulseISR(void);
void calculateFlowRate(void);
void checkLeakDetection(void);

void sendReport(void);
void processLoRa(void);

void enterPairingMode(void);
void exitPairingMode(void);

uint16_t readBatteryMv(void);
void updateStatusFlags(void);

/* ==========================================================================
 * SETUP
 * ========================================================================== */

void setup() {
    #if DEBUG_MODE
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    DEBUG_PRINTLN("Water Meter Starting...");
    #endif

    initPins();
    initSPI();
    initFRAM();
    loadCounters();
    
    // Get device UID and initialize LoRa
    getDeviceUid(deviceUid);
    initLoRa();
    
    // Initialize AgSys LoRa layer
    if (!agsys_lora_init(deviceUid, AGSYS_DEVICE_TYPE_WATER_METER)) {
        DEBUG_PRINTLN("ERROR: Failed to initialize AgSys LoRa");
    }
    
    // Load crypto nonce from FRAM
    uint32_t savedNonce = 0;
    fram.read(FRAM_ADDR_NONCE, (uint8_t*)&savedNonce, sizeof(savedNonce));
    agsys_crypto_setNonce(savedNonce);
    
    // Attach flow pulse interrupt
    pinMode(PIN_FLOW_PULSE, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FLOW_PULSE), flowPulseISR, FALLING);
    
    DEBUG_PRINTLN("Water Meter Ready");
    DEBUG_PRINTF("Total liters: %lu\n", totalLiters);
    
    // Send initial report
    sendReport();
    lastReportTime = millis();
}

/* ==========================================================================
 * MAIN LOOP
 * ========================================================================== */

void loop() {
    uint32_t now = millis();
    
    // Handle pairing mode
    if (pairingModeActive) {
        if (now - pairingModeStartTime > BLE_PAIRING_TIMEOUT_MS) {
            exitPairingMode();
        }
        return;
    }
    
    // Check pairing button
    if (digitalRead(PIN_PAIRING_BUTTON) == LOW) {
        uint32_t pressStart = millis();
        while (digitalRead(PIN_PAIRING_BUTTON) == LOW && 
               (millis() - pressStart) < PAIRING_BUTTON_HOLD_MS) {
            delay(10);
        }
        if ((millis() - pressStart) >= PAIRING_BUTTON_HOLD_MS) {
            enterPairingMode();
            return;
        }
    }
    
    // Calculate flow rate periodically
    if (now - lastFlowCalcTime >= 1000) {
        calculateFlowRate();
        lastFlowCalcTime = now;
    }
    
    // Check for leak detection
    checkLeakDetection();
    
    // Process incoming LoRa messages
    processLoRa();
    
    // Determine report interval based on flow state
    uint32_t reportInterval = isFlowing ? REPORT_INTERVAL_FLOW_MS : REPORT_INTERVAL_IDLE_MS;
    
    // Send periodic report
    if (now - lastReportTime >= reportInterval) {
        sendReport();
        lastReportTime = now;
    }
    
    // Save counters periodically (every 10 liters or 5 minutes)
    static uint32_t lastSaveLiters = 0;
    static uint32_t lastSaveTime = 0;
    if (totalLiters - lastSaveLiters >= 10 || now - lastSaveTime >= 300000) {
        saveCounters();
        lastSaveLiters = totalLiters;
        lastSaveTime = now;
    }
    
    // Low power delay
    delay(SLEEP_INTERVAL_MS);
}

/* ==========================================================================
 * INITIALIZATION FUNCTIONS
 * ========================================================================== */

void initPins(void) {
    // LED
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    
    // SPI chip selects
    pinMode(PIN_LORA_CS, OUTPUT);
    pinMode(PIN_FRAM_CS, OUTPUT);
    pinMode(PIN_FLASH_CS, OUTPUT);
    
    digitalWrite(PIN_LORA_CS, HIGH);
    digitalWrite(PIN_FRAM_CS, HIGH);
    digitalWrite(PIN_FLASH_CS, HIGH);
    
    // Pairing button
    pinMode(PIN_PAIRING_BUTTON, INPUT_PULLUP);
}

void initSPI(void) {
    SPI.begin();
}

void initLoRa(void) {
    DEBUG_PRINTLN("Initializing LoRa...");
    
    LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
    
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("ERROR: LoRa init failed!");
        while (1) {
            digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
            delay(100);
        }
    }
    
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    
    DEBUG_PRINTLN("LoRa initialized");
}

void initFRAM(void) {
    DEBUG_PRINTLN("Initializing FRAM...");
    if (!fram.begin()) {
        DEBUG_PRINTLN("WARNING: FRAM init failed, using defaults");
    }
    DEBUG_PRINTLN("FRAM initialized");
}

void loadCounters(void) {
    // Load persistent counters from FRAM
    fram.read(FRAM_ADDR_COUNTERS, (uint8_t*)&totalPulses, sizeof(totalPulses));
    fram.read(FRAM_ADDR_COUNTERS + 4, (uint8_t*)&totalLiters, sizeof(totalLiters));
    
    DEBUG_PRINTF("Loaded counters: pulses=%lu, liters=%lu\n", totalPulses, totalLiters);
}

void saveCounters(void) {
    fram.write(FRAM_ADDR_COUNTERS, (uint8_t*)&totalPulses, sizeof(totalPulses));
    fram.write(FRAM_ADDR_COUNTERS + 4, (uint8_t*)&totalLiters, sizeof(totalLiters));
    
    // Also save crypto nonce
    uint32_t nonce = agsys_crypto_getNonce();
    fram.write(FRAM_ADDR_NONCE, (uint8_t*)&nonce, sizeof(nonce));
    
    DEBUG_PRINTLN("Counters saved to FRAM");
}

void getDeviceUid(uint8_t* uid) {
    // Read device ID from nRF52 FICR registers
    uint32_t deviceId0 = NRF_FICR->DEVICEID[0];
    uint32_t deviceId1 = NRF_FICR->DEVICEID[1];
    
    uid[0] = (deviceId0 >> 0) & 0xFF;
    uid[1] = (deviceId0 >> 8) & 0xFF;
    uid[2] = (deviceId0 >> 16) & 0xFF;
    uid[3] = (deviceId0 >> 24) & 0xFF;
    uid[4] = (deviceId1 >> 0) & 0xFF;
    uid[5] = (deviceId1 >> 8) & 0xFF;
    uid[6] = (deviceId1 >> 16) & 0xFF;
    uid[7] = (deviceId1 >> 24) & 0xFF;
}

/* ==========================================================================
 * FLOW MEASUREMENT
 * ========================================================================== */

void flowPulseISR(void) {
    uint32_t now = millis();
    
    // Debounce
    if (now - lastPulseTime < PULSE_DEBOUNCE_MS) {
        return;
    }
    
    pulseCount++;
    lastPulseTime = now;
}

void calculateFlowRate(void) {
    // Get pulse count atomically
    noInterrupts();
    uint32_t currentPulses = pulseCount;
    interrupts();
    
    // Calculate pulses since last calculation
    uint32_t deltaPulses = currentPulses - pulsesAtLastCalc;
    pulsesAtLastCalc = currentPulses;
    
    // Update total counters
    totalPulses = currentPulses;
    totalLiters = totalPulses / FLOW_PULSES_PER_LITER;
    
    // Calculate flow rate (liters per minute * 10)
    // deltaPulses in 1 second = deltaPulses * 60 per minute
    // liters per minute = (deltaPulses * 60) / FLOW_PULSES_PER_LITER
    // * 10 for fixed point
    currentFlowRateLPM10 = (uint16_t)((deltaPulses * 600UL) / FLOW_PULSES_PER_LITER);
    
    // Update flow state
    bool wasFlowing = isFlowing;
    isFlowing = (currentFlowRateLPM10 >= FLOW_MIN_RATE_LPM10);
    
    if (isFlowing && !wasFlowing) {
        // Flow just started
        flowStartTime = millis();
        DEBUG_PRINTLN("Flow started");
    } else if (!isFlowing && wasFlowing) {
        // Flow just stopped
        DEBUG_PRINTLN("Flow stopped");
    }
}

void checkLeakDetection(void) {
    if (!isFlowing) {
        statusFlags &= ~AGSYS_METER_FLAG_LEAK_DETECTED;
        return;
    }
    
    // Check if flowing continuously for too long
    uint32_t flowDuration = millis() - flowStartTime;
    if (flowDuration >= (LEAK_DETECTION_MINUTES * 60UL * 1000UL)) {
        statusFlags |= AGSYS_METER_FLAG_LEAK_DETECTED;
        DEBUG_PRINTLN("WARNING: Possible leak detected!");
    }
}

/* ==========================================================================
 * LORA COMMUNICATION
 * ========================================================================== */

void sendReport(void) {
    DEBUG_PRINTLN("Sending water meter report...");
    
    // Update status flags
    updateStatusFlags();
    
    // Build payload
    AgsysWaterMeterReport report;
    report.timestamp = millis() / 1000;  // Uptime in seconds
    report.totalPulses = totalPulses;
    report.totalLiters = totalLiters;
    report.flowRateLPM = currentFlowRateLPM10;
    report.batteryMv = readBatteryMv();
    report.flags = statusFlags;
    
    // Send via AgSys LoRa layer
    if (agsys_lora_send(AGSYS_MSG_WATER_METER_REPORT, (uint8_t*)&report, sizeof(report))) {
        DEBUG_PRINTLN("Report sent successfully");
        digitalWrite(PIN_LED_STATUS, HIGH);
        delay(50);
        digitalWrite(PIN_LED_STATUS, LOW);
    } else {
        DEBUG_PRINTLN("ERROR: Failed to send report");
    }
    
    // Save nonce after TX
    uint32_t nonce = agsys_crypto_getNonce();
    fram.write(FRAM_ADDR_NONCE, (uint8_t*)&nonce, sizeof(nonce));
}

void processLoRa(void) {
    AgsysHeader header;
    uint8_t payload[64];
    size_t payloadLen = sizeof(payload);
    int16_t rssi;
    
    if (agsys_lora_receive(&header, payload, &payloadLen, &rssi)) {
        DEBUG_PRINTF("Received message type 0x%02X, RSSI=%d\n", header.msgType, rssi);
        
        switch (header.msgType) {
            case AGSYS_MSG_TIME_SYNC: {
                if (payloadLen >= sizeof(AgsysTimeSync)) {
                    AgsysTimeSync* timeSync = (AgsysTimeSync*)payload;
                    DEBUG_PRINTF("Time sync: %lu\n", timeSync->unixTimestamp);
                    // Water meter doesn't have RTC, but could use for logging
                }
                break;
            }
            
            case AGSYS_MSG_CONFIG_UPDATE: {
                if (payloadLen >= sizeof(AgsysConfigUpdate)) {
                    AgsysConfigUpdate* config = (AgsysConfigUpdate*)payload;
                    DEBUG_PRINTF("Config update: version=%d\n", config->configVersion);
                    // Apply configuration changes
                }
                break;
            }
            
            case AGSYS_MSG_ACK: {
                if (payloadLen >= sizeof(AgsysAck)) {
                    AgsysAck* ack = (AgsysAck*)payload;
                    DEBUG_PRINTF("ACK for seq %d, status=%d\n", ack->ackedSequence, ack->status);
                }
                break;
            }
            
            default:
                DEBUG_PRINTF("Unknown message type: 0x%02X\n", header.msgType);
                break;
        }
    }
}

/* ==========================================================================
 * BLE OPERATIONS
 * ========================================================================== */

void enterPairingMode(void) {
    DEBUG_PRINTLN("Entering BLE pairing mode");
    pairingModeActive = true;
    pairingModeStartTime = millis();
    
    Bluefruit.begin();
    Bluefruit.setName(BLE_DEVICE_NAME);
    Bluefruit.Advertising.start();
}

void exitPairingMode(void) {
    DEBUG_PRINTLN("Exiting BLE pairing mode");
    pairingModeActive = false;
    
    Bluefruit.Advertising.stop();
}

/* ==========================================================================
 * UTILITY FUNCTIONS
 * ========================================================================== */

uint16_t readBatteryMv(void) {
    // Read battery voltage via ADC
    int raw = analogRead(PIN_BATTERY_ANALOG);
    
    // Convert to mV (assuming 3.3V reference, 12-bit ADC, 2:1 divider)
    uint16_t mv = (uint16_t)((raw * 3300UL * BATTERY_DIVIDER_RATIO) / 4095);
    
    return mv;
}

void updateStatusFlags(void) {
    uint16_t batteryMv = readBatteryMv();
    
    if (batteryMv < BATTERY_LOW_THRESHOLD_MV) {
        statusFlags |= AGSYS_METER_FLAG_LOW_BATTERY;
    } else {
        statusFlags &= ~AGSYS_METER_FLAG_LOW_BATTERY;
    }
    
    // Leak detection flag is set in checkLeakDetection()
}
