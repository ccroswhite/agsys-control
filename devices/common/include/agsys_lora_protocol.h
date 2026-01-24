/**
 * @file agsys_lora_protocol.h
 * @brief AgSys LoRa Protocol - Single Source of Truth
 * 
 * THIS FILE IS THE CANONICAL DEFINITION for the LoRa protocol used between
 * IoT devices and the Property Controller.
 * 
 * Location: agsys-api/gen/c/lora/v1/agsys_lora_protocol.h
 * 
 * Supported Devices:
 * - Soil Moisture Sensor (0x01)
 * - Valve Controller (0x02)
 * - Water Meter (0x03)
 * - Valve Actuator (0x04) - CAN bus only, no direct LoRa
 * 
 * Wire Format:
 * [Nonce:4][Encrypted(Header+Payload)][Tag:4]
 * 
 * Encryption: AES-128-GCM with truncated nonce and tag
 * Key derivation: SHA-256(SECRET_SALT || DEVICE_UID)[0:16]
 * 
 * DO NOT MODIFY THIS FILE DIRECTLY IN DEVICE PROJECTS.
 * Changes should be made here and synced to all consumers.
 */

#ifndef AGSYS_LORA_PROTOCOL_H
#define AGSYS_LORA_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * PROTOCOL VERSION AND MAGIC
 * ========================================================================== */

#define AGSYS_PROTOCOL_VERSION      1
#define AGSYS_MAGIC_BYTE1           0x41    /* 'A' */
#define AGSYS_MAGIC_BYTE2           0x47    /* 'G' */

/* ==========================================================================
 * DEVICE TYPES (may be defined elsewhere, use guards)
 * ========================================================================== */

#ifndef AGSYS_DEVICE_TYPE_SOIL_MOISTURE
#define AGSYS_DEVICE_TYPE_SOIL_MOISTURE     0x01
#endif
#ifndef AGSYS_DEVICE_TYPE_VALVE_CONTROLLER
#define AGSYS_DEVICE_TYPE_VALVE_CONTROLLER  0x02
#endif
#ifndef AGSYS_DEVICE_TYPE_WATER_METER
#define AGSYS_DEVICE_TYPE_WATER_METER       0x03
#endif
#ifndef AGSYS_DEVICE_TYPE_VALVE_ACTUATOR
#define AGSYS_DEVICE_TYPE_VALVE_ACTUATOR    0x04  /* CAN bus only, no direct LoRa */
#endif

/* ==========================================================================
 * MESSAGE TYPES
 * 
 * Organized by device/function:
 * - 0x00-0x0F: Common messages (all devices)
 * - 0x10-0x1F: Common controller->device messages
 * - 0x20-0x2F: Soil moisture sensor
 * - 0x30-0x3F: Water meter
 * - 0x40-0x4F: Valve controller
 * - 0xE0-0xEF: OTA firmware updates
 * ========================================================================== */

/* Common messages - All devices (0x00 - 0x0F) */
#define AGSYS_MSG_HEARTBEAT             0x01
#define AGSYS_MSG_LOG_BATCH             0x02
#define AGSYS_MSG_CONFIG_REQUEST        0x03
#define AGSYS_MSG_ACK                   0x0E
#define AGSYS_MSG_NACK                  0x0F

/* Common controller -> device messages (0x10 - 0x1F) */
#define AGSYS_MSG_CONFIG_UPDATE         0x10
#define AGSYS_MSG_TIME_SYNC             0x11

/* Soil moisture sensor messages (0x20 - 0x2F) */
#define AGSYS_MSG_SOIL_REPORT           0x20
#define AGSYS_MSG_SOIL_CALIBRATE_REQ    0x21

/* Water meter messages (0x30 - 0x3F) */
#define AGSYS_MSG_METER_REPORT          0x30
#define AGSYS_MSG_METER_ALARM           0x31
#define AGSYS_MSG_METER_CALIBRATE_REQ   0x32
#define AGSYS_MSG_METER_RESET_TOTAL     0x33

/* Valve controller messages (0x40 - 0x4F) */
#define AGSYS_MSG_VALVE_STATUS          0x40
#define AGSYS_MSG_VALVE_ACK             0x41
#define AGSYS_MSG_VALVE_SCHEDULE_REQ    0x42
#define AGSYS_MSG_VALVE_COMMAND         0x43
#define AGSYS_MSG_VALVE_SCHEDULE        0x44

/* OTA firmware messages (0xE0 - 0xEF) */
#define AGSYS_MSG_OTA_ANNOUNCE          0xE0
#define AGSYS_MSG_OTA_CHUNK             0xE1
#define AGSYS_MSG_OTA_STATUS            0xE2
#define AGSYS_MSG_OTA_REQUEST           0xE3  /* Device requests OTA after seeing OTA_PENDING */
#define AGSYS_MSG_OTA_READY             0xE4  /* Device ready to receive chunks */
#define AGSYS_MSG_OTA_FINISH            0xE5  /* Controller signals OTA complete */

/* ==========================================================================
 * PACKET HEADER (15 bytes on wire)
 * ========================================================================== */

#define AGSYS_HEADER_SIZE               15
#define AGSYS_DEVICE_UID_SIZE           8

typedef struct __attribute__((packed)) {
    uint8_t     magic[2];       /* Protocol magic bytes (0x41, 0x47 = "AG") */
    uint8_t     version;        /* Protocol version (currently 1) */
    uint8_t     msg_type;       /* Message type (see AGSYS_MSG_* defines) */
    uint8_t     device_type;    /* Device type (see AGSYS_DEVICE_TYPE_* defines) */
    uint8_t     device_uid[8];  /* Device unique ID (from MCU FICR) */
    uint16_t    sequence;       /* Sequence number for dedup/ordering */
} agsys_header_t;

/* ==========================================================================
 * ENCRYPTION PARAMETERS
 * ========================================================================== */

#define AGSYS_CRYPTO_KEY_SIZE           16  /* AES-128 */
#define AGSYS_CRYPTO_NONCE_SIZE         4   /* Truncated nonce (counter) */
#define AGSYS_CRYPTO_TAG_SIZE           4   /* Truncated auth tag */
#define AGSYS_CRYPTO_OVERHEAD           (AGSYS_CRYPTO_NONCE_SIZE + AGSYS_CRYPTO_TAG_SIZE)

/* Maximum sizes */
#define AGSYS_MAX_PAYLOAD_SIZE          200
#define AGSYS_MAX_PACKET_SIZE           (AGSYS_MAX_PAYLOAD_SIZE + AGSYS_CRYPTO_OVERHEAD)

/* Secret salt for key derivation (16 bytes)
 * WARNING: Change this for production deployments! */
#define AGSYS_SECRET_SALT               { \
    0x41, 0x67, 0x53, 0x79, 0x73, 0x4C, 0x6F, 0x52, \
    0x61, 0x53, 0x61, 0x6C, 0x74, 0x32, 0x30, 0x32  \
}   /* "AgSysLoRaSalt202" */

/* ==========================================================================
 * SOIL MOISTURE SENSOR PAYLOADS (0x20)
 * ========================================================================== */

#define AGSYS_MAX_PROBES                4

/* Sensor report flags */
#define AGSYS_SENSOR_FLAG_LOW_BATTERY       (1 << 0)
#define AGSYS_SENSOR_FLAG_FIRST_BOOT        (1 << 1)
#define AGSYS_SENSOR_FLAG_CONFIG_REQUEST    (1 << 2)
#define AGSYS_SENSOR_FLAG_HAS_PENDING_LOGS  (1 << 3)

/* Single probe reading */
typedef struct __attribute__((packed)) {
    uint8_t     probe_index;        /* Probe index (0-3) */
    uint16_t    frequency_hz;       /* Raw oscillator frequency (for diagnostics) */
    uint8_t     moisture_percent;   /* Calculated moisture percentage (0-100) */
} agsys_probe_reading_t;

/* Full sensor report payload (AGSYS_MSG_SOIL_REPORT) */
typedef struct __attribute__((packed)) {
    uint32_t    timestamp;          /* Device uptime in seconds */
    uint8_t     probe_count;        /* Number of probes (1-4) */
    agsys_probe_reading_t probes[AGSYS_MAX_PROBES];  /* Probe readings */
    uint16_t    battery_mv;         /* Battery voltage in mV */
    int16_t     temperature;        /* Temperature in 0.1°C units */
    uint8_t     pending_logs;       /* Number of unsent log entries */
    uint8_t     flags;              /* Status flags */
    uint8_t     fw_version[3];      /* Firmware version (major, minor, patch) */
    uint8_t     boot_reason;        /* Boot reason (see AGSYS_BOOT_REASON_*) */
} agsys_soil_report_t;

/* ==========================================================================
 * WATER METER PAYLOADS (0x30)
 * ========================================================================== */

/* Water meter report flags */
#define AGSYS_METER_FLAG_LOW_BATTERY        (1 << 0)
#define AGSYS_METER_FLAG_REVERSE_FLOW       (1 << 1)
#define AGSYS_METER_FLAG_LEAK_DETECTED      (1 << 2)
#define AGSYS_METER_FLAG_TAMPER             (1 << 3)

/* Water meter report payload (AGSYS_MSG_METER_REPORT)
 * Note: Uses IEEE 754 single-precision floats for full resolution.
 * Floats are transmitted in little-endian byte order.
 */
typedef struct __attribute__((packed)) {
    uint32_t    timestamp;          /* Device uptime in seconds */
    float       total_volume_l;     /* Total volume in liters (IEEE 754 float) */
    float       flow_rate_lpm;      /* Current flow rate in liters/min (IEEE 754 float) */
    float       signal_uv;          /* Raw electrode signal in microvolts */
    float       temperature_c;      /* Device temperature in Celsius */
    uint16_t    battery_mv;         /* Battery voltage in mV (0 if mains powered) */
    uint8_t     signal_quality;     /* Signal quality 0-100% */
    uint8_t     flags;              /* Status flags */
    uint8_t     fw_version[3];      /* Firmware version (major, minor, patch) */
    uint8_t     boot_reason;        /* Boot reason (see AGSYS_BOOT_REASON_*) */
} agsys_meter_report_t;

/* Water meter alarm types */
#define AGSYS_METER_ALARM_CLEARED       0x00
#define AGSYS_METER_ALARM_LEAK          0x01
#define AGSYS_METER_ALARM_REVERSE       0x02
#define AGSYS_METER_ALARM_TAMPER        0x03
#define AGSYS_METER_ALARM_HIGH_FLOW     0x04

/* Water meter alarm payload (AGSYS_MSG_METER_ALARM) */
typedef struct __attribute__((packed)) {
    uint32_t    timestamp;          /* Device uptime in seconds */
    uint8_t     alarm_type;         /* Type of alarm */
    uint8_t     reserved;           /* Alignment padding */
    float       flow_rate_lpm;      /* Current flow rate in liters/min (IEEE 754 float) */
    uint32_t    duration_sec;       /* Duration of alarm condition in seconds */
    float       total_volume_l;     /* Total volume at alarm time (IEEE 754 float) */
    uint8_t     flags;              /* Additional flags */
} agsys_meter_alarm_t;

/* ==========================================================================
 * VALVE CONTROLLER PAYLOADS (0x40)
 * ========================================================================== */

/* Valve states */
#define AGSYS_VALVE_STATE_CLOSED        0
#define AGSYS_VALVE_STATE_OPEN          1
#define AGSYS_VALVE_STATE_OPENING       2
#define AGSYS_VALVE_STATE_CLOSING       3
#define AGSYS_VALVE_STATE_ERROR         4

/* Valve status report (AGSYS_MSG_VALVE_STATUS) */
typedef struct __attribute__((packed)) {
    uint8_t     valve_id;           /* Valve/actuator ID (0-63) */
    uint8_t     state;              /* Valve state (see AGSYS_VALVE_STATE_*) */
    uint16_t    current_ma;         /* Motor current in mA */
    uint32_t    last_change_time;   /* Unix timestamp of last state change */
    uint8_t     error_code;         /* Error code if state=ERROR */
    uint8_t     flags;              /* Status flags */
    uint8_t     fw_version[3];      /* Firmware version (major, minor, patch) */
    uint8_t     boot_reason;        /* Boot reason (see AGSYS_BOOT_REASON_*) */
} agsys_valve_status_t;

/* Valve command (AGSYS_MSG_VALVE_COMMAND) */
typedef struct __attribute__((packed)) {
    uint8_t     valve_id;           /* Valve/actuator ID */
    uint8_t     command;            /* 0=close, 1=open */
    uint16_t    duration_sec;       /* Auto-close after N seconds (0=manual) */
    uint32_t    command_id;         /* Unique command ID for ACK */
} agsys_valve_cmd_t;

/* Valve ACK (AGSYS_MSG_VALVE_ACK) */
typedef struct __attribute__((packed)) {
    uint32_t    command_id;         /* Command being acknowledged */
    uint8_t     valve_id;           /* Valve/actuator ID */
    uint8_t     result;             /* 0=success, non-zero=error code */
    uint8_t     new_state;          /* New valve state after command */
    uint8_t     reserved;
} agsys_valve_ack_t;

/* ==========================================================================
 * COMMON PAYLOADS
 * ========================================================================== */

/* Generic ACK/NACK (AGSYS_MSG_ACK, AGSYS_MSG_NACK) */
typedef struct __attribute__((packed)) {
    uint16_t    acked_sequence;     /* Sequence number being acknowledged */
    uint8_t     status;             /* 0 = OK, non-zero = error code */
    uint8_t     flags;              /* Response flags */
} agsys_ack_t;

/* ACK flags */
#define AGSYS_ACK_FLAG_SEND_LOGS        (1 << 0)
#define AGSYS_ACK_FLAG_CONFIG_AVAILABLE (1 << 1)
#define AGSYS_ACK_FLAG_TIME_SYNC        (1 << 2)
#define AGSYS_ACK_FLAG_OTA_PENDING      (1 << 3)  /* OTA update available, device should stay awake */

/* Time Sync (AGSYS_MSG_TIME_SYNC) */
typedef struct __attribute__((packed)) {
    uint32_t    unix_timestamp;     /* Current Unix timestamp */
    int16_t     utc_offset_min;     /* UTC offset in minutes */
    uint8_t     reserved[2];
} agsys_time_sync_t;

/* ==========================================================================
 * BOOT REASON (included in device reports, may be defined elsewhere)
 * ========================================================================== */

#ifndef AGSYS_BOOT_REASON_NORMAL
#define AGSYS_BOOT_REASON_NORMAL        0x00  /* Normal boot */
#endif
#ifndef AGSYS_BOOT_REASON_POWER_CYCLE
#define AGSYS_BOOT_REASON_POWER_CYCLE   0x01  /* Power cycle */
#endif
#ifndef AGSYS_BOOT_REASON_WATCHDOG
#define AGSYS_BOOT_REASON_WATCHDOG      0x02  /* Watchdog reset */
#endif
#ifndef AGSYS_BOOT_REASON_OTA_SUCCESS
#define AGSYS_BOOT_REASON_OTA_SUCCESS   0x03  /* First boot after successful OTA */
#endif
#ifndef AGSYS_BOOT_REASON_OTA_ROLLBACK
#define AGSYS_BOOT_REASON_OTA_ROLLBACK  0x04  /* Reverted to previous firmware */
#endif
#ifndef AGSYS_BOOT_REASON_HARD_FAULT
#define AGSYS_BOOT_REASON_HARD_FAULT    0x05  /* Hard fault */
#endif

/* ==========================================================================
 * OTA PAYLOADS (0xE0 - 0xE5)
 * ========================================================================== */

/* OTA Announce - Controller tells device about available update (AGSYS_MSG_OTA_ANNOUNCE) */
typedef struct __attribute__((packed)) {
    uint8_t     version_major;      /* Target firmware version */
    uint8_t     version_minor;
    uint8_t     version_patch;
    uint8_t     hw_revision_min;    /* Minimum compatible hardware revision */
    uint32_t    firmware_size;      /* Total firmware size in bytes */
    uint16_t    chunk_count;        /* Total number of chunks */
    uint16_t    chunk_size;         /* Size of each chunk (last may be smaller) */
    uint32_t    firmware_crc;       /* CRC32 of entire firmware */
} agsys_ota_announce_t;

/* OTA Request - Device requests OTA after seeing OTA_PENDING flag (AGSYS_MSG_OTA_REQUEST) */
typedef struct __attribute__((packed)) {
    uint8_t     current_major;      /* Current firmware version */
    uint8_t     current_minor;
    uint8_t     current_patch;
    uint8_t     hw_revision;        /* Device hardware revision */
    uint8_t     reserved[4];
} agsys_ota_request_t;

/* OTA Ready - Device confirms ready to receive chunks (AGSYS_MSG_OTA_READY) */
typedef struct __attribute__((packed)) {
    uint16_t    start_chunk;        /* Chunk index to start from (for resume) */
    uint8_t     reserved[2];
} agsys_ota_ready_t;

/* OTA Chunk - Controller sends firmware chunk (AGSYS_MSG_OTA_CHUNK) */
typedef struct __attribute__((packed)) {
    uint16_t    chunk_index;        /* Chunk index (0-based) */
    uint16_t    chunk_size;         /* Size of this chunk */
    uint8_t     data[];             /* Chunk data (variable length) */
} agsys_ota_chunk_t;

/* OTA Finish - Controller signals all chunks sent (AGSYS_MSG_OTA_FINISH) */
typedef struct __attribute__((packed)) {
    uint32_t    firmware_crc;       /* CRC32 for final verification */
    uint16_t    total_chunks;       /* Total chunks sent */
    uint8_t     reserved[2];
} agsys_ota_finish_t;

/* OTA Status - Device reports OTA progress/result (AGSYS_MSG_OTA_STATUS) */
#define AGSYS_OTA_STATUS_IN_PROGRESS    0x00
#define AGSYS_OTA_STATUS_SUCCESS        0x01
#define AGSYS_OTA_STATUS_FAILED         0x02
#define AGSYS_OTA_STATUS_ROLLED_BACK    0x03

#define AGSYS_OTA_ERROR_NONE            0x00
#define AGSYS_OTA_ERROR_CRC_MISMATCH    0x01
#define AGSYS_OTA_ERROR_SIZE_MISMATCH   0x02
#define AGSYS_OTA_ERROR_HW_INCOMPATIBLE 0x03
#define AGSYS_OTA_ERROR_FLASH_WRITE     0x04
#define AGSYS_OTA_ERROR_TIMEOUT         0x05
#define AGSYS_OTA_ERROR_VALIDATION      0x06

/* OTA status payload for LoRa transmission (AGSYS_MSG_OTA_STATUS) */
typedef struct __attribute__((packed)) {
    uint8_t     status;             /* OTA status (see AGSYS_OTA_STATUS_*) */
    uint8_t     error_code;         /* Error code if failed (see AGSYS_OTA_ERROR_*) */
    uint16_t    chunks_received;    /* Number of chunks received so far */
    uint8_t     version_major;      /* Current/new firmware version */
    uint8_t     version_minor;
    uint8_t     version_patch;
    uint8_t     boot_reason;        /* Boot reason (see AGSYS_BOOT_REASON_*) */
} agsys_ota_status_payload_t;

/* ==========================================================================
 * COMMON FLAGS (legacy aliases)
 * ========================================================================== */

#define AGSYS_FLAG_LOW_BATTERY      AGSYS_SENSOR_FLAG_LOW_BATTERY
#define AGSYS_FLAG_FIRST_BOOT       AGSYS_SENSOR_FLAG_FIRST_BOOT

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_LORA_PROTOCOL_H */
