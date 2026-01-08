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
#include "agsys_protocol.h"
#include "agsys_lora.h"
#include "agsys_crypto.h"

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

// Device UID
uint8_t deviceUid[AGSYS_DEVICE_UID_SIZE];

// Pending command tracking
uint16_t pendingCommandId = 0;

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
void getDeviceUid(uint8_t* uid);
void handleValveCommand(AgsysValveCommand* cmd);
void sendValveAck(uint8_t actuatorAddr, uint16_t commandId, uint8_t resultState, bool success, uint8_t errorCode);

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
    
    // Get device UID and initialize AgSys LoRa layer
    getDeviceUid(deviceUid);
    if (!agsys_lora_init(deviceUid, AGSYS_DEVICE_TYPE_VALVE_CONTROLLER)) {
        DEBUG_PRINTLN("ERROR: Failed to initialize AgSys LoRa");
    }
    
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
    AgsysHeader header;
    uint8_t payload[128];
    size_t payloadLen = sizeof(payload);
    int16_t rssi;
    
    if (agsys_lora_receive(&header, payload, &payloadLen, &rssi)) {
        DEBUG_PRINTF("Received message type 0x%02X, RSSI=%d\n", header.msgType, rssi);
        
        switch (header.msgType) {
            case AGSYS_MSG_VALVE_COMMAND: {
                if (payloadLen >= sizeof(AgsysValveCommand)) {
                    AgsysValveCommand* cmd = (AgsysValveCommand*)payload;
                    handleValveCommand(cmd);
                }
                break;
            }
            
            case AGSYS_MSG_SCHEDULE_UPDATE: {
                if (payloadLen >= sizeof(AgsysScheduleHeader)) {
                    AgsysScheduleHeader* schedHeader = (AgsysScheduleHeader*)payload;
                    DEBUG_PRINTF("Schedule update: version=%d, entries=%d\n", 
                                 schedHeader->scheduleVersion, schedHeader->entryCount);
                    // Parse and store schedule entries
                    // schedule_update(payload, payloadLen);
                }
                break;
            }
            
            case AGSYS_MSG_TIME_SYNC: {
                if (payloadLen >= sizeof(AgsysTimeSync)) {
                    AgsysTimeSync* timeSync = (AgsysTimeSync*)payload;
                    DEBUG_PRINTF("Time sync: %lu\n", timeSync->unixTimestamp);
                    rtc_setUnixTime(timeSync->unixTimestamp);
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

void handleValveCommand(AgsysValveCommand* cmd) {
    DEBUG_PRINTF("Valve command: addr=%d, cmd=%d, id=%d, duration=%d\n",
                 cmd->actuatorAddr, cmd->command, cmd->commandId, cmd->durationSec);
    
    bool success = false;
    uint8_t resultState = AGSYS_VALVE_STATE_ERROR;
    uint8_t errorCode = AGSYS_VALVE_ERR_NONE;
    
    // Execute command via CAN bus
    switch (cmd->command) {
        case AGSYS_VALVE_CMD_OPEN:
            if (cmd->actuatorAddr == 0xFF) {
                // Open all - not typically used
                success = false;
                errorCode = AGSYS_VALVE_ERR_ACTUATOR_OFFLINE;
            } else {
                success = canbus_open_valve(cmd->actuatorAddr);
                resultState = success ? AGSYS_VALVE_STATE_OPEN : AGSYS_VALVE_STATE_ERROR;
            }
            break;
            
        case AGSYS_VALVE_CMD_CLOSE:
            if (cmd->actuatorAddr == 0xFF) {
                canbus_emergency_close_all();
                success = true;
                resultState = AGSYS_VALVE_STATE_CLOSED;
            } else {
                success = canbus_close_valve(cmd->actuatorAddr);
                resultState = success ? AGSYS_VALVE_STATE_CLOSED : AGSYS_VALVE_STATE_ERROR;
            }
            break;
            
        case AGSYS_VALVE_CMD_STOP:
            success = canbus_stop_valve(cmd->actuatorAddr);
            resultState = AGSYS_VALVE_STATE_ERROR;  // Unknown state after stop
            break;
            
        case AGSYS_VALVE_CMD_QUERY:
            resultState = canbus_get_valve_state(cmd->actuatorAddr);
            success = (resultState != AGSYS_VALVE_STATE_ERROR);
            break;
    }
    
    if (!success && errorCode == AGSYS_VALVE_ERR_NONE) {
        errorCode = AGSYS_VALVE_ERR_ACTUATOR_OFFLINE;
    }
    
    // Send acknowledgment
    sendValveAck(cmd->actuatorAddr, cmd->commandId, resultState, success, errorCode);
    
    // Log event
    logEvent(cmd->command == AGSYS_VALVE_CMD_OPEN ? EVENT_VALVE_OPEN : EVENT_VALVE_CLOSE,
             cmd->actuatorAddr, cmd->durationSec, 0);
}

void sendValveAck(uint8_t actuatorAddr, uint16_t commandId, uint8_t resultState, bool success, uint8_t errorCode) {
    AgsysValveAck ack;
    ack.actuatorAddr = actuatorAddr;
    ack.commandId = commandId;
    ack.resultState = resultState;
    ack.success = success ? 1 : 0;
    ack.errorCode = errorCode;
    
    if (agsys_lora_send(AGSYS_MSG_VALVE_ACK, (uint8_t*)&ack, sizeof(ack))) {
        DEBUG_PRINTLN("Valve ACK sent");
    } else {
        DEBUG_PRINTLN("ERROR: Failed to send valve ACK");
    }
}

void sendStatusReport(void) {
    DEBUG_PRINTLN("Sending valve status report...");
    
    // Build status report with all actuator states
    uint8_t buffer[128];
    AgsysValveStatusHeader* header = (AgsysValveStatusHeader*)buffer;
    header->timestamp = getRTCTime();
    header->actuatorCount = 0;
    
    AgsysActuatorStatus* statuses = (AgsysActuatorStatus*)(buffer + sizeof(AgsysValveStatusHeader));
    
    // Query all online actuators
    for (uint8_t addr = ACTUATOR_ADDR_MIN; addr <= ACTUATOR_ADDR_MAX; addr++) {
        if (canbus_is_actuator_online(addr)) {
            statuses[header->actuatorCount].address = addr;
            statuses[header->actuatorCount].state = canbus_get_valve_state(addr);
            statuses[header->actuatorCount].currentMa = canbus_get_motor_current(addr);
            statuses[header->actuatorCount].flags = onBatteryPower ? AGSYS_VALVE_FLAG_ON_BATTERY : 0;
            header->actuatorCount++;
            
            if (header->actuatorCount >= 20) break;  // Limit to fit in packet
        }
    }
    
    size_t payloadLen = sizeof(AgsysValveStatusHeader) + 
                        (header->actuatorCount * sizeof(AgsysActuatorStatus));
    
    if (agsys_lora_send(AGSYS_MSG_VALVE_STATUS, buffer, payloadLen)) {
        DEBUG_PRINTF("Status report sent: %d actuators\n", header->actuatorCount);
    } else {
        DEBUG_PRINTLN("ERROR: Failed to send status report");
    }
}

void pullScheduleUpdate(void) {
    DEBUG_PRINTLN("Requesting schedule update from property controller...");
    
    // Send schedule request message (no payload needed)
    if (agsys_lora_send(AGSYS_MSG_SCHEDULE_REQUEST, NULL, 0)) {
        DEBUG_PRINTLN("Schedule request sent");
    } else {
        DEBUG_PRINTLN("ERROR: Failed to send schedule request");
    }
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
