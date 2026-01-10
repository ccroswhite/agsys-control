/**
 * @file test_controller_side.cpp
 * @brief Integration test firmware for valve controller (CAN bus tests)
 * 
 * This firmware runs on the valve controller and tests CAN bus communication
 * with connected valve actuators.
 * 
 * REQUIREMENTS:
 * - Valve controller board
 * - One or more valve actuators connected via CAN bus
 * - CAN bus properly terminated
 * 
 * USAGE:
 * 1. Flash this to valve controller
 * 2. Flash test_actuator_side.cpp to actuator(s)
 * 3. Monitor serial output from both devices
 * 4. Tests run automatically on boot
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

#define PIN_CAN_CS      10
#define PIN_CAN_INT     9

#define MAX_ACTUATORS   64
#define TEST_TIMEOUT_MS 5000

static MCP2515 canBus(PIN_CAN_CS);

// Test state
static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

// Discovered actuators
static uint8_t discoveredAddresses[MAX_ACTUATORS];
static uint8_t discoveredCount = 0;

void logTest(const char* name) {
    Serial.printf("\n[INTEGRATION] Test: %s\n", name);
}

void logPass(const char* name) {
    Serial.printf("[INTEGRATION] PASS: %s\n", name);
    testsPassed++;
    testsRun++;
}

void logFail(const char* name, const char* reason) {
    Serial.printf("[INTEGRATION] FAIL: %s - %s\n", name, reason);
    testsFailed++;
    testsRun++;
}

void logInfo(const char* msg) {
    Serial.printf("[INTEGRATION] %s\n", msg);
}

bool waitForResponse(uint32_t baseId, uint32_t idMask, struct can_frame* frame, uint32_t timeout_ms) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        if (canBus.readMessage(frame) == MCP2515::ERROR_OK) {
            if ((frame->can_id & idMask) == baseId) {
                return true;
            }
        }
        delay(1);
    }
    return false;
}

// =============================================================================
// TEST: Discovery Broadcast
// =============================================================================

void test_discovery_broadcast(void) {
    logTest("discovery_broadcast");
    
    discoveredCount = 0;
    memset(discoveredAddresses, 0, sizeof(discoveredAddresses));
    
    // Send discovery broadcast
    struct can_frame frame;
    frame.can_id = CAN_ID_DISCOVER_ALL;
    frame.can_dlc = 0;
    
    logInfo("Sending discovery broadcast...");
    if (canBus.sendMessage(&frame) != MCP2515::ERROR_OK) {
        logFail("discovery_broadcast", "Failed to send broadcast");
        return;
    }
    
    // Wait for responses (up to 500ms for staggered responses)
    uint32_t start = millis();
    while (millis() - start < 500) {
        struct can_frame rxFrame;
        if (canBus.readMessage(&rxFrame) == MCP2515::ERROR_OK) {
            if (rxFrame.can_id >= CAN_ID_UID_RESPONSE_BASE && 
                rxFrame.can_id < CAN_ID_UID_RESPONSE_BASE + MAX_ACTUATORS) {
                
                uint8_t addr = rxFrame.can_id - CAN_ID_UID_RESPONSE_BASE;
                discoveredAddresses[discoveredCount++] = addr;
                
                Serial.printf("[INTEGRATION] Discovered actuator at address %d, UID=", addr);
                for (int i = 0; i < 8; i++) {
                    Serial.printf("%02X", rxFrame.data[i]);
                }
                Serial.println();
            }
        }
        delay(1);
    }
    
    if (discoveredCount > 0) {
        Serial.printf("[INTEGRATION] Discovered %d actuator(s)\n", discoveredCount);
        logPass("discovery_broadcast");
    } else {
        logFail("discovery_broadcast", "No actuators responded");
    }
}

// =============================================================================
// TEST: UID Query (specific address)
// =============================================================================

void test_uid_query(void) {
    logTest("uid_query");
    
    if (discoveredCount == 0) {
        logFail("uid_query", "No actuators discovered, skipping");
        return;
    }
    
    uint8_t targetAddr = discoveredAddresses[0];
    Serial.printf("[INTEGRATION] Querying UID for address %d...\n", targetAddr);
    
    struct can_frame frame;
    frame.can_id = CAN_ID_UID_QUERY;
    frame.can_dlc = 1;
    frame.data[0] = targetAddr;
    
    if (canBus.sendMessage(&frame) != MCP2515::ERROR_OK) {
        logFail("uid_query", "Failed to send query");
        return;
    }
    
    struct can_frame rxFrame;
    if (waitForResponse(CAN_ID_UID_RESPONSE_BASE + targetAddr, 0xFFF, &rxFrame, TEST_TIMEOUT_MS)) {
        Serial.printf("[INTEGRATION] Received UID response: ");
        for (int i = 0; i < 8; i++) {
            Serial.printf("%02X", rxFrame.data[i]);
        }
        Serial.println();
        logPass("uid_query");
    } else {
        logFail("uid_query", "Timeout waiting for response");
    }
}

// =============================================================================
// TEST: Valve Open Command
// =============================================================================

void test_valve_open(void) {
    logTest("valve_open");
    
    if (discoveredCount == 0) {
        logFail("valve_open", "No actuators discovered, skipping");
        return;
    }
    
    uint8_t targetAddr = discoveredAddresses[0];
    Serial.printf("[INTEGRATION] Sending OPEN command to address %d...\n", targetAddr);
    
    struct can_frame frame;
    frame.can_id = CAN_ID_VALVE_OPEN;
    frame.can_dlc = 1;
    frame.data[0] = targetAddr;
    
    if (canBus.sendMessage(&frame) != MCP2515::ERROR_OK) {
        logFail("valve_open", "Failed to send command");
        return;
    }
    
    // Wait for status response
    struct can_frame rxFrame;
    if (waitForResponse(CAN_ID_STATUS_BASE + targetAddr, 0xFFF, &rxFrame, TEST_TIMEOUT_MS)) {
        uint8_t state = rxFrame.data[0];
        Serial.printf("[INTEGRATION] Received status: state=0x%02X\n", state);
        
        // State should be OPENING (0x02) or OPEN (0x01)
        if (state == 0x01 || state == 0x02) {
            logPass("valve_open");
        } else {
            logFail("valve_open", "Unexpected state");
        }
    } else {
        logFail("valve_open", "Timeout waiting for status");
    }
}

// =============================================================================
// TEST: Valve Close Command
// =============================================================================

void test_valve_close(void) {
    logTest("valve_close");
    
    if (discoveredCount == 0) {
        logFail("valve_close", "No actuators discovered, skipping");
        return;
    }
    
    uint8_t targetAddr = discoveredAddresses[0];
    Serial.printf("[INTEGRATION] Sending CLOSE command to address %d...\n", targetAddr);
    
    struct can_frame frame;
    frame.can_id = CAN_ID_VALVE_CLOSE;
    frame.can_dlc = 1;
    frame.data[0] = targetAddr;
    
    if (canBus.sendMessage(&frame) != MCP2515::ERROR_OK) {
        logFail("valve_close", "Failed to send command");
        return;
    }
    
    // Wait for status response
    struct can_frame rxFrame;
    if (waitForResponse(CAN_ID_STATUS_BASE + targetAddr, 0xFFF, &rxFrame, TEST_TIMEOUT_MS)) {
        uint8_t state = rxFrame.data[0];
        Serial.printf("[INTEGRATION] Received status: state=0x%02X\n", state);
        
        // State should be CLOSING (0x03) or CLOSED (0x00)
        if (state == 0x00 || state == 0x03) {
            logPass("valve_close");
        } else {
            logFail("valve_close", "Unexpected state");
        }
    } else {
        logFail("valve_close", "Timeout waiting for status");
    }
}

// =============================================================================
// TEST: Status Query
// =============================================================================

void test_status_query(void) {
    logTest("status_query");
    
    if (discoveredCount == 0) {
        logFail("status_query", "No actuators discovered, skipping");
        return;
    }
    
    uint8_t targetAddr = discoveredAddresses[0];
    Serial.printf("[INTEGRATION] Querying status for address %d...\n", targetAddr);
    
    struct can_frame frame;
    frame.can_id = CAN_ID_VALVE_QUERY;
    frame.can_dlc = 1;
    frame.data[0] = targetAddr;
    
    if (canBus.sendMessage(&frame) != MCP2515::ERROR_OK) {
        logFail("status_query", "Failed to send query");
        return;
    }
    
    struct can_frame rxFrame;
    if (waitForResponse(CAN_ID_STATUS_BASE + targetAddr, 0xFFF, &rxFrame, TEST_TIMEOUT_MS)) {
        uint8_t state = rxFrame.data[0];
        uint16_t current = (rxFrame.data[1] << 8) | rxFrame.data[2];
        Serial.printf("[INTEGRATION] Status: state=0x%02X, current=%dmA\n", state, current);
        logPass("status_query");
    } else {
        logFail("status_query", "Timeout waiting for response");
    }
}

// =============================================================================
// MAIN
// =============================================================================

void setup() {
    delay(2000);
    Serial.begin(115200);
    
    Serial.println("\n========================================");
    Serial.println("Valve Controller - CAN Bus Integration Tests");
    Serial.println("========================================\n");
    
    // Initialize CAN bus
    SPI.begin();
    canBus.reset();
    canBus.setBitrate(CAN_125KBPS, MCP_8MHZ);
    canBus.setNormalMode();
    
    Serial.println("[INTEGRATION] CAN bus initialized");
    Serial.println("[INTEGRATION] Waiting 2 seconds for actuators to boot...\n");
    delay(2000);
    
    // Run tests
    test_discovery_broadcast();
    delay(500);
    
    test_uid_query();
    delay(500);
    
    test_valve_open();
    delay(2000);  // Wait for valve to open
    
    test_valve_close();
    delay(2000);  // Wait for valve to close
    
    test_status_query();
    
    // Summary
    Serial.println("\n========================================");
    Serial.printf("Tests Run: %d\n", testsRun);
    Serial.printf("Passed: %d\n", testsPassed);
    Serial.printf("Failed: %d\n", testsFailed);
    Serial.println("========================================");
    
    if (testsFailed == 0) {
        Serial.println("\n*** ALL TESTS PASSED ***\n");
    } else {
        Serial.println("\n*** SOME TESTS FAILED ***\n");
    }
}

void loop() {
    // Nothing to do - tests complete
    delay(10000);
}
