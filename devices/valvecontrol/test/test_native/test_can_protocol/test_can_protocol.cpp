/**
 * @file test_can_protocol.cpp
 * @brief Unit tests for CAN bus protocol message encoding/decoding
 * 
 * Tests CAN message ID parsing and frame construction.
 * Run with: pio test -e native -f test_can_protocol
 */

#include <unity.h>
#include <string.h>
#include <stdint.h>

// CAN message IDs (from config.h)
#define CAN_ID_VALVE_OPEN       0x100
#define CAN_ID_VALVE_CLOSE      0x101
#define CAN_ID_VALVE_STOP       0x102
#define CAN_ID_VALVE_QUERY      0x103
#define CAN_ID_UID_QUERY        0x104
#define CAN_ID_DISCOVER_ALL     0x105
#define CAN_ID_STATUS_BASE      0x200
#define CAN_ID_UID_RESPONSE_BASE 0x280

#define ACTUATOR_ADDR_MIN 1
#define ACTUATOR_ADDR_MAX 64

// CAN frame structure (matches arduino-mcp2515)
struct can_frame {
    uint32_t can_id;
    uint8_t can_dlc;
    uint8_t data[8];
};

// Functions under test
bool build_valve_command_frame(struct can_frame* frame, uint8_t address, uint8_t command) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    uint32_t base_id;
    switch (command) {
        case 0: base_id = CAN_ID_VALVE_OPEN; break;
        case 1: base_id = CAN_ID_VALVE_CLOSE; break;
        case 2: base_id = CAN_ID_VALVE_STOP; break;
        case 3: base_id = CAN_ID_VALVE_QUERY; break;
        default: return false;
    }
    
    frame->can_id = base_id;
    frame->can_dlc = 1;
    frame->data[0] = address;
    return true;
}

bool parse_status_response(const struct can_frame* frame, uint8_t* address, uint8_t* status_flags, uint16_t* current_ma) {
    if (frame->can_id < CAN_ID_STATUS_BASE || 
        frame->can_id >= CAN_ID_STATUS_BASE + ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    if (frame->can_dlc < 3) {
        return false;
    }
    
    *address = frame->can_id - CAN_ID_STATUS_BASE;
    *status_flags = frame->data[0];
    *current_ma = (frame->data[1] << 8) | frame->data[2];
    return true;
}

bool parse_uid_response(const struct can_frame* frame, uint8_t* address, uint8_t* uid) {
    if (frame->can_id < CAN_ID_UID_RESPONSE_BASE || 
        frame->can_id >= CAN_ID_UID_RESPONSE_BASE + ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    if (frame->can_dlc != 8) {
        return false;
    }
    
    *address = frame->can_id - CAN_ID_UID_RESPONSE_BASE;
    memcpy(uid, frame->data, 8);
    return true;
}

bool build_discovery_frame(struct can_frame* frame) {
    frame->can_id = CAN_ID_DISCOVER_ALL;
    frame->can_dlc = 0;
    return true;
}

bool build_uid_query_frame(struct can_frame* frame, uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return false;
    }
    
    frame->can_id = CAN_ID_UID_QUERY;
    frame->can_dlc = 1;
    frame->data[0] = address;
    return true;
}

// Test fixtures
void setUp(void) {
    // Nothing to set up
}

void tearDown(void) {
    // Nothing to clean up
}

// =============================================================================
// TEST CASES - Valve Commands
// =============================================================================

void test_build_valve_open_command(void) {
    struct can_frame frame;
    TEST_ASSERT_TRUE(build_valve_command_frame(&frame, 5, 0));
    TEST_ASSERT_EQUAL_UINT32(CAN_ID_VALVE_OPEN, frame.can_id);
    TEST_ASSERT_EQUAL_UINT8(1, frame.can_dlc);
    TEST_ASSERT_EQUAL_UINT8(5, frame.data[0]);
}

void test_build_valve_close_command(void) {
    struct can_frame frame;
    TEST_ASSERT_TRUE(build_valve_command_frame(&frame, 10, 1));
    TEST_ASSERT_EQUAL_UINT32(CAN_ID_VALVE_CLOSE, frame.can_id);
    TEST_ASSERT_EQUAL_UINT8(10, frame.data[0]);
}

void test_build_valve_stop_command(void) {
    struct can_frame frame;
    TEST_ASSERT_TRUE(build_valve_command_frame(&frame, 64, 2));
    TEST_ASSERT_EQUAL_UINT32(CAN_ID_VALVE_STOP, frame.can_id);
    TEST_ASSERT_EQUAL_UINT8(64, frame.data[0]);
}

void test_build_valve_query_command(void) {
    struct can_frame frame;
    TEST_ASSERT_TRUE(build_valve_command_frame(&frame, 1, 3));
    TEST_ASSERT_EQUAL_UINT32(CAN_ID_VALVE_QUERY, frame.can_id);
    TEST_ASSERT_EQUAL_UINT8(1, frame.data[0]);
}

void test_build_valve_command_invalid_address_zero(void) {
    struct can_frame frame;
    TEST_ASSERT_FALSE(build_valve_command_frame(&frame, 0, 0));
}

void test_build_valve_command_invalid_address_too_high(void) {
    struct can_frame frame;
    TEST_ASSERT_FALSE(build_valve_command_frame(&frame, 65, 0));
}

void test_build_valve_command_invalid_command(void) {
    struct can_frame frame;
    TEST_ASSERT_FALSE(build_valve_command_frame(&frame, 5, 4));
    TEST_ASSERT_FALSE(build_valve_command_frame(&frame, 5, 255));
}

// =============================================================================
// TEST CASES - Status Response Parsing
// =============================================================================

void test_parse_status_response_valid(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_STATUS_BASE + 5;
    frame.can_dlc = 3;
    frame.data[0] = 0x01;  // status flags
    frame.data[1] = 0x01;  // current high byte
    frame.data[2] = 0xF4;  // current low byte (500 mA)
    
    uint8_t address, status_flags;
    uint16_t current_ma;
    
    TEST_ASSERT_TRUE(parse_status_response(&frame, &address, &status_flags, &current_ma));
    TEST_ASSERT_EQUAL_UINT8(5, address);
    TEST_ASSERT_EQUAL_UINT8(0x01, status_flags);
    TEST_ASSERT_EQUAL_UINT16(500, current_ma);
}

void test_parse_status_response_invalid_id_low(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_STATUS_BASE - 1;
    frame.can_dlc = 3;
    
    uint8_t address, status_flags;
    uint16_t current_ma;
    
    TEST_ASSERT_FALSE(parse_status_response(&frame, &address, &status_flags, &current_ma));
}

void test_parse_status_response_invalid_id_high(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_STATUS_BASE + 65;
    frame.can_dlc = 3;
    
    uint8_t address, status_flags;
    uint16_t current_ma;
    
    TEST_ASSERT_FALSE(parse_status_response(&frame, &address, &status_flags, &current_ma));
}

void test_parse_status_response_short_frame(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_STATUS_BASE + 5;
    frame.can_dlc = 2;  // Too short
    
    uint8_t address, status_flags;
    uint16_t current_ma;
    
    TEST_ASSERT_FALSE(parse_status_response(&frame, &address, &status_flags, &current_ma));
}

// =============================================================================
// TEST CASES - UID Response Parsing
// =============================================================================

void test_parse_uid_response_valid(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_UID_RESPONSE_BASE + 10;
    frame.can_dlc = 8;
    frame.data[0] = 0xAA;
    frame.data[1] = 0xBB;
    frame.data[2] = 0xCC;
    frame.data[3] = 0xDD;
    frame.data[4] = 0xEE;
    frame.data[5] = 0xFF;
    frame.data[6] = 0x11;
    frame.data[7] = 0x22;
    
    uint8_t address;
    uint8_t uid[8];
    
    TEST_ASSERT_TRUE(parse_uid_response(&frame, &address, uid));
    TEST_ASSERT_EQUAL_UINT8(10, address);
    TEST_ASSERT_EQUAL_UINT8(0xAA, uid[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22, uid[7]);
}

void test_parse_uid_response_invalid_dlc(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_UID_RESPONSE_BASE + 10;
    frame.can_dlc = 7;  // Must be exactly 8
    
    uint8_t address;
    uint8_t uid[8];
    
    TEST_ASSERT_FALSE(parse_uid_response(&frame, &address, uid));
}

void test_parse_uid_response_invalid_id(void) {
    struct can_frame frame;
    frame.can_id = CAN_ID_STATUS_BASE + 10;  // Wrong base
    frame.can_dlc = 8;
    
    uint8_t address;
    uint8_t uid[8];
    
    TEST_ASSERT_FALSE(parse_uid_response(&frame, &address, uid));
}

// =============================================================================
// TEST CASES - Discovery
// =============================================================================

void test_build_discovery_frame(void) {
    struct can_frame frame;
    TEST_ASSERT_TRUE(build_discovery_frame(&frame));
    TEST_ASSERT_EQUAL_UINT32(CAN_ID_DISCOVER_ALL, frame.can_id);
    TEST_ASSERT_EQUAL_UINT8(0, frame.can_dlc);
}

void test_build_uid_query_frame_valid(void) {
    struct can_frame frame;
    TEST_ASSERT_TRUE(build_uid_query_frame(&frame, 32));
    TEST_ASSERT_EQUAL_UINT32(CAN_ID_UID_QUERY, frame.can_id);
    TEST_ASSERT_EQUAL_UINT8(1, frame.can_dlc);
    TEST_ASSERT_EQUAL_UINT8(32, frame.data[0]);
}

void test_build_uid_query_frame_invalid_address(void) {
    struct can_frame frame;
    TEST_ASSERT_FALSE(build_uid_query_frame(&frame, 0));
    TEST_ASSERT_FALSE(build_uid_query_frame(&frame, 65));
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Valve commands
    RUN_TEST(test_build_valve_open_command);
    RUN_TEST(test_build_valve_close_command);
    RUN_TEST(test_build_valve_stop_command);
    RUN_TEST(test_build_valve_query_command);
    RUN_TEST(test_build_valve_command_invalid_address_zero);
    RUN_TEST(test_build_valve_command_invalid_address_too_high);
    RUN_TEST(test_build_valve_command_invalid_command);
    
    // Status response parsing
    RUN_TEST(test_parse_status_response_valid);
    RUN_TEST(test_parse_status_response_invalid_id_low);
    RUN_TEST(test_parse_status_response_invalid_id_high);
    RUN_TEST(test_parse_status_response_short_frame);
    
    // UID response parsing
    RUN_TEST(test_parse_uid_response_valid);
    RUN_TEST(test_parse_uid_response_invalid_dlc);
    RUN_TEST(test_parse_uid_response_invalid_id);
    
    // Discovery
    RUN_TEST(test_build_discovery_frame);
    RUN_TEST(test_build_uid_query_frame_valid);
    RUN_TEST(test_build_uid_query_frame_invalid_address);
    
    return UNITY_END();
}
