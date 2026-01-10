/**
 * @file test_hardware.cpp
 * @brief Embedded tests for valve actuator hardware
 * 
 * These tests run on the actual nRF52 hardware.
 * Run with: pio test -e test_embedded -f test_hardware
 * 
 * REQUIREMENTS:
 * - Valve actuator board connected via SWD
 * - DIP switches set to a known address
 * - H-bridge and motor NOT connected (tests H-bridge signals only)
 */

#include <Arduino.h>
#include <unity.h>
#include <SPI.h>
#include <mcp2515.h>

// Pin definitions (from config.h)
#define PIN_CAN_CS          10
#define PIN_CAN_INT         9
#define PIN_HBRIDGE_A       14
#define PIN_HBRIDGE_B       15
#define PIN_HBRIDGE_EN      16
#define PIN_LIMIT_OPEN      17
#define PIN_LIMIT_CLOSE     18
#define PIN_DIP_1           2
#define PIN_DIP_2           3
#define PIN_DIP_3           4
#define PIN_DIP_4           5
#define PIN_DIP_5           6
#define PIN_DIP_6           7
#define PIN_CURRENT_SENSE   A0

static MCP2515* canBus = nullptr;

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
    // Ensure H-bridge is off after each test
    digitalWrite(PIN_HBRIDGE_EN, LOW);
    digitalWrite(PIN_HBRIDGE_A, LOW);
    digitalWrite(PIN_HBRIDGE_B, LOW);
}

// =============================================================================
// TEST CASES - GPIO Configuration
// =============================================================================

void test_hbridge_pins_output(void) {
    pinMode(PIN_HBRIDGE_A, OUTPUT);
    pinMode(PIN_HBRIDGE_B, OUTPUT);
    pinMode(PIN_HBRIDGE_EN, OUTPUT);
    
    // Test A pin
    digitalWrite(PIN_HBRIDGE_A, HIGH);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN_HBRIDGE_A));
    digitalWrite(PIN_HBRIDGE_A, LOW);
    TEST_ASSERT_EQUAL(LOW, digitalRead(PIN_HBRIDGE_A));
    
    // Test B pin
    digitalWrite(PIN_HBRIDGE_B, HIGH);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN_HBRIDGE_B));
    digitalWrite(PIN_HBRIDGE_B, LOW);
    TEST_ASSERT_EQUAL(LOW, digitalRead(PIN_HBRIDGE_B));
    
    // Test EN pin
    digitalWrite(PIN_HBRIDGE_EN, HIGH);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN_HBRIDGE_EN));
    digitalWrite(PIN_HBRIDGE_EN, LOW);
    TEST_ASSERT_EQUAL(LOW, digitalRead(PIN_HBRIDGE_EN));
}

void test_limit_switch_pins_input(void) {
    pinMode(PIN_LIMIT_OPEN, INPUT_PULLUP);
    pinMode(PIN_LIMIT_CLOSE, INPUT_PULLUP);
    
    // With pullups, pins should read HIGH when switches are open
    // (Actual state depends on physical switch position)
    int open_state = digitalRead(PIN_LIMIT_OPEN);
    int close_state = digitalRead(PIN_LIMIT_CLOSE);
    
    // Just verify we can read them without error
    TEST_ASSERT_TRUE(open_state == HIGH || open_state == LOW);
    TEST_ASSERT_TRUE(close_state == HIGH || close_state == LOW);
}

void test_dip_switch_pins_input(void) {
    pinMode(PIN_DIP_1, INPUT_PULLUP);
    pinMode(PIN_DIP_2, INPUT_PULLUP);
    pinMode(PIN_DIP_3, INPUT_PULLUP);
    pinMode(PIN_DIP_4, INPUT_PULLUP);
    pinMode(PIN_DIP_5, INPUT_PULLUP);
    pinMode(PIN_DIP_6, INPUT_PULLUP);
    
    // Read all DIP switches
    uint8_t address = 0;
    if (digitalRead(PIN_DIP_1) == LOW) address |= (1 << 0);
    if (digitalRead(PIN_DIP_2) == LOW) address |= (1 << 1);
    if (digitalRead(PIN_DIP_3) == LOW) address |= (1 << 2);
    if (digitalRead(PIN_DIP_4) == LOW) address |= (1 << 3);
    if (digitalRead(PIN_DIP_5) == LOW) address |= (1 << 4);
    if (digitalRead(PIN_DIP_6) == LOW) address |= (1 << 5);
    
    // Address should be 1-64 (0 is invalid)
    // Just verify we get a valid reading
    TEST_ASSERT_TRUE(address <= 63);
    
    // Print for manual verification
    Serial.printf("DIP switch address: %d\n", address + 1);
}

// =============================================================================
// TEST CASES - CAN Bus
// =============================================================================

void test_can_init(void) {
    canBus = new MCP2515(PIN_CAN_CS);
    MCP2515::ERROR result = canBus->reset();
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    result = canBus->setBitrate(CAN_125KBPS, MCP_8MHZ);
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    result = canBus->setNormalMode();
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
}

void test_can_loopback(void) {
    MCP2515::ERROR result = canBus->setLoopbackMode();
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    struct can_frame txFrame;
    txFrame.can_id = 0x100;
    txFrame.can_dlc = 1;
    txFrame.data[0] = 0x05;  // Address 5
    
    result = canBus->sendMessage(&txFrame);
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    
    delay(10);
    
    struct can_frame rxFrame;
    result = canBus->readMessage(&rxFrame);
    TEST_ASSERT_EQUAL(MCP2515::ERROR_OK, result);
    TEST_ASSERT_EQUAL_UINT32(0x100, rxFrame.can_id);
    TEST_ASSERT_EQUAL_UINT8(0x05, rxFrame.data[0]);
    
    canBus->setNormalMode();
}

// =============================================================================
// TEST CASES - ADC (Current Sense)
// =============================================================================

void test_current_sense_adc(void) {
    // Read current sense ADC
    // With motor off, should read near zero
    int reading = analogRead(PIN_CURRENT_SENSE);
    
    // nRF52 ADC is 10-bit (0-1023) or 12-bit depending on config
    TEST_ASSERT_TRUE(reading >= 0);
    TEST_ASSERT_TRUE(reading < 1024);
    
    Serial.printf("Current sense ADC reading: %d\n", reading);
    
    // With no motor connected, expect low reading (noise floor)
    // Allow some margin for noise
    TEST_ASSERT_TRUE(reading < 100);
}

// =============================================================================
// TEST CASES - Device UID
// =============================================================================

void test_device_uid_readable(void) {
    uint32_t deviceId0 = NRF_FICR->DEVICEID[0];
    uint32_t deviceId1 = NRF_FICR->DEVICEID[1];
    
    // UID should not be all zeros or all ones
    TEST_ASSERT_TRUE(deviceId0 != 0x00000000);
    TEST_ASSERT_TRUE(deviceId1 != 0x00000000);
    TEST_ASSERT_TRUE(deviceId0 != 0xFFFFFFFF);
    TEST_ASSERT_TRUE(deviceId1 != 0xFFFFFFFF);
    
    Serial.printf("Device UID: %08lX%08lX\n", deviceId1, deviceId0);
}

// =============================================================================
// MAIN
// =============================================================================

void setup() {
    delay(2000);  // Wait for serial monitor
    Serial.begin(115200);
    
    SPI.begin();
    
    UNITY_BEGIN();
    
    // GPIO tests
    RUN_TEST(test_hbridge_pins_output);
    RUN_TEST(test_limit_switch_pins_input);
    RUN_TEST(test_dip_switch_pins_input);
    
    // CAN bus tests
    RUN_TEST(test_can_init);
    RUN_TEST(test_can_loopback);
    
    // ADC tests
    RUN_TEST(test_current_sense_adc);
    
    // UID test
    RUN_TEST(test_device_uid_readable);
    
    UNITY_END();
    
    if (canBus) {
        delete canBus;
    }
}

void loop() {
    // Nothing to do
}
