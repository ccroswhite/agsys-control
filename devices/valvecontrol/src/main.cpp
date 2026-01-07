/**
 * @file main.cpp
 * @brief Valve Controller Main Application
 * 
 * Controls up to 64 valve actuators via CAN bus, communicates with
 * property controller via LoRa, and supports BLE for local configuration.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <mcp2515.h>
#include <bluefruit.h>
#include <Adafruit_FRAM_SPI.h>
#include "config.h"
#include "schedule.h"
#include "can_bus.h"
#include "rtc.h"
#include "schedule_manager.h"

/* ==========================================================================
 * GLOBAL STATE
 * ========================================================================== */

// FRAM instance
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(PIN_FRAM_CS);

// Power state
volatile bool powerFailFlag = false;
bool onBatteryPower = false;

// Pairing mode
bool pairingModeActive = false;
uint32_t pairingModeStartTime = 0;

// Timing
uint32_t lastSchedulePull = 0;
uint32_t lastHeartbeat = 0;
uint32_t lastStatusReport = 0;

/* ==========================================================================
 * FUNCTION PROTOTYPES
 * ========================================================================== */

// Initialization
void initPins(void);
void initSPI(void);
void initCAN(void);
void initLoRa(void);
void initRTC(void);
void initFRAM(void);
void loadSchedules(void);

// CAN bus operations (now in can_bus module)

// Schedule operations
void checkSchedules(void);
bool shouldRunSchedule(ScheduleEntry* entry);
bool requestProceedCheck(uint8_t valveId);
void runScheduledIrrigation(ScheduleEntry* entry);

// LoRa operations
void processLoRa(void);
void sendStatusReport(void);
void pullScheduleUpdate(void);

// Power management
void powerFailISR(void);
void handlePowerFail(void);
void handlePowerRestore(void);

// BLE operations
void enterPairingMode(void);
void exitPairingMode(void);

// Utility
uint32_t getRTCTime(void);
void logEvent(uint8_t eventType, uint8_t valveId, uint16_t duration, uint16_t volume);
void updateLEDs(void);

/* ==========================================================================
 * SETUP
 * ========================================================================== */

void setup() {
    #if DEBUG_MODE
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    DEBUG_PRINTLN("Valve Controller Starting...");
    #endif

    initPins();
    initSPI();
    initRTC();
    initFRAM();
    initCAN();
    initLoRa();
    
    loadSchedules();
    
    // Attach power fail interrupt
    pinMode(PIN_POWER_FAIL, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_POWER_FAIL), powerFailISR, FALLING);
    
    // Check initial power state
    onBatteryPower = (digitalRead(PIN_POWER_FAIL) == LOW);
    if (onBatteryPower) {
        DEBUG_PRINTLN("WARNING: Starting on battery power");
        logEvent(EVENT_POWER_FAIL, 0, 0, 0);
    }
    
    DEBUG_PRINTLN("Valve Controller Ready");
    DEBUG_PRINTF("Actuators online: %d\n", canbus_get_online_count());
}

/* ==========================================================================
 * MAIN LOOP
 * ========================================================================== */

void loop() {
    uint32_t now = millis();
    
    // Handle power fail (highest priority)
    if (powerFailFlag) {
        powerFailFlag = false;
        handlePowerFail();
    }
    
    // Handle pairing mode
    if (pairingModeActive) {
        if (now - pairingModeStartTime > BLE_PAIRING_TIMEOUT_MS) {
            exitPairingMode();
        }
        // In pairing mode, skip normal operations
        updateLEDs();
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
        }
    }
    
    // Process CAN bus messages
    if (canbus_has_message()) {
        canbus_process();
    }
    
    // Process LoRa messages
    processLoRa();
    
    // Periodic schedule pull (only when on mains power)
    if (!onBatteryPower && (now - lastSchedulePull > SCHEDULE_PULL_INTERVAL_MS)) {
        pullScheduleUpdate();
        lastSchedulePull = now;
    }
    
    // Check and run schedules (only when on mains power)
    if (!onBatteryPower) {
        checkSchedules();
    }
    
    // Periodic heartbeat to actuators
    if (now - lastHeartbeat > HEARTBEAT_INTERVAL_MS) {
        canbus_query_all();
        lastHeartbeat = now;
    }
    
    // Periodic status report to property controller
    if (now - lastStatusReport > STATUS_REPORT_INTERVAL_MS) {
        sendStatusReport();
        lastStatusReport = now;
    }
    
    updateLEDs();
}

/* ==========================================================================
 * INITIALIZATION FUNCTIONS
 * ========================================================================== */

void initPins(void) {
    // LEDs
    pinMode(PIN_LED_3V3, OUTPUT);
    pinMode(PIN_LED_24V, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    
    digitalWrite(PIN_LED_3V3, HIGH);    // 3.3V present
    digitalWrite(PIN_LED_24V, LOW);     // Will be set based on power state
    digitalWrite(PIN_LED_STATUS, LOW);
    
    // SPI chip selects (active low, set high initially)
    pinMode(PIN_LORA_CS, OUTPUT);
    pinMode(PIN_CAN_CS, OUTPUT);
    pinMode(PIN_FRAM_CS, OUTPUT);
    pinMode(PIN_FLASH_CS, OUTPUT);
    
    digitalWrite(PIN_LORA_CS, HIGH);
    digitalWrite(PIN_CAN_CS, HIGH);
    digitalWrite(PIN_FRAM_CS, HIGH);
    digitalWrite(PIN_FLASH_CS, HIGH);
    
    // Interrupts
    pinMode(PIN_CAN_INT, INPUT_PULLUP);
    pinMode(PIN_LORA_DIO0, INPUT);
    
    // Pairing button
    pinMode(PIN_PAIRING_BUTTON, INPUT_PULLUP);
}

void initSPI(void) {
    SPI.begin();
}

void initCAN(void) {
    canbus_init();
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

void initRTC(void) {
    rtc_init();
}

void initFRAM(void) {
    DEBUG_PRINTLN("Initializing FRAM...");
    // FRAM initialization will be implemented
    DEBUG_PRINTLN("FRAM initialized");
}

void loadSchedules(void) {
    DEBUG_PRINTLN("Loading schedules from FRAM...");
    schedule_init();
    DEBUG_PRINTLN("Schedules loaded");
}

/* ==========================================================================
 * POWER MANAGEMENT
 * ========================================================================== */

void powerFailISR(void) {
    powerFailFlag = true;
}

void handlePowerFail(void) {
    // Debounce
    delay(POWER_FAIL_DEBOUNCE_MS);
    if (digitalRead(PIN_POWER_FAIL) == HIGH) {
        return;  // False alarm
    }
    
    if (!onBatteryPower) {
        DEBUG_PRINTLN("POWER FAIL: Switching to battery");
        onBatteryPower = true;
        
        // Emergency close all valves
        canbus_emergency_close_all();
        
        logEvent(EVENT_POWER_FAIL, 0, 0, 0);
    }
}

void handlePowerRestore(void) {
    if (onBatteryPower && digitalRead(PIN_POWER_FAIL) == HIGH) {
        DEBUG_PRINTLN("POWER RESTORED: Resuming normal operation");
        onBatteryPower = false;
        
        logEvent(EVENT_POWER_RESTORE, 0, 0, 0);
        
        // Pull fresh schedule
        pullScheduleUpdate();
    }
}

/* ==========================================================================
 * SCHEDULE OPERATIONS
 * ========================================================================== */

void checkSchedules(void) {
    // Implementation will check current time against schedules
    // and trigger irrigation as needed
}

bool shouldRunSchedule(ScheduleEntry* entry) {
    // Check if schedule should run based on:
    // - Current day of week
    // - Current time
    // - Flags (enabled, skip if wet, etc.)
    return false;
}

bool requestProceedCheck(uint8_t valveId) {
    // Ask property controller if we should proceed
    // Returns true if OK to proceed, or if no response (fallback)
    return true;
}

void runScheduledIrrigation(ScheduleEntry* entry) {
    // Open valve, monitor, close when done
}

/* ==========================================================================
 * LORA OPERATIONS
 * ========================================================================== */

void processLoRa(void) {
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {
        // Process incoming LoRa message
        // Implementation will handle commands from property controller
    }
}

void sendStatusReport(void) {
    // Send status to property controller
}

void pullScheduleUpdate(void) {
    DEBUG_PRINTLN("Pulling schedule update from property controller...");
    // Request schedule update via LoRa
}

/* ==========================================================================
 * BLE OPERATIONS
 * ========================================================================== */

void enterPairingMode(void) {
    DEBUG_PRINTLN("Entering BLE pairing mode");
    pairingModeActive = true;
    pairingModeStartTime = millis();
    
    // Initialize BLE
    Bluefruit.begin();
    Bluefruit.setName(BLE_DEVICE_NAME);
    Bluefruit.Advertising.start();
}

void exitPairingMode(void) {
    DEBUG_PRINTLN("Exiting BLE pairing mode");
    pairingModeActive = false;
    
    Bluefruit.Advertising.stop();
    // Optionally disable BLE to save power
}

/* ==========================================================================
 * UTILITY FUNCTIONS
 * ========================================================================== */

uint32_t getRTCTime(void) {
    // Read Unix timestamp from RV-3028
    return 0;  // Placeholder
}

void logEvent(uint8_t eventType, uint8_t valveId, uint16_t duration, uint16_t volume) {
    EventLogEntry entry;
    entry.timestamp = getRTCTime();
    entry.event_type = eventType;
    entry.valve_id = valveId;
    entry.duration_sec = duration;
    entry.volume_gallons = volume;
    entry.flags = 0;
    entry.reserved = 0;
    
    // Write to FRAM event log
    DEBUG_PRINTF("Event logged: type=%d, valve=%d\n", eventType, valveId);
}

void updateLEDs(void) {
    // 3.3V LED is always on (hardwired or set in initPins)
    
    // 24V LED - on when not on battery
    digitalWrite(PIN_LED_24V, !onBatteryPower);
    
    // Status LED - flash patterns for different states
    static uint32_t lastBlink = 0;
    static bool ledState = false;
    
    if (pairingModeActive) {
        // Fast blink in pairing mode
        if (millis() - lastBlink > 200) {
            ledState = !ledState;
            digitalWrite(PIN_LED_STATUS, ledState);
            lastBlink = millis();
        }
    } else if (onBatteryPower) {
        // Slow blink on battery
        if (millis() - lastBlink > 1000) {
            ledState = !ledState;
            digitalWrite(PIN_LED_STATUS, ledState);
            lastBlink = millis();
        }
    } else {
        // Off in normal operation
        digitalWrite(PIN_LED_STATUS, LOW);
    }
}
