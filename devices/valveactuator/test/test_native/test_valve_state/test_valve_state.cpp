/**
 * @file test_valve_state.cpp
 * @brief Unit tests for valve state machine logic
 * 
 * Tests the valve actuator state transitions and command handling.
 * Run with: pio test -e native -f test_valve_state
 */

#include <unity.h>
#include <string.h>
#include <stdint.h>

// Valve states (from config.h)
#define VALVE_STATE_CLOSED      0x00
#define VALVE_STATE_OPEN        0x01
#define VALVE_STATE_OPENING     0x02
#define VALVE_STATE_CLOSING     0x03
#define VALVE_STATE_ERROR       0xFF

// Valve commands
#define VALVE_CMD_OPEN          0x00
#define VALVE_CMD_CLOSE         0x01
#define VALVE_CMD_STOP          0x02

// Status flags
#define STATUS_FLAG_LIMIT_OPEN  (1 << 0)
#define STATUS_FLAG_LIMIT_CLOSE (1 << 1)
#define STATUS_FLAG_OVERCURRENT (1 << 2)
#define STATUS_FLAG_TIMEOUT     (1 << 3)

// Simulated valve state
typedef struct {
    uint8_t state;
    uint8_t status_flags;
    uint16_t current_ma;
    uint32_t operation_start_time;
    bool motor_running;
    int8_t motor_direction;  // 1 = opening, -1 = closing, 0 = stopped
} ValveState;

static ValveState valve;

// Simulated time
static uint32_t mock_millis = 0;

uint32_t get_millis(void) {
    return mock_millis;
}

void advance_time(uint32_t ms) {
    mock_millis += ms;
}

// State machine functions under test
void valve_init(void) {
    valve.state = VALVE_STATE_CLOSED;
    valve.status_flags = STATUS_FLAG_LIMIT_CLOSE;
    valve.current_ma = 0;
    valve.operation_start_time = 0;
    valve.motor_running = false;
    valve.motor_direction = 0;
}

bool valve_start_open(void) {
    if (valve.state == VALVE_STATE_OPEN) {
        return true;  // Already open
    }
    if (valve.state == VALVE_STATE_OPENING || valve.state == VALVE_STATE_CLOSING) {
        return false;  // Already in motion
    }
    
    valve.state = VALVE_STATE_OPENING;
    valve.motor_running = true;
    valve.motor_direction = 1;
    valve.operation_start_time = get_millis();
    valve.status_flags &= ~STATUS_FLAG_LIMIT_CLOSE;
    return true;
}

bool valve_start_close(void) {
    if (valve.state == VALVE_STATE_CLOSED) {
        return true;  // Already closed
    }
    if (valve.state == VALVE_STATE_OPENING || valve.state == VALVE_STATE_CLOSING) {
        return false;  // Already in motion
    }
    
    valve.state = VALVE_STATE_CLOSING;
    valve.motor_running = true;
    valve.motor_direction = -1;
    valve.operation_start_time = get_millis();
    valve.status_flags &= ~STATUS_FLAG_LIMIT_OPEN;
    return true;
}

void valve_stop(void) {
    valve.motor_running = false;
    valve.motor_direction = 0;
    valve.current_ma = 0;
    
    // State becomes error if stopped mid-motion
    if (valve.state == VALVE_STATE_OPENING || valve.state == VALVE_STATE_CLOSING) {
        valve.state = VALVE_STATE_ERROR;
    }
}

void valve_on_limit_open(void) {
    if (valve.state == VALVE_STATE_OPENING) {
        valve.state = VALVE_STATE_OPEN;
        valve.motor_running = false;
        valve.motor_direction = 0;
        valve.current_ma = 0;
    }
    valve.status_flags |= STATUS_FLAG_LIMIT_OPEN;
}

void valve_on_limit_close(void) {
    if (valve.state == VALVE_STATE_CLOSING) {
        valve.state = VALVE_STATE_CLOSED;
        valve.motor_running = false;
        valve.motor_direction = 0;
        valve.current_ma = 0;
    }
    valve.status_flags |= STATUS_FLAG_LIMIT_CLOSE;
}

bool valve_check_timeout(uint32_t timeout_ms) {
    if (!valve.motor_running) {
        return false;
    }
    
    if (get_millis() - valve.operation_start_time > timeout_ms) {
        valve.state = VALVE_STATE_ERROR;
        valve.status_flags |= STATUS_FLAG_TIMEOUT;
        valve.motor_running = false;
        valve.motor_direction = 0;
        return true;
    }
    return false;
}

bool valve_check_overcurrent(uint16_t threshold_ma) {
    if (valve.current_ma > threshold_ma) {
        valve.state = VALVE_STATE_ERROR;
        valve.status_flags |= STATUS_FLAG_OVERCURRENT;
        valve.motor_running = false;
        valve.motor_direction = 0;
        return true;
    }
    return false;
}

// Test fixtures
void setUp(void) {
    mock_millis = 0;
    valve_init();
}

void tearDown(void) {
    // Nothing to clean up
}

// =============================================================================
// TEST CASES - Initialization
// =============================================================================

void test_valve_init_state(void) {
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_CLOSED, valve.state);
    TEST_ASSERT_TRUE(valve.status_flags & STATUS_FLAG_LIMIT_CLOSE);
    TEST_ASSERT_FALSE(valve.motor_running);
}

// =============================================================================
// TEST CASES - Open Command
// =============================================================================

void test_valve_open_from_closed(void) {
    TEST_ASSERT_TRUE(valve_start_open());
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_OPENING, valve.state);
    TEST_ASSERT_TRUE(valve.motor_running);
    TEST_ASSERT_EQUAL_INT8(1, valve.motor_direction);
}

void test_valve_open_when_already_open(void) {
    valve.state = VALVE_STATE_OPEN;
    valve.status_flags = STATUS_FLAG_LIMIT_OPEN;
    
    TEST_ASSERT_TRUE(valve_start_open());
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_OPEN, valve.state);
    TEST_ASSERT_FALSE(valve.motor_running);
}

void test_valve_open_while_opening(void) {
    valve_start_open();
    TEST_ASSERT_FALSE(valve_start_open());  // Should reject
}

void test_valve_open_while_closing(void) {
    valve.state = VALVE_STATE_OPEN;
    valve_start_close();
    TEST_ASSERT_FALSE(valve_start_open());  // Should reject
}

// =============================================================================
// TEST CASES - Close Command
// =============================================================================

void test_valve_close_from_open(void) {
    valve.state = VALVE_STATE_OPEN;
    valve.status_flags = STATUS_FLAG_LIMIT_OPEN;
    
    TEST_ASSERT_TRUE(valve_start_close());
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_CLOSING, valve.state);
    TEST_ASSERT_TRUE(valve.motor_running);
    TEST_ASSERT_EQUAL_INT8(-1, valve.motor_direction);
}

void test_valve_close_when_already_closed(void) {
    TEST_ASSERT_TRUE(valve_start_close());
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_CLOSED, valve.state);
    TEST_ASSERT_FALSE(valve.motor_running);
}

// =============================================================================
// TEST CASES - Stop Command
// =============================================================================

void test_valve_stop_while_opening(void) {
    valve_start_open();
    valve_stop();
    
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_ERROR, valve.state);
    TEST_ASSERT_FALSE(valve.motor_running);
}

void test_valve_stop_while_idle(void) {
    valve_stop();
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_CLOSED, valve.state);  // No change
}

// =============================================================================
// TEST CASES - Limit Switches
// =============================================================================

void test_valve_limit_open_reached(void) {
    valve_start_open();
    valve_on_limit_open();
    
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_OPEN, valve.state);
    TEST_ASSERT_FALSE(valve.motor_running);
    TEST_ASSERT_TRUE(valve.status_flags & STATUS_FLAG_LIMIT_OPEN);
}

void test_valve_limit_close_reached(void) {
    valve.state = VALVE_STATE_OPEN;
    valve_start_close();
    valve_on_limit_close();
    
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_CLOSED, valve.state);
    TEST_ASSERT_FALSE(valve.motor_running);
    TEST_ASSERT_TRUE(valve.status_flags & STATUS_FLAG_LIMIT_CLOSE);
}

// =============================================================================
// TEST CASES - Timeout Protection
// =============================================================================

void test_valve_timeout_during_open(void) {
    valve_start_open();
    advance_time(30000);  // 30 seconds
    
    TEST_ASSERT_TRUE(valve_check_timeout(25000));  // 25 second timeout
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_ERROR, valve.state);
    TEST_ASSERT_TRUE(valve.status_flags & STATUS_FLAG_TIMEOUT);
    TEST_ASSERT_FALSE(valve.motor_running);
}

void test_valve_no_timeout_within_limit(void) {
    valve_start_open();
    advance_time(20000);  // 20 seconds
    
    TEST_ASSERT_FALSE(valve_check_timeout(25000));  // 25 second timeout
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_OPENING, valve.state);
    TEST_ASSERT_TRUE(valve.motor_running);
}

// =============================================================================
// TEST CASES - Overcurrent Protection
// =============================================================================

void test_valve_overcurrent_detected(void) {
    valve_start_open();
    valve.current_ma = 2500;  // 2.5A
    
    TEST_ASSERT_TRUE(valve_check_overcurrent(2000));  // 2A threshold
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_ERROR, valve.state);
    TEST_ASSERT_TRUE(valve.status_flags & STATUS_FLAG_OVERCURRENT);
    TEST_ASSERT_FALSE(valve.motor_running);
}

void test_valve_current_within_limit(void) {
    valve_start_open();
    valve.current_ma = 1500;  // 1.5A
    
    TEST_ASSERT_FALSE(valve_check_overcurrent(2000));  // 2A threshold
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_OPENING, valve.state);
    TEST_ASSERT_TRUE(valve.motor_running);
}

// =============================================================================
// TEST CASES - Full Cycle
// =============================================================================

void test_valve_full_open_close_cycle(void) {
    // Start closed
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_CLOSED, valve.state);
    
    // Open
    valve_start_open();
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_OPENING, valve.state);
    
    // Simulate reaching open limit
    advance_time(5000);
    valve_on_limit_open();
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_OPEN, valve.state);
    
    // Close
    valve_start_close();
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_CLOSING, valve.state);
    
    // Simulate reaching close limit
    advance_time(5000);
    valve_on_limit_close();
    TEST_ASSERT_EQUAL_UINT8(VALVE_STATE_CLOSED, valve.state);
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Initialization
    RUN_TEST(test_valve_init_state);
    
    // Open command
    RUN_TEST(test_valve_open_from_closed);
    RUN_TEST(test_valve_open_when_already_open);
    RUN_TEST(test_valve_open_while_opening);
    RUN_TEST(test_valve_open_while_closing);
    
    // Close command
    RUN_TEST(test_valve_close_from_open);
    RUN_TEST(test_valve_close_when_already_closed);
    
    // Stop command
    RUN_TEST(test_valve_stop_while_opening);
    RUN_TEST(test_valve_stop_while_idle);
    
    // Limit switches
    RUN_TEST(test_valve_limit_open_reached);
    RUN_TEST(test_valve_limit_close_reached);
    
    // Timeout protection
    RUN_TEST(test_valve_timeout_during_open);
    RUN_TEST(test_valve_no_timeout_within_limit);
    
    // Overcurrent protection
    RUN_TEST(test_valve_overcurrent_detected);
    RUN_TEST(test_valve_current_within_limit);
    
    // Full cycle
    RUN_TEST(test_valve_full_open_close_cycle);
    
    return UNITY_END();
}
