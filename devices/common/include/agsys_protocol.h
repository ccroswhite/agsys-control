/**
 * @file agsys_protocol.h
 * @brief AgSys LoRa Protocol Definitions
 * 
 * This file defines the common protocol structures and constants used by all
 * AgSys devices for LoRa communication with the property controller.
 * 
 * Protocol Overview:
 * - All packets are encrypted with AES-128-GCM
 * - Each device derives its own key from: SHA-256(SECRET_SALT || DEVICE_UID)[0:16]
 * - Packet format: [Nonce:4][Encrypted(Header+Payload)][Tag:4]
 * - Header includes magic bytes, version, and device identification
 */

#ifndef AGSYS_PROTOCOL_H
#define AGSYS_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * PROTOCOL VERSION AND MAGIC
 * ========================================================================== */

#define AGSYS_PROTOCOL_VERSION      1
#define AGSYS_MAGIC_BYTE1           0x41    // 'A'
#define AGSYS_MAGIC_BYTE2           0x47    // 'G'

/* ==========================================================================
 * DEVICE TYPES
 * ========================================================================== */

#define AGSYS_DEVICE_TYPE_SOIL_MOISTURE     0x01
#define AGSYS_DEVICE_TYPE_VALVE_CONTROLLER  0x02
#define AGSYS_DEVICE_TYPE_WATER_METER       0x03
#define AGSYS_DEVICE_TYPE_VALVE_ACTUATOR    0x04    // Not LoRa (CAN only)

/* ==========================================================================
 * MESSAGE TYPES
 * 
 * Device -> Controller: 0x01 - 0x0F
 * Controller -> Device: 0x10 - 0x1F
 * OTA Messages:         0x20 - 0x2F
 * Bidirectional:        0xF0 - 0xFF
 * ========================================================================== */

// Device -> Controller messages
#define AGSYS_MSG_SENSOR_REPORT         0x01    // Soil moisture sensor data
#define AGSYS_MSG_WATER_METER_REPORT    0x02    // Water meter reading
#define AGSYS_MSG_VALVE_STATUS          0x03    // Valve controller status
#define AGSYS_MSG_VALVE_ACK             0x04    // Valve command acknowledgment
#define AGSYS_MSG_SCHEDULE_REQUEST      0x05    // Request schedule from controller
#define AGSYS_MSG_HEARTBEAT             0x06    // Device heartbeat/keepalive
#define AGSYS_MSG_LOG_BATCH             0x07    // Batch of stored log entries

// Controller -> Device messages
#define AGSYS_MSG_VALVE_COMMAND         0x10    // Immediate valve open/close
#define AGSYS_MSG_SCHEDULE_UPDATE       0x11    // Schedule data for valve controller
#define AGSYS_MSG_CONFIG_UPDATE         0x12    // Configuration update
#define AGSYS_MSG_TIME_SYNC             0x13    // Time synchronization

// OTA messages
#define AGSYS_MSG_OTA_ANNOUNCE          0x20    // OTA firmware announcement
#define AGSYS_MSG_OTA_CHUNK             0x21    // OTA firmware chunk
#define AGSYS_MSG_OTA_STATUS            0x22    // OTA status response

// Bidirectional messages
#define AGSYS_MSG_ACK                   0xF0    // Generic acknowledgment
#define AGSYS_MSG_NACK                  0xF1    // Negative acknowledgment

/* ==========================================================================
 * PACKET HEADER
 * 
 * All LoRa packets start with this header (after decryption).
 * Total header size: 15 bytes
 * ========================================================================== */

#define AGSYS_HEADER_SIZE               15
#define AGSYS_DEVICE_UID_SIZE           8

typedef struct __attribute__((packed)) {
    uint8_t  magic[2];          // Protocol magic bytes (0x41, 0x47 = "AG")
    uint8_t  version;           // Protocol version (currently 1)
    uint8_t  msgType;           // Message type (see AGSYS_MSG_* defines)
    uint8_t  deviceType;        // Device type (see AGSYS_DEVICE_TYPE_* defines)
    uint8_t  deviceUid[8];      // Device unique ID (from MCU FICR)
    uint16_t sequence;          // Sequence number for dedup/ordering
} AgsysHeader;

/* ==========================================================================
 * ENCRYPTION PARAMETERS
 * 
 * Uses AES-128-GCM with per-device keys derived from shared salt.
 * Key derivation: SHA-256(SECRET_SALT || DEVICE_UID)[0:16]
 * ========================================================================== */

#define AGSYS_CRYPTO_KEY_SIZE           16      // AES-128
#define AGSYS_CRYPTO_NONCE_SIZE         4       // Truncated nonce (counter)
#define AGSYS_CRYPTO_TAG_SIZE           4       // Truncated auth tag
#define AGSYS_CRYPTO_OVERHEAD           (AGSYS_CRYPTO_NONCE_SIZE + AGSYS_CRYPTO_TAG_SIZE)  // 8 bytes

// Maximum payload sizes
#define AGSYS_MAX_PLAINTEXT             200     // Max plaintext (header + payload)
#define AGSYS_MAX_PACKET                (AGSYS_MAX_PLAINTEXT + AGSYS_CRYPTO_OVERHEAD)

// Secret salt for key derivation (16 bytes)
// WARNING: Change this for production deployments!
#define AGSYS_SECRET_SALT               { \
    0x41, 0x67, 0x53, 0x79, 0x73, 0x4C, 0x6F, 0x52, \
    0x61, 0x53, 0x61, 0x6C, 0x74, 0x32, 0x30, 0x32  \
}   // "AgSysLoRaSalt202"

/* ==========================================================================
 * PAYLOAD STRUCTURES - SENSOR REPORT (0x01)
 * 
 * Sent by soil moisture sensors every 2 hours.
 * Supports up to 4 probes at different depths.
 * ========================================================================== */

#define AGSYS_MAX_PROBES                4

// Flags for sensor report
#define AGSYS_SENSOR_FLAG_LOW_BATTERY       (1 << 0)
#define AGSYS_SENSOR_FLAG_FIRST_BOOT        (1 << 1)
#define AGSYS_SENSOR_FLAG_CONFIG_REQUEST    (1 << 2)
#define AGSYS_SENSOR_FLAG_HAS_PENDING_LOGS  (1 << 3)

// Single probe reading
typedef struct __attribute__((packed)) {
    uint8_t  probeIndex;        // Probe index (0-3)
    uint16_t frequencyHz;       // Raw oscillator frequency (for diagnostics)
    uint8_t  moisturePercent;   // Calculated moisture percentage (0-100)
} AgsysProbeReading;

// Full sensor report payload
typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Device uptime in seconds
    uint8_t  probeCount;        // Number of probes (1-4)
    AgsysProbeReading probes[AGSYS_MAX_PROBES];  // Probe readings
    uint16_t batteryMv;         // Battery voltage in mV
    int16_t  temperature;       // Temperature in 0.1°C units
    uint8_t  pendingLogs;       // Number of unsent log entries
    uint8_t  flags;             // Status flags
} AgsysSensorReport;

/* ==========================================================================
 * PAYLOAD STRUCTURES - WATER METER REPORT (0x02)
 * 
 * Sent by water meters every 5 minutes (or on significant flow change).
 * ========================================================================== */

// Flags for water meter report
#define AGSYS_METER_FLAG_LOW_BATTERY        (1 << 0)
#define AGSYS_METER_FLAG_REVERSE_FLOW       (1 << 1)
#define AGSYS_METER_FLAG_LEAK_DETECTED      (1 << 2)
#define AGSYS_METER_FLAG_TAMPER             (1 << 3)

typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // Device uptime in seconds
    uint32_t totalPulses;       // Total pulse count since installation
    uint32_t totalLiters;       // Total liters (calculated from pulses)
    uint16_t flowRateLPM;       // Current flow rate in liters/min * 10
    uint16_t batteryMv;         // Battery voltage in mV
    uint8_t  flags;             // Status flags
} AgsysWaterMeterReport;

/* ==========================================================================
 * PAYLOAD STRUCTURES - VALVE STATUS (0x03)
 * 
 * Sent by valve controller periodically and after state changes.
 * ========================================================================== */

// Valve states
#define AGSYS_VALVE_STATE_CLOSED        0x00
#define AGSYS_VALVE_STATE_OPEN          0x01
#define AGSYS_VALVE_STATE_OPENING       0x02
#define AGSYS_VALVE_STATE_CLOSING       0x03
#define AGSYS_VALVE_STATE_ERROR         0xFF

// Flags for valve status
#define AGSYS_VALVE_FLAG_POWER_FAIL     (1 << 0)
#define AGSYS_VALVE_FLAG_OVERCURRENT    (1 << 1)
#define AGSYS_VALVE_FLAG_TIMEOUT        (1 << 2)
#define AGSYS_VALVE_FLAG_ON_BATTERY     (1 << 3)

typedef struct __attribute__((packed)) {
    uint32_t timestamp;         // RTC Unix timestamp
    uint8_t  actuatorCount;     // Number of actuators reporting
    // Followed by actuatorCount instances of AgsysActuatorStatus
} AgsysValveStatusHeader;

typedef struct __attribute__((packed)) {
    uint8_t  address;           // Actuator address (1-64)
    uint8_t  state;             // Current valve state
    uint16_t currentMa;         // Motor current in mA (during operation)
    uint8_t  flags;             // Status flags
} AgsysActuatorStatus;

/* ==========================================================================
 * PAYLOAD STRUCTURES - VALVE ACK (0x04)
 * 
 * Sent by valve controller to acknowledge a command.
 * ========================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t  actuatorAddr;      // Actuator that executed the command
    uint16_t commandId;         // Command ID being acknowledged
    uint8_t  resultState;       // Resulting valve state
    uint8_t  success;           // 1 = success, 0 = failure
    uint8_t  errorCode;         // Error code if failed (0 = no error)
} AgsysValveAck;

// Error codes for valve operations
#define AGSYS_VALVE_ERR_NONE            0x00
#define AGSYS_VALVE_ERR_TIMEOUT         0x01
#define AGSYS_VALVE_ERR_OVERCURRENT     0x02
#define AGSYS_VALVE_ERR_ACTUATOR_OFFLINE 0x03
#define AGSYS_VALVE_ERR_POWER_FAIL      0x04

/* ==========================================================================
 * PAYLOAD STRUCTURES - VALVE COMMAND (0x10)
 * 
 * Sent by property controller to open/close valves.
 * ========================================================================== */

// Valve commands
#define AGSYS_VALVE_CMD_CLOSE           0x00
#define AGSYS_VALVE_CMD_OPEN            0x01
#define AGSYS_VALVE_CMD_STOP            0x02
#define AGSYS_VALVE_CMD_QUERY           0x03

typedef struct __attribute__((packed)) {
    uint8_t  actuatorAddr;      // Target actuator (1-64, 0xFF = all)
    uint8_t  command;           // Command (open/close/stop/query)
    uint16_t commandId;         // Unique command ID for tracking
    uint16_t durationSec;       // Duration in seconds (0 = indefinite)
} AgsysValveCommand;

/* ==========================================================================
 * PAYLOAD STRUCTURES - SCHEDULE UPDATE (0x11)
 * 
 * Sent by property controller to update valve schedules.
 * ========================================================================== */

typedef struct __attribute__((packed)) {
    uint16_t scheduleVersion;   // Schedule version number
    uint8_t  entryCount;        // Number of entries following
    // Followed by entryCount instances of AgsysScheduleEntry
} AgsysScheduleHeader;

typedef struct __attribute__((packed)) {
    uint8_t  dayMask;           // Bit mask for days (bit 0 = Sunday)
    uint8_t  startHour;         // Start hour (0-23)
    uint8_t  startMinute;       // Start minute (0-59)
    uint16_t durationMins;      // Duration in minutes
    uint8_t  actuatorMask[8];   // Bit mask for actuators (up to 64)
    uint8_t  flags;             // Schedule flags
} AgsysScheduleEntry;

// Schedule flags
#define AGSYS_SCHEDULE_FLAG_ENABLED     (1 << 0)
#define AGSYS_SCHEDULE_FLAG_SKIP_IF_WET (1 << 1)
#define AGSYS_SCHEDULE_FLAG_PROCEED_CHECK (1 << 2)

/* ==========================================================================
 * PAYLOAD STRUCTURES - CONFIG UPDATE (0x12)
 * 
 * Sent by property controller to update device configuration.
 * ========================================================================== */

typedef struct __attribute__((packed)) {
    uint16_t configVersion;     // Configuration version
    uint16_t sleepIntervalSec;  // Sleep interval in seconds
    uint8_t  txPowerDbm;        // Transmit power (dBm)
    uint8_t  spreadingFactor;   // LoRa spreading factor
    uint8_t  flags;             // Configuration flags
} AgsysConfigUpdate;

/* ==========================================================================
 * PAYLOAD STRUCTURES - TIME SYNC (0x13)
 * 
 * Sent by property controller to synchronize device time.
 * ========================================================================== */

typedef struct __attribute__((packed)) {
    uint32_t unixTimestamp;     // Current Unix timestamp (UTC)
    int8_t   utcOffset;         // UTC offset in hours
} AgsysTimeSync;

/* ==========================================================================
 * PAYLOAD STRUCTURES - ACK/NACK (0xF0, 0xF1)
 * 
 * Generic acknowledgment for any message.
 * ========================================================================== */

typedef struct __attribute__((packed)) {
    uint16_t ackedSequence;     // Sequence number being acknowledged
    uint8_t  status;            // 0 = OK, non-zero = error code
    uint8_t  flags;             // Response flags
} AgsysAck;

// ACK flags
#define AGSYS_ACK_FLAG_SEND_LOGS        (1 << 0)    // Request pending logs
#define AGSYS_ACK_FLAG_CONFIG_AVAILABLE (1 << 1)    // New config available
#define AGSYS_ACK_FLAG_TIME_SYNC        (1 << 2)    // Time sync follows
#define AGSYS_ACK_FLAG_SCHEDULE_UPDATE  (1 << 3)    // Schedule update follows

/* ==========================================================================
 * HELPER MACROS
 * ========================================================================== */

// Calculate total packet size (encrypted)
#define AGSYS_PACKET_SIZE(payload_size) \
    (AGSYS_CRYPTO_OVERHEAD + AGSYS_HEADER_SIZE + (payload_size))

// Validate header magic bytes
#define AGSYS_HEADER_VALID(hdr) \
    ((hdr)->magic[0] == AGSYS_MAGIC_BYTE1 && (hdr)->magic[1] == AGSYS_MAGIC_BYTE2)

#ifdef __cplusplus
}
#endif

#endif // AGSYS_PROTOCOL_H
