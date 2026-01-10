/**
 * @file main.cpp
 * @brief Valve Actuator Main Application
 * 
 * Controls a single motorized ball valve via discrete H-bridge,
 * communicates with valve controller via CAN bus.
 */

#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>
#include <Adafruit_FRAM_SPI.h>
#include "config.h"
#include "hbridge.h"
#include "valve.h"
#include "agsys_ble.h"

/* ==========================================================================
 * GLOBAL STATE
 * ========================================================================== */

// CAN bus
MCP2515 canBus(PIN_CAN_CS);
volatile bool canInterruptFlag = false;

// FRAM for PIN storage and diagnostics
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(PIN_FRAM_CS);

// BLE pairing mode state
static bool pairingModeActive = false;
static unsigned long pairingModeStartTime = 0;

// Device address (read from DIP switches)
uint8_t deviceAddress = 0;

/* ==========================================================================
 * FUNCTION PROTOTYPES
 * ========================================================================== */

void initPins(void);
void initSPI(void);
void initCAN(void);
void initFRAM(void);
void initBLE(void);
uint8_t readAddress(void);
bool isTerminationEnabled(void);
void canISR(void);
void processCAN(void);
void sendStatus(void);
void sendUID(void);
void getDeviceUID(uint8_t* uid);
void updateLEDs(void);
void checkPairingMode(void);

/* ==========================================================================
 * SETUP
 * ========================================================================== */

void setup() {
    #if DEBUG_MODE
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    DEBUG_PRINTLN("Valve Actuator Starting...");
    #endif

    initPins();
    initSPI();
    initFRAM();
    
    // Read device address from DIP switches
    deviceAddress = readAddress();
    DEBUG_PRINTF("Device address: %d\n", deviceAddress);
    
    if (deviceAddress == 0 || deviceAddress > 64) {
        DEBUG_PRINTLN("ERROR: Invalid address! Check DIP switches.");
    }
    
    initCAN();
    
    // Initialize valve control module
    valve_init();
    
    // Initialize BLE for DFU updates
    initBLE();
    
    DEBUG_PRINTLN("Valve Actuator Ready");
}

/* ==========================================================================
 * MAIN LOOP
 * ========================================================================== */

void loop() {
    // Process CAN messages
    if (canInterruptFlag) {
        canInterruptFlag = false;
        processCAN();
    }
    
    // Update valve state machine
    valve_update();
    
    // Process BLE (handles pairing timeout)
    agsys_ble_process();
    checkPairingMode();
    
    // Update LED indicators
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
    pinMode(PIN_LED_VALVE_OPEN, OUTPUT);
    
    digitalWrite(PIN_LED_3V3, HIGH);
    digitalWrite(PIN_LED_24V, HIGH);
    digitalWrite(PIN_LED_STATUS, LOW);
    digitalWrite(PIN_LED_VALVE_OPEN, LOW);
    
    // DIP switch bank (10-position, active LOW with internal pullup)
    pinMode(PIN_DIP_1, INPUT_PULLUP);
    pinMode(PIN_DIP_2, INPUT_PULLUP);
    pinMode(PIN_DIP_3, INPUT_PULLUP);
    pinMode(PIN_DIP_4, INPUT_PULLUP);
    pinMode(PIN_DIP_5, INPUT_PULLUP);
    pinMode(PIN_DIP_6, INPUT_PULLUP);
    pinMode(PIN_DIP_7, INPUT_PULLUP);
    pinMode(PIN_DIP_8, INPUT_PULLUP);
    pinMode(PIN_DIP_9, INPUT_PULLUP);
    pinMode(PIN_DIP_10, INPUT_PULLUP);
    
    // CAN chip select
    pinMode(PIN_CAN_CS, OUTPUT);
    digitalWrite(PIN_CAN_CS, HIGH);
    
    // CAN interrupt
    pinMode(PIN_CAN_INT, INPUT_PULLUP);
    
    // FRAM chip select
    pinMode(PIN_FRAM_CS, OUTPUT);
    digitalWrite(PIN_FRAM_CS, HIGH);
    
    // Pairing button (active LOW, external 10K pullup)
    pinMode(PIN_PAIRING_BUTTON, INPUT);
}

void initSPI(void) {
    SPI.begin();
}

void initCAN(void) {
    DEBUG_PRINTLN("Initializing CAN bus...");
    
    canBus.reset();
    canBus.setBitrate(CAN_SPEED, CAN_CLOCK);
    canBus.setNormalMode();
    
    // Attach interrupt
    attachInterrupt(digitalPinToInterrupt(PIN_CAN_INT), canISR, FALLING);
    
    DEBUG_PRINTLN("CAN bus initialized at 1 Mbps");
}

uint8_t readAddress(void) {
    uint8_t addr = 0;
    
    // DIP switches are active LOW (ON = LOW = 0)
    if (digitalRead(PIN_DIP_1) == LOW) addr |= 0x01;
    if (digitalRead(PIN_DIP_2) == LOW) addr |= 0x02;
    if (digitalRead(PIN_DIP_3) == LOW) addr |= 0x04;
    if (digitalRead(PIN_DIP_4) == LOW) addr |= 0x08;
    if (digitalRead(PIN_DIP_5) == LOW) addr |= 0x10;
    if (digitalRead(PIN_DIP_6) == LOW) addr |= 0x20;
    
    return addr;
}

bool isTerminationEnabled(void) {
    return (digitalRead(PIN_DIP_10) == LOW);
}

/* ==========================================================================
 * CAN BUS OPERATIONS
 * ========================================================================== */

void canISR(void) {
    canInterruptFlag = true;
}

void processCAN(void) {
    struct can_frame frame;
    
    while (canBus.readMessage(&frame) == MCP2515::ERROR_OK) {
        DEBUG_PRINTF("CAN RX: ID=0x%03X, DLC=%d\n", frame.can_id, frame.can_dlc);
        
        switch (frame.can_id) {
            case CAN_ID_VALVE_OPEN:
                if (frame.can_dlc >= 1 && frame.data[0] == deviceAddress) {
                    DEBUG_PRINTLN("Command: OPEN");
                    valve_open();
                    sendStatus();
                }
                break;
                
            case CAN_ID_VALVE_CLOSE:
                if (frame.can_dlc >= 1 && frame.data[0] == deviceAddress) {
                    DEBUG_PRINTLN("Command: CLOSE");
                    valve_close();
                    sendStatus();
                }
                break;
                
            case CAN_ID_VALVE_STOP:
                if (frame.can_dlc >= 1 && frame.data[0] == deviceAddress) {
                    DEBUG_PRINTLN("Command: STOP");
                    valve_stop();
                    sendStatus();
                }
                break;
                
            case CAN_ID_VALVE_QUERY:
                if (frame.can_dlc >= 1 && frame.data[0] == deviceAddress) {
                    DEBUG_PRINTLN("Command: QUERY");
                    sendStatus();
                }
                break;
                
            case CAN_ID_EMERGENCY_CLOSE:
                DEBUG_PRINTLN("Command: EMERGENCY CLOSE");
                valve_emergency_close();
                sendStatus();
                break;
                
            case CAN_ID_UID_QUERY:
                if (frame.can_dlc >= 1 && frame.data[0] == deviceAddress) {
                    DEBUG_PRINTLN("Command: UID QUERY");
                    sendUID();
                }
                break;
                
            case CAN_ID_DISCOVER_ALL:
                // Broadcast discovery - all actuators respond with UID
                DEBUG_PRINTLN("Command: DISCOVER ALL");
                // Stagger response by address to avoid collisions
                delay(deviceAddress * 5);
                sendUID();
                break;
        }
    }
}

void sendStatus(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_STATUS_BASE + deviceAddress;
    frame.can_dlc = 4;
    frame.data[0] = valve_get_status_flags();
    frame.data[1] = (valve_get_current_ma() >> 8) & 0xFF;
    frame.data[2] = valve_get_current_ma() & 0xFF;
    frame.data[3] = 0;
    
    if (canBus.sendMessage(&frame) != MCP2515::ERROR_OK) {
        DEBUG_PRINTLN("ERROR: Failed to send status");
    }
}

void sendUID(void) {
    uint8_t uid[8];
    getDeviceUID(uid);
    
    struct can_frame frame;
    frame.can_id = CAN_ID_UID_RESPONSE_BASE + deviceAddress;
    frame.can_dlc = 8;
    memcpy(frame.data, uid, 8);
    
    DEBUG_PRINTF("Sending UID: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                 uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);
    
    if (canBus.sendMessage(&frame) != MCP2515::ERROR_OK) {
        DEBUG_PRINTLN("ERROR: Failed to send UID");
    }
}

void getDeviceUID(uint8_t* uid) {
    // Read nRF52 device ID from FICR registers
    // DEVICEID[0] and DEVICEID[1] provide 64-bit unique ID
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
 * LED INDICATORS
 * ========================================================================== */

void updateLEDs(void) {
    // Valve open LED - blue when open
    digitalWrite(PIN_LED_VALVE_OPEN, valve_is_open() ? HIGH : LOW);
    
    // Status LED - flash patterns
    static uint32_t lastBlink = 0;
    static bool ledState = false;
    uint8_t flags = valve_get_status_flags();
    
    if (flags & STATUS_FLAG_FAULT) {
        // Fast blink on fault
        if (millis() - lastBlink > 100) {
            ledState = !ledState;
            digitalWrite(PIN_LED_STATUS, ledState);
            lastBlink = millis();
        }
    } else if (flags & STATUS_FLAG_MOVING) {
        // Slow blink while moving
        if (millis() - lastBlink > 500) {
            ledState = !ledState;
            digitalWrite(PIN_LED_STATUS, ledState);
            lastBlink = millis();
        }
    } else {
        digitalWrite(PIN_LED_STATUS, LOW);
    }
}

/* ==========================================================================
 * FRAM AND BLE INITIALIZATION
 * ========================================================================== */

void initFRAM(void) {
    DEBUG_PRINTLN("Initializing FRAM...");
    if (!fram.begin()) {
        DEBUG_PRINTLN("WARNING: FRAM init failed");
    } else {
        DEBUG_PRINTLN("FRAM initialized");
    }
}

void initBLE(void) {
    DEBUG_PRINTLN("Initializing BLE...");
    agsys_ble_init(AGSYS_BLE_DEVICE_NAME, AGSYS_DEVICE_TYPE_VALVE_ACTUATOR, AGSYS_BLE_FRAM_PIN_ADDR,
                   FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
    DEBUG_PRINTLN("BLE initialized (DFU ready)");
}

void checkPairingMode(void) {
    static uint32_t buttonPressStart = 0;
    static bool buttonWasPressed = false;
    
    bool buttonPressed = (digitalRead(PIN_PAIRING_BUTTON) == LOW);
    
    if (buttonPressed && !buttonWasPressed) {
        // Button just pressed
        buttonPressStart = millis();
        buttonWasPressed = true;
    } else if (!buttonPressed && buttonWasPressed) {
        // Button released
        buttonWasPressed = false;
    } else if (buttonPressed && buttonWasPressed) {
        // Button held
        uint32_t holdTime = millis() - buttonPressStart;
        
        if (!pairingModeActive && holdTime >= PAIRING_BUTTON_HOLD_MS) {
            // Enter pairing mode
            pairingModeActive = true;
            pairingModeStartTime = millis();
            agsys_ble_start_advertising();
            DEBUG_PRINTLN("Pairing mode activated");
            
            // Blink LED to indicate pairing mode
            for (int i = 0; i < 5; i++) {
                digitalWrite(PIN_LED_STATUS, HIGH);
                delay(100);
                digitalWrite(PIN_LED_STATUS, LOW);
                delay(100);
            }
        }
    }
    
    if (pairingModeActive) {
        // Slow blink while in pairing mode
        static uint32_t lastBlink = 0;
        if (millis() - lastBlink > 500) {
            digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
            lastBlink = millis();
        }
        
        // Check timeout
        if (millis() - pairingModeStartTime > BLE_PAIRING_TIMEOUT_MS) {
            pairingModeActive = false;
            agsys_ble_stop_advertising();
            digitalWrite(PIN_LED_STATUS, LOW);
            DEBUG_PRINTLN("Pairing mode timeout");
        }
    }
}
