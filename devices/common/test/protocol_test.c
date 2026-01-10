/**
 * @file protocol_test.c
 * @brief Protocol encoding/decoding tests for AgSys devices
 * 
 * This test file validates that C struct encoding matches what the
 * Go property controller expects. Run with: make test
 * 
 * The test outputs hex dumps that can be compared against Go test output
 * to ensure cross-platform compatibility.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "../include/agsys_protocol.h"

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    printf("Testing %s... ", name); \
    tests_run++;

#define PASS() \
    printf("PASS\n"); \
    tests_passed++;

#define FAIL(msg) \
    printf("FAIL: %s\n", msg); \
    return;

// Helper to print hex dump
void hexdump(const char* label, const uint8_t* data, size_t len) {
    printf("%s (%zu bytes): ", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// Test header encoding
void test_header_encoding(void) {
    TEST("Header encoding");
    
    AgsysHeader header = {
        .magic = {AGSYS_MAGIC_BYTE1, AGSYS_MAGIC_BYTE2},
        .version = AGSYS_PROTOCOL_VERSION,
        .msgType = AGSYS_MSG_METER_ALARM,
        .deviceType = AGSYS_DEVICE_TYPE_WATER_METER,
        .deviceUid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
        .sequence = 0x1234
    };
    
    // Verify size
    if (sizeof(AgsysHeader) != AGSYS_HEADER_SIZE) {
        FAIL("Header size mismatch");
    }
    
    // Verify magic bytes
    if (header.magic[0] != 0x41 || header.magic[1] != 0x47) {
        FAIL("Magic bytes wrong");
    }
    
    // Verify packed struct layout
    uint8_t* raw = (uint8_t*)&header;
    if (raw[0] != 0x41 || raw[1] != 0x47) {
        FAIL("Packed layout wrong for magic");
    }
    if (raw[2] != AGSYS_PROTOCOL_VERSION) {
        FAIL("Packed layout wrong for version");
    }
    if (raw[3] != AGSYS_MSG_METER_ALARM) {
        FAIL("Packed layout wrong for msgType");
    }
    
    hexdump("Header", raw, sizeof(header));
    PASS();
}

// Test MeterAlarm encoding
void test_meter_alarm_encoding(void) {
    TEST("MeterAlarm encoding");
    
    AgsysMeterAlarm alarm = {
        .timestamp = 12345,
        .alarmType = AGSYS_METER_ALARM_LEAK,
        .flowRateLPM = 150,  // 15.0 L/min
        .durationSec = 3600,
        .totalLiters = 50000,
        .flags = 0x01
    };
    
    // Verify size (should be 16 bytes)
    if (sizeof(AgsysMeterAlarm) != 16) {
        printf("Size is %zu, expected 16\n", sizeof(AgsysMeterAlarm));
        FAIL("MeterAlarm size wrong");
    }
    
    uint8_t* raw = (uint8_t*)&alarm;
    
    // Verify timestamp at offset 0 (little-endian)
    uint32_t ts = raw[0] | (raw[1] << 8) | (raw[2] << 16) | (raw[3] << 24);
    if (ts != 12345) {
        FAIL("Timestamp encoding wrong");
    }
    
    // Verify alarmType at offset 4
    if (raw[4] != AGSYS_METER_ALARM_LEAK) {
        FAIL("AlarmType encoding wrong");
    }
    
    // Verify flowRateLPM at offset 5 (little-endian)
    uint16_t flow = raw[5] | (raw[6] << 8);
    if (flow != 150) {
        FAIL("FlowRateLPM encoding wrong");
    }
    
    // Verify durationSec at offset 7 (little-endian)
    uint32_t dur = raw[7] | (raw[8] << 8) | (raw[9] << 16) | (raw[10] << 24);
    if (dur != 3600) {
        FAIL("DurationSec encoding wrong");
    }
    
    // Verify totalLiters at offset 11 (little-endian)
    uint32_t total = raw[11] | (raw[12] << 8) | (raw[13] << 16) | (raw[14] << 24);
    if (total != 50000) {
        FAIL("TotalLiters encoding wrong");
    }
    
    // Verify flags at offset 15
    if (raw[15] != 0x01) {
        FAIL("Flags encoding wrong");
    }
    
    hexdump("MeterAlarm", raw, sizeof(alarm));
    PASS();
}

// Test MeterConfig encoding
void test_meter_config_encoding(void) {
    TEST("MeterConfig encoding");
    
    AgsysMeterConfig config = {
        .configVersion = 5,
        .reportIntervalSec = 60,
        .pulsesPerLiter = 45000,
        .leakThresholdMin = 60,
        .maxFlowRateLPM = 1000,
        .flags = AGSYS_METER_CFG_LEAK_DETECT_EN | AGSYS_METER_CFG_TAMPER_DETECT
    };
    
    // Verify size (should be 11 bytes)
    if (sizeof(AgsysMeterConfig) != 11) {
        printf("Size is %zu, expected 11\n", sizeof(AgsysMeterConfig));
        FAIL("MeterConfig size wrong");
    }
    
    uint8_t* raw = (uint8_t*)&config;
    
    // Verify configVersion at offset 0
    uint16_t ver = raw[0] | (raw[1] << 8);
    if (ver != 5) {
        FAIL("ConfigVersion encoding wrong");
    }
    
    // Verify reportIntervalSec at offset 2
    uint16_t interval = raw[2] | (raw[3] << 8);
    if (interval != 60) {
        FAIL("ReportIntervalSec encoding wrong");
    }
    
    // Verify flags at offset 10
    if (raw[10] != (AGSYS_METER_CFG_LEAK_DETECT_EN | AGSYS_METER_CFG_TAMPER_DETECT)) {
        FAIL("Flags encoding wrong");
    }
    
    hexdump("MeterConfig", raw, sizeof(config));
    PASS();
}

// Test MeterResetTotal encoding
void test_meter_reset_encoding(void) {
    TEST("MeterResetTotal encoding");
    
    AgsysMeterResetTotal reset = {
        .commandId = 1234,
        .resetType = 1,
        .newTotalLiters = 100000
    };
    
    // Verify size (should be 7 bytes)
    if (sizeof(AgsysMeterResetTotal) != 7) {
        printf("Size is %zu, expected 7\n", sizeof(AgsysMeterResetTotal));
        FAIL("MeterResetTotal size wrong");
    }
    
    uint8_t* raw = (uint8_t*)&reset;
    
    // Verify commandId at offset 0
    uint16_t cmdId = raw[0] | (raw[1] << 8);
    if (cmdId != 1234) {
        FAIL("CommandId encoding wrong");
    }
    
    // Verify resetType at offset 2
    if (raw[2] != 1) {
        FAIL("ResetType encoding wrong");
    }
    
    // Verify newTotalLiters at offset 3
    uint32_t total = raw[3] | (raw[4] << 8) | (raw[5] << 16) | (raw[6] << 24);
    if (total != 100000) {
        FAIL("NewTotalLiters encoding wrong");
    }
    
    hexdump("MeterResetTotal", raw, sizeof(reset));
    PASS();
}

// Test ACK encoding
void test_ack_encoding(void) {
    TEST("ACK encoding");
    
    AgsysAck ack = {
        .ackedSequence = 0x1234,
        .status = 0,
        .flags = AGSYS_ACK_FLAG_CONFIG_AVAILABLE | AGSYS_ACK_FLAG_TIME_SYNC
    };
    
    // Verify size (should be 4 bytes)
    if (sizeof(AgsysAck) != 4) {
        printf("Size is %zu, expected 4\n", sizeof(AgsysAck));
        FAIL("ACK size wrong");
    }
    
    uint8_t* raw = (uint8_t*)&ack;
    
    // Verify ackedSequence at offset 0
    uint16_t seq = raw[0] | (raw[1] << 8);
    if (seq != 0x1234) {
        FAIL("AckedSequence encoding wrong");
    }
    
    // Verify status at offset 2
    if (raw[2] != 0) {
        FAIL("Status encoding wrong");
    }
    
    // Verify flags at offset 3
    if (raw[3] != (AGSYS_ACK_FLAG_CONFIG_AVAILABLE | AGSYS_ACK_FLAG_TIME_SYNC)) {
        FAIL("Flags encoding wrong");
    }
    
    hexdump("ACK", raw, sizeof(ack));
    PASS();
}

// Test message type constants
void test_message_types(void) {
    TEST("Message type constants");
    
    // Common messages (0x00 - 0x0F)
    assert(AGSYS_MSG_HEARTBEAT == 0x01);
    assert(AGSYS_MSG_ACK == 0x0E);
    assert(AGSYS_MSG_NACK == 0x0F);
    
    // Controller -> device (0x10 - 0x1F)
    assert(AGSYS_MSG_CONFIG_UPDATE == 0x10);
    assert(AGSYS_MSG_TIME_SYNC == 0x11);
    
    // Soil moisture (0x20 - 0x2F)
    assert(AGSYS_MSG_SOIL_REPORT == 0x20);
    
    // Water meter (0x30 - 0x3F)
    assert(AGSYS_MSG_METER_REPORT == 0x30);
    assert(AGSYS_MSG_METER_ALARM == 0x31);
    assert(AGSYS_MSG_METER_RESET_TOTAL == 0x33);
    
    // Valve controller (0x40 - 0x4F)
    assert(AGSYS_MSG_VALVE_STATUS == 0x40);
    assert(AGSYS_MSG_VALVE_COMMAND == 0x43);
    
    PASS();
}

// Test WaterMeterReport encoding
void test_water_meter_report_encoding(void) {
    TEST("WaterMeterReport encoding");
    
    AgsysWaterMeterReport report = {
        .timestamp = 54321,
        .totalPulses = 1000000,
        .totalLiters = 2222,
        .flowRateLPM = 155,  // 15.5 L/min
        .batteryMv = 3700,
        .flags = AGSYS_METER_FLAG_LOW_BATTERY
    };
    
    // Verify size (should be 17 bytes)
    if (sizeof(AgsysWaterMeterReport) != 17) {
        printf("Size is %zu, expected 17\n", sizeof(AgsysWaterMeterReport));
        FAIL("WaterMeterReport size wrong");
    }
    
    uint8_t* raw = (uint8_t*)&report;
    hexdump("WaterMeterReport", raw, sizeof(report));
    PASS();
}

int main(void) {
    printf("=== AgSys Protocol Tests ===\n\n");
    
    test_header_encoding();
    test_meter_alarm_encoding();
    test_meter_config_encoding();
    test_meter_reset_encoding();
    test_ack_encoding();
    test_message_types();
    test_water_meter_report_encoding();
    
    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
