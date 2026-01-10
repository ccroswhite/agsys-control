/**
 * @file test_can_bus.cpp
 * @brief Embedded tests for CAN bus hardware functionality
 * 
 * These tests run on the actual nRF52832 hardware with MCP2515 CAN controller.
 * Run with: pio test -e test_embedded -f test_can_bus
 * 
 * REQUIREMENTS:
 * - Valve controller board connected via SWD
 * - CAN bus properly terminated (120 ohm resistors)
 * - Optional: Valve actuator connected for loopback tests
 */

#include <Arduino.h>
#include <unity.h>
#include <SPI.h>
#include <mcp2515.h>

// Pin definitions (from config.h)
#define PIN_CAN_CS      10
#define PIN_CAN_INT     9

static MCP2515* canBus = nullptr;

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

// =============================================================================
// TEST CASES
// =============================================================================

void test_mcp2515_init(void) {
    canBus = new MCP2515(PIN_CAN_CS);
    MCP2515::ERROR result = canBus->reset();
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    result = canBus->setBitrate(CAN_125KBPS, MCP_8MHZ);
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    result = canBus->setNormalMode();
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
}

void test_mcp2515_loopback_mode(void) {
    // Set loopback mode for self-test
    MCP2515::ERROR result = canBus->setLoopbackMode();
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    // Send a test frame
    struct can_frame txFrame;
    txFrame.can_id = 0x123;
    txFrame.can_dlc = 4;
    txFrame.data[0] = 0xDE;
    txFrame.data[1] = 0xAD;
    txFrame.data[2] = 0xBE;
    txFrame.data[3] = 0xEF;
    
    result = canBus->sendMessage(&txFrame);
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    // Wait for loopback
    delay(10);
    
    // Receive the frame
    struct can_frame rxFrame;
    result = canBus->readMessage(&rxFrame);
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    TEST_ASSERT_EQUAL_UINT32(0x123, rxFrame.can_id);
    TEST_ASSERT_EQUAL_UINT8(4, rxFrame.can_dlc);
    TEST_ASSERT_EQUAL_UINT8(0xDE, rxFrame.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAD, rxFrame.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, rxFrame.data[2]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, rxFrame.data[3]);
    
    // Return to normal mode
    canBus->setNormalMode();
}

void test_mcp2515_filter_setup(void) {
    // Test setting up filters for actuator responses
    MCP2515::ERROR result;
    
    // Accept status responses (0x200-0x23F)
    result = canBus->setFilterMask(MCP2515::MASK0, false, 0x7C0);
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    result = canBus->setFilter(MCP2515::RXF0, false, 0x200);
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
}

void test_can_interrupt_pin(void) {
    // Verify interrupt pin is configured correctly
    pinMode(PIN_CAN_INT, INPUT_PULLUP);
    
    // In idle state with no messages, INT should be HIGH
    // (active low when message received)
    int pinState = digitalRead(PIN_CAN_INT);
    TEST_ASSERT_EQUAL(HIGH, pinState);
}

// =============================================================================
// MAIN
// =============================================================================

void setup() {
    delay(2000);  // Wait for serial monitor
    
    SPI.begin();
    
    UNITY_BEGIN();
    
    RUN_TEST(test_mcp2515_init);
    RUN_TEST(test_mcp2515_loopback_mode);
    RUN_TEST(test_mcp2515_filter_setup);
    RUN_TEST(test_can_interrupt_pin);
    
    UNITY_END();
    
    if (canBus) {
        delete canBus;
    }
}

void loop() {
    // Nothing to do
}
