/**
 * @file test_uid_mapping.cpp
 * @brief Unit tests for UID-to-CAN-address mapping
 * 
 * Tests the actuator UID lookup and mapping functionality.
 * Run with: pio test -e native -f test_uid_mapping
 */

#include <unity.h>
#include <string.h>
#include <stdint.h>

// Mock the actuator status structure (matches can_bus.h)
typedef uint8_t ActuatorUID[8];

typedef struct {
    uint8_t address;
    bool online;
    bool uid_known;
    ActuatorUID uid;
    uint8_t status_flags;
    uint16_t current_ma;
    uint32_t last_seen;
} ActuatorStatus;

#define MAX_ACTUATORS 64
#define ACTUATOR_ADDR_MIN 1
#define ACTUATOR_ADDR_MAX 64

static ActuatorStatus actuators[MAX_ACTUATORS];

// Helper functions under test (reimplemented for native testing)
void init_actuators(void) {
    for (int i = 0; i < MAX_ACTUATORS; i++) {
        actuators[i].address = i + 1;
        actuators[i].online = false;
        actuators[i].uid_known = false;
        memset(actuators[i].uid, 0, sizeof(ActuatorUID));
        actuators[i].status_flags = 0;
        actuators[i].current_ma = 0;
        actuators[i].last_seen = 0;
    }
}

bool uid_equals(const ActuatorUID a, const ActuatorUID b) {
    return memcmp(a, b, sizeof(ActuatorUID)) == 0;
}

int8_t lookup_address_by_uid(const ActuatorUID uid) {
    for (int i = 0; i < MAX_ACTUATORS; i++) {
        if (actuators[i].online && actuators[i].uid_known) {
            if (uid_equals(actuators[i].uid, uid)) {
                return actuators[i].address;
            }
        }
    }
    return -1;  // Not found
}

ActuatorStatus* get_actuator(uint8_t address) {
    if (address < ACTUATOR_ADDR_MIN || address > ACTUATOR_ADDR_MAX) {
        return NULL;
    }
    return &actuators[address - 1];
}

// Test fixtures
void setUp(void) {
    init_actuators();
}

void tearDown(void) {
    // Nothing to clean up
}

// =============================================================================
// TEST CASES
// =============================================================================

void test_init_all_actuators_offline(void) {
    for (int i = 0; i < MAX_ACTUATORS; i++) {
        TEST_ASSERT_FALSE(actuators[i].online);
        TEST_ASSERT_FALSE(actuators[i].uid_known);
        TEST_ASSERT_EQUAL_UINT8(i + 1, actuators[i].address);
    }
}

void test_uid_equals_same(void) {
    ActuatorUID a = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    ActuatorUID b = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    TEST_ASSERT_TRUE(uid_equals(a, b));
}

void test_uid_equals_different(void) {
    ActuatorUID a = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    ActuatorUID b = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09};
    TEST_ASSERT_FALSE(uid_equals(a, b));
}

void test_uid_equals_all_zeros(void) {
    ActuatorUID a = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ActuatorUID b = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_TRUE(uid_equals(a, b));
}

void test_lookup_uid_not_found_empty(void) {
    ActuatorUID uid = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    int8_t addr = lookup_address_by_uid(uid);
    TEST_ASSERT_EQUAL_INT8(-1, addr);
}

void test_lookup_uid_found(void) {
    // Set up actuator at address 5 with a known UID
    ActuatorUID uid = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    actuators[4].online = true;
    actuators[4].uid_known = true;
    memcpy(actuators[4].uid, uid, sizeof(ActuatorUID));
    
    int8_t addr = lookup_address_by_uid(uid);
    TEST_ASSERT_EQUAL_INT8(5, addr);
}

void test_lookup_uid_not_found_offline(void) {
    // Set up actuator with UID but mark as offline
    ActuatorUID uid = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    actuators[4].online = false;
    actuators[4].uid_known = true;
    memcpy(actuators[4].uid, uid, sizeof(ActuatorUID));
    
    int8_t addr = lookup_address_by_uid(uid);
    TEST_ASSERT_EQUAL_INT8(-1, addr);
}

void test_lookup_uid_not_found_uid_unknown(void) {
    // Set up actuator online but UID not known
    ActuatorUID uid = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    actuators[4].online = true;
    actuators[4].uid_known = false;
    memcpy(actuators[4].uid, uid, sizeof(ActuatorUID));
    
    int8_t addr = lookup_address_by_uid(uid);
    TEST_ASSERT_EQUAL_INT8(-1, addr);
}

void test_lookup_uid_multiple_actuators(void) {
    // Set up multiple actuators
    ActuatorUID uid1 = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
    ActuatorUID uid2 = {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02};
    ActuatorUID uid3 = {0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03};
    
    actuators[0].online = true;
    actuators[0].uid_known = true;
    memcpy(actuators[0].uid, uid1, sizeof(ActuatorUID));
    
    actuators[9].online = true;
    actuators[9].uid_known = true;
    memcpy(actuators[9].uid, uid2, sizeof(ActuatorUID));
    
    actuators[63].online = true;
    actuators[63].uid_known = true;
    memcpy(actuators[63].uid, uid3, sizeof(ActuatorUID));
    
    TEST_ASSERT_EQUAL_INT8(1, lookup_address_by_uid(uid1));
    TEST_ASSERT_EQUAL_INT8(10, lookup_address_by_uid(uid2));
    TEST_ASSERT_EQUAL_INT8(64, lookup_address_by_uid(uid3));
}

void test_get_actuator_valid_address(void) {
    ActuatorStatus* status = get_actuator(1);
    TEST_ASSERT_NOT_NULL(status);
    TEST_ASSERT_EQUAL_UINT8(1, status->address);
    
    status = get_actuator(64);
    TEST_ASSERT_NOT_NULL(status);
    TEST_ASSERT_EQUAL_UINT8(64, status->address);
}

void test_get_actuator_invalid_address(void) {
    TEST_ASSERT_NULL(get_actuator(0));
    TEST_ASSERT_NULL(get_actuator(65));
    TEST_ASSERT_NULL(get_actuator(255));
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_init_all_actuators_offline);
    RUN_TEST(test_uid_equals_same);
    RUN_TEST(test_uid_equals_different);
    RUN_TEST(test_uid_equals_all_zeros);
    RUN_TEST(test_lookup_uid_not_found_empty);
    RUN_TEST(test_lookup_uid_found);
    RUN_TEST(test_lookup_uid_not_found_offline);
    RUN_TEST(test_lookup_uid_not_found_uid_unknown);
    RUN_TEST(test_lookup_uid_multiple_actuators);
    RUN_TEST(test_get_actuator_valid_address);
    RUN_TEST(test_get_actuator_invalid_address);
    
    return UNITY_END();
}
