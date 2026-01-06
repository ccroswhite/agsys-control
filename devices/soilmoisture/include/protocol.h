/**
 * @file protocol.h
 * @brief Communication Protocol for Leader
 * 
 * Defines packet structures and message types for LoRa communication
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>
#include "config.h"

// Packet header - common to all messages
struct PacketHeader {
    uint8_t  magic[2];          // Protocol magic bytes
    uint8_t  version;           // Protocol version
    uint8_t  msgType;           // Message type
    uint8_t  deviceType;        // Device type identifier
    uint8_t  uuid[16];          // Device UUID
    uint16_t sequence;          // Sequence number for dedup
    uint8_t  payloadLen;        // Length of payload
} __attribute__((packed));

// Sensor report payload
struct SensorReport {
    uint32_t timestamp;         // Device uptime in seconds
    uint16_t moistureRaw;       // Raw moisture ADC value
    uint8_t  moisturePercent;   // Calculated moisture %
    uint16_t batteryMv;         // Battery voltage in mV
    int16_t  temperature;       // Temperature in 0.1°C
    int16_t  rssi;              // Last received RSSI
    uint8_t  pendingLogs;       // Number of unsent log entries
    uint8_t  flags;             // Status flags
} __attribute__((packed));

// Sensor report flags
#define REPORT_FLAG_LOW_BATTERY     (1 << 0)
#define REPORT_FLAG_FIRST_BOOT      (1 << 1)
#define REPORT_FLAG_CONFIG_REQUEST  (1 << 2)
#define REPORT_FLAG_HAS_PENDING     (1 << 3)

// ACK payload
struct AckPayload {
    uint16_t ackedSequence;     // Sequence number being acknowledged
    uint8_t  status;            // 0 = OK, non-zero = error code
    uint8_t  flags;             // Response flags
} __attribute__((packed));

// ACK flags
#define ACK_FLAG_SEND_LOGS          (1 << 0)    // Request pending logs
#define ACK_FLAG_CONFIG_AVAILABLE   (1 << 1)    // New config available
#define ACK_FLAG_TIME_SYNC          (1 << 2)    // Time sync follows

// Configuration payload
struct ConfigPayload {
    uint16_t sleepIntervalSec;  // Sleep interval in seconds
    uint8_t  txPowerDbm;        // Transmit power
    uint8_t  spreadingFactor;   // LoRa SF
    uint16_t moistureDryCal;    // Dry calibration value
    uint16_t moistureWetCal;    // Wet calibration value
    uint8_t  flags;             // Configuration flags
} __attribute__((packed));

// Log entry structure (for NVRAM storage)
struct LogEntry {
    uint32_t timestamp;         // Seconds since device boot
    uint16_t moistureRaw;       // Raw ADC moisture reading
    uint16_t batteryMv;         // Battery voltage in mV
    uint8_t  moisturePercent;   // Calculated moisture percentage
    uint8_t  flags;             // Status flags
    uint8_t  reserved[6];       // Reserved for future use
} __attribute__((packed));

// Log entry flags
#define LOG_FLAG_TX_SUCCESS     (1 << 0)
#define LOG_FLAG_TX_PENDING     (1 << 1)
#define LOG_FLAG_LOW_BATTERY    (1 << 2)

// Protocol class
class Protocol {
public:
    void init(const uint8_t* uuid);
    
    // Build a sensor report packet
    uint8_t buildSensorReport(
        uint8_t* buffer,
        uint8_t maxLen,
        uint16_t moistureRaw,
        uint8_t moisturePct,
        uint16_t batteryMv,
        int16_t temperature,
        uint8_t pendingLogs,
        uint8_t flags
    );
    
    // Parse received packet
    bool parse(const uint8_t* data, uint8_t len, PacketHeader* header, uint8_t* payload);
    
    // Get next sequence number
    uint16_t nextSequence();
    
    // Update uptime
    void updateUptime(uint32_t seconds);
    
private:
    uint8_t _uuid[16];
    uint16_t _sequence;
    uint32_t _uptime;
};

#endif // PROTOCOL_H
