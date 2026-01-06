/**
 * @file protocol.cpp
 * @brief Communication Protocol Implementation
 */

#include "protocol.h"

void Protocol::init(const uint8_t* uuid) {
    memcpy(_uuid, uuid, 16);
    _sequence = 0;
    _uptime = 0;
}

uint8_t Protocol::buildSensorReport(
    uint8_t* buffer,
    uint8_t maxLen,
    uint16_t moistureRaw,
    uint8_t moisturePct,
    uint16_t batteryMv,
    int16_t temperature,
    uint8_t pendingLogs,
    uint8_t flags)
{
    uint8_t totalLen = sizeof(PacketHeader) + sizeof(SensorReport);
    
    if (totalLen > maxLen) {
        return 0;
    }
    
    // Build header
    PacketHeader* header = (PacketHeader*)buffer;
    header->magic[0] = PROTOCOL_MAGIC_BYTE1;
    header->magic[1] = PROTOCOL_MAGIC_BYTE2;
    header->version = PROTOCOL_VERSION;
    header->msgType = MSG_TYPE_SENSOR_REPORT;
    header->deviceType = DEVICE_TYPE_SOIL_MOISTURE;
    memcpy(header->uuid, _uuid, 16);
    header->sequence = nextSequence();
    header->payloadLen = sizeof(SensorReport);
    
    // Build payload
    SensorReport* report = (SensorReport*)(buffer + sizeof(PacketHeader));
    report->timestamp = _uptime;
    report->moistureRaw = moistureRaw;
    report->moisturePercent = moisturePct;
    report->batteryMv = batteryMv;
    report->temperature = temperature;
    report->rssi = 0;  // Will be updated if needed
    report->pendingLogs = pendingLogs;
    report->flags = flags;
    
    return totalLen;
}

bool Protocol::parse(const uint8_t* data, uint8_t len, PacketHeader* header, uint8_t* payload) {
    // Minimum packet size check
    if (len < sizeof(PacketHeader)) {
        return false;
    }
    
    // Copy header
    memcpy(header, data, sizeof(PacketHeader));
    
    // Validate magic bytes
    if (header->magic[0] != PROTOCOL_MAGIC_BYTE1 ||
        header->magic[1] != PROTOCOL_MAGIC_BYTE2) {
        return false;
    }
    
    // Validate version
    if (header->version != PROTOCOL_VERSION) {
        return false;
    }
    
    // Validate payload length
    if (len < sizeof(PacketHeader) + header->payloadLen) {
        return false;
    }
    
    // Copy payload if provided
    if (payload != nullptr && header->payloadLen > 0) {
        memcpy(payload, data + sizeof(PacketHeader), header->payloadLen);
    }
    
    return true;
}

uint16_t Protocol::nextSequence() {
    return _sequence++;
}

void Protocol::updateUptime(uint32_t seconds) {
    _uptime = seconds;
}
