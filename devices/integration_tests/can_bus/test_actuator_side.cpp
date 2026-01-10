/**
 * @file test_actuator_side.cpp
 * @brief Integration test firmware for valve actuator (CAN bus tests)
 * 
 * This firmware runs on the valve actuator and responds to CAN bus commands
 * from the valve controller integration tests.
 * 
 * REQUIREMENTS:
 * - Valve actuator board
 * - Connected to valve controller via CAN bus
 * - DIP switches set to a valid address (1-64)
 * 
 * USAGE:
 * 1. Flash this to actuator(s)
 * 2. Flash test_controller_side.cpp to valve controller
 * 3. Monitor serial output from both devices
 */

#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

// CAN message IDs
#define CAN_ID_VALVE_OPEN       0x100
#define CAN_ID_VALVE_CLOSE      0x101
#define CAN_ID_VALVE_STOP       0x102
#define CAN_ID_VALVE_QUERY      0x103
#define CAN_ID_UID_QUERY        0x104
#define CAN_ID_DISCOVER_ALL     0x105
#define CAN_ID_STATUS_BASE      0x200
#define CAN_ID_UID_RESPONSE_BASE 0x280

// Valve states
#define VALVE_STATE_CLOSED      0x00
#define VALVE_STATE_OPEN        0x01
#define VALVE_STATE_OPENING     0x02
#define VALVE_STATE_CLOSING     0x03

// Pin definitions
#define PIN_CAN_CS      10
#define PIN_CAN_INT     9
#define PIN_DIP_1       2
#define PIN_DIP_2       3
#define PIN_DIP_3       4
#define PIN_DIP_4       5
#define PIN_DIP_5       6
#define PIN_DIP_6       7

static MCP2515 canBus(PIN_CAN_CS);
static uint8_t deviceAddress = 0;
static uint8_t deviceUID[8];
static uint8_t valveState = VALVE_STATE_CLOSED;
static uint16_t motorCurrent = 0;

// Statistics
static uint32_t commandsReceived = 0;
static uint32_t responseSent = 0;

void readDeviceAddress(void) {
    pinMode(PIN_DIP_1, INPUT_PULLUP);
    pinMode(PIN_DIP_2, INPUT_PULLUP);
    pinMode(PIN_DIP_3, INPUT_PULLUP);
    pinMode(PIN_DIP_4, INPUT_PULLUP);
    pinMode(PIN_DIP_5, INPUT_PULLUP);
    pinMode(PIN_DIP_6, INPUT_PULLUP);
    
    deviceAddress = 0;
    if (digitalRead(PIN_DIP_1) == LOW) deviceAddress |= (1 << 0);
    if (digitalRead(PIN_DIP_2) == LOW) deviceAddress |= (1 << 1);
    if (digitalRead(PIN_DIP_3) == LOW) deviceAddress |= (1 << 2);
    if (digitalRead(PIN_DIP_4) == LOW) deviceAddress |= (1 << 3);
    if (digitalRead(PIN_DIP_5) == LOW) deviceAddress |= (1 << 4);
    if (digitalRead(PIN_DIP_6) == LOW) deviceAddress |= (1 << 5);
    
    deviceAddress += 1;  // Convert 0-63 to 1-64
}

void readDeviceUID(void) {
    uint32_t deviceId0 = NRF_FICR->DEVICEID[0];
    uint32_t deviceId1 = NRF_FICR->DEVICEID[1];
    
    deviceUID[0] = (deviceId0 >> 0) & 0xFF;
    deviceUID[1] = (deviceId0 >> 8) & 0xFF;
    deviceUID[2] = (deviceId0 >> 16) & 0xFF;
    deviceUID[3] = (deviceId0 >> 24) & 0xFF;
    deviceUID[4] = (deviceId1 >> 0) & 0xFF;
    deviceUID[5] = (deviceId1 >> 8) & 0xFF;
    deviceUID[6] = (deviceId1 >> 16) & 0xFF;
    deviceUID[7] = (deviceId1 >> 24) & 0xFF;
}

void sendUID(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_UID_RESPONSE_BASE + deviceAddress;
    frame.can_dlc = 8;
    memcpy(frame.data, deviceUID, 8);
    
    if (canBus.sendMessage(&frame) == MCP2515::ERROR_OK) {
        responseSent++;
        Serial.println("[ACTUATOR] Sent UID response");
    }
}

void sendStatus(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_STATUS_BASE + deviceAddress;
    frame.can_dlc = 3;
    frame.data[0] = valveState;
    frame.data[1] = (motorCurrent >> 8) & 0xFF;
    frame.data[2] = motorCurrent & 0xFF;
    
    if (canBus.sendMessage(&frame) == MCP2515::ERROR_OK) {
        responseSent++;
        Serial.printf("[ACTUATOR] Sent status: state=0x%02X, current=%dmA\n", valveState, motorCurrent);
    }
}

void handleValveOpen(void) {
    Serial.println("[ACTUATOR] Received OPEN command");
    commandsReceived++;
    
    if (valveState == VALVE_STATE_OPEN) {
        Serial.println("[ACTUATOR] Already open");
    } else {
        valveState = VALVE_STATE_OPENING;
        motorCurrent = 500;  // Simulate motor current
        Serial.println("[ACTUATOR] Opening valve...");
    }
    
    sendStatus();
    
    // Simulate valve opening (in real firmware this would be async)
    delay(1000);
    valveState = VALVE_STATE_OPEN;
    motorCurrent = 0;
    Serial.println("[ACTUATOR] Valve now OPEN");
}

void handleValveClose(void) {
    Serial.println("[ACTUATOR] Received CLOSE command");
    commandsReceived++;
    
    if (valveState == VALVE_STATE_CLOSED) {
        Serial.println("[ACTUATOR] Already closed");
    } else {
        valveState = VALVE_STATE_CLOSING;
        motorCurrent = 500;  // Simulate motor current
        Serial.println("[ACTUATOR] Closing valve...");
    }
    
    sendStatus();
    
    // Simulate valve closing
    delay(1000);
    valveState = VALVE_STATE_CLOSED;
    motorCurrent = 0;
    Serial.println("[ACTUATOR] Valve now CLOSED");
}

void handleValveStop(void) {
    Serial.println("[ACTUATOR] Received STOP command");
    commandsReceived++;
    
    motorCurrent = 0;
    // Keep current state (could be mid-transition)
    
    sendStatus();
}

void handleValveQuery(void) {
    Serial.println("[ACTUATOR] Received QUERY command");
    commandsReceived++;
    sendStatus();
}

void handleUIDQuery(void) {
    Serial.println("[ACTUATOR] Received UID query");
    commandsReceived++;
    sendUID();
}

void handleDiscoveryBroadcast(void) {
    Serial.println("[ACTUATOR] Received discovery broadcast");
    commandsReceived++;
    
    // Stagger response based on address to avoid collisions
    delay(deviceAddress * 5);
    sendUID();
}

void processCANMessage(struct can_frame* frame) {
    switch (frame->can_id) {
        case CAN_ID_VALVE_OPEN:
            if (frame->can_dlc >= 1 && frame->data[0] == deviceAddress) {
                handleValveOpen();
            }
            break;
            
        case CAN_ID_VALVE_CLOSE:
            if (frame->can_dlc >= 1 && frame->data[0] == deviceAddress) {
                handleValveClose();
            }
            break;
            
        case CAN_ID_VALVE_STOP:
            if (frame->can_dlc >= 1 && frame->data[0] == deviceAddress) {
                handleValveStop();
            }
            break;
            
        case CAN_ID_VALVE_QUERY:
            if (frame->can_dlc >= 1 && frame->data[0] == deviceAddress) {
                handleValveQuery();
            }
            break;
            
        case CAN_ID_UID_QUERY:
            if (frame->can_dlc >= 1 && frame->data[0] == deviceAddress) {
                handleUIDQuery();
            }
            break;
            
        case CAN_ID_DISCOVER_ALL:
            handleDiscoveryBroadcast();
            break;
    }
}

void setup() {
    delay(1000);
    Serial.begin(115200);
    
    Serial.println("\n========================================");
    Serial.println("Valve Actuator - Integration Test Mode");
    Serial.println("========================================\n");
    
    // Read device address from DIP switches
    readDeviceAddress();
    Serial.printf("[ACTUATOR] Device address: %d\n", deviceAddress);
    
    // Read device UID
    readDeviceUID();
    Serial.printf("[ACTUATOR] Device UID: ");
    for (int i = 0; i < 8; i++) {
        Serial.printf("%02X", deviceUID[i]);
    }
    Serial.println();
    
    // Initialize CAN bus
    SPI.begin();
    canBus.reset();
    canBus.setBitrate(CAN_125KBPS, MCP_8MHZ);
    canBus.setNormalMode();
    
    Serial.println("[ACTUATOR] CAN bus initialized");
    Serial.println("[ACTUATOR] Waiting for commands...\n");
}

void loop() {
    struct can_frame frame;
    
    if (canBus.readMessage(&frame) == MCP2515::ERROR_OK) {
        processCANMessage(&frame);
    }
    
    // Periodic status
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 10000) {
        Serial.printf("[ACTUATOR] Stats: commands=%lu, responses=%lu, state=0x%02X\n",
                      commandsReceived, responseSent, valveState);
        lastStatus = millis();
    }
}
