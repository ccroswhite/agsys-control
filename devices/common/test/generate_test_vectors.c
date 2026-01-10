/**
 * @file generate_test_vectors.c
 * @brief Generate test vectors for cross-validation with Go
 * 
 * This program outputs JSON test vectors that can be consumed by Go tests
 * to validate that C and Go encoding/decoding are compatible.
 * 
 * Usage: ./generate_test_vectors > test_vectors.json
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/agsys_protocol.h"

// Print bytes as hex string
void print_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
}

int main(void) {
    printf("{\n");
    
    // MeterAlarm test vectors
    printf("  \"meter_alarms\": [\n");
    
    AgsysMeterAlarm alarms[] = {
        {.timestamp = 12345, .alarmType = AGSYS_METER_ALARM_LEAK, .flowRateLPM = 150, .durationSec = 3600, .totalLiters = 50000, .flags = 0x01},
        {.timestamp = 99999, .alarmType = AGSYS_METER_ALARM_HIGH_FLOW, .flowRateLPM = 1200, .durationSec = 60, .totalLiters = 100000, .flags = 0x00},
        {.timestamp = 54321, .alarmType = AGSYS_METER_ALARM_CLEARED, .flowRateLPM = 0, .durationSec = 0, .totalLiters = 75000, .flags = 0x00}
    };
    
    for (int i = 0; i < 3; i++) {
        printf("    {\"timestamp\": %u, \"alarm_type\": %u, \"flow_rate_lpm\": %u, \"duration_sec\": %u, \"total_liters\": %u, \"flags\": %u, \"encoded\": \"",
               alarms[i].timestamp, alarms[i].alarmType, alarms[i].flowRateLPM, 
               alarms[i].durationSec, alarms[i].totalLiters, alarms[i].flags);
        print_hex((uint8_t*)&alarms[i], sizeof(AgsysMeterAlarm));
        printf("\"}%s\n", i < 2 ? "," : "");
    }
    printf("  ],\n");
    
    // MeterConfig test vectors
    printf("  \"meter_configs\": [\n");
    
    AgsysMeterConfig configs[] = {
        {.configVersion = 5, .reportIntervalSec = 60, .pulsesPerLiter = 45000, .leakThresholdMin = 60, .maxFlowRateLPM = 1000, .flags = 0x05},
        {.configVersion = 1, .reportIntervalSec = 120, .pulsesPerLiter = 58800, .leakThresholdMin = 30, .maxFlowRateLPM = 500, .flags = 0x01}
    };
    
    for (int i = 0; i < 2; i++) {
        printf("    {\"config_version\": %u, \"report_interval_sec\": %u, \"pulses_per_liter\": %u, \"leak_threshold_min\": %u, \"max_flow_rate_lpm\": %u, \"flags\": %u, \"encoded\": \"",
               configs[i].configVersion, configs[i].reportIntervalSec, configs[i].pulsesPerLiter,
               configs[i].leakThresholdMin, configs[i].maxFlowRateLPM, configs[i].flags);
        print_hex((uint8_t*)&configs[i], sizeof(AgsysMeterConfig));
        printf("\"}%s\n", i < 1 ? "," : "");
    }
    printf("  ],\n");
    
    // MeterResetTotal test vectors
    printf("  \"meter_resets\": [\n");
    
    AgsysMeterResetTotal resets[] = {
        {.commandId = 1234, .resetType = 0, .newTotalLiters = 0},
        {.commandId = 5678, .resetType = 1, .newTotalLiters = 100000}
    };
    
    for (int i = 0; i < 2; i++) {
        printf("    {\"command_id\": %u, \"reset_type\": %u, \"new_total_liters\": %u, \"encoded\": \"",
               resets[i].commandId, resets[i].resetType, resets[i].newTotalLiters);
        print_hex((uint8_t*)&resets[i], sizeof(AgsysMeterResetTotal));
        printf("\"}%s\n", i < 1 ? "," : "");
    }
    printf("  ],\n");
    
    // ACK test vectors
    printf("  \"acks\": [\n");
    
    AgsysAck acks[] = {
        {.ackedSequence = 0x1234, .status = 0, .flags = 0x06},
        {.ackedSequence = 0xABCD, .status = 1, .flags = 0x00}
    };
    
    for (int i = 0; i < 2; i++) {
        printf("    {\"acked_sequence\": %u, \"status\": %u, \"flags\": %u, \"encoded\": \"",
               acks[i].ackedSequence, acks[i].status, acks[i].flags);
        print_hex((uint8_t*)&acks[i], sizeof(AgsysAck));
        printf("\"}%s\n", i < 1 ? "," : "");
    }
    printf("  ],\n");
    
    // Header test vector
    printf("  \"headers\": [\n");
    AgsysHeader header = {
        .magic = {AGSYS_MAGIC_BYTE1, AGSYS_MAGIC_BYTE2},
        .version = AGSYS_PROTOCOL_VERSION,
        .msgType = AGSYS_MSG_METER_ALARM,
        .deviceType = AGSYS_DEVICE_TYPE_WATER_METER,
        .deviceUid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
        .sequence = 0x1234
    };
    printf("    {\"version\": %u, \"msg_type\": %u, \"device_type\": %u, \"sequence\": %u, \"device_uid\": \"0102030405060708\", \"encoded\": \"",
           header.version, header.msgType, header.deviceType, header.sequence);
    print_hex((uint8_t*)&header, sizeof(AgsysHeader));
    printf("\"}\n");
    printf("  ]\n");
    
    printf("}\n");
    
    return 0;
}
