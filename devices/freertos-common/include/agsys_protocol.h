/**
 * @file agsys_protocol.h
 * @brief AgSys LoRa protocol message structures
 * 
 * Defines message formats for communication between devices and
 * the property controller via LoRa.
 * 
 * Wire format: [Header (4 bytes)] [Payload (variable)] [IV (12 bytes)] [Tag (16 bytes)]
 */

#ifndef AGSYS_PROTOCOL_H
#define AGSYS_PROTOCOL_H

#include "agsys_common.h"

/* ==========================================================================
 * PROTOCOL VERSION
 * ========================================================================== */

#define AGSYS_PROTOCOL_VERSION      1

/* ==========================================================================
 * MESSAGE TYPES
 * ========================================================================== */

typedef enum {
    /* Device → Controller */
    AGSYS_MSG_SENSOR_DATA       = 0x01,  /* Soil moisture reading */
    AGSYS_MSG_METER_DATA        = 0x02,  /* Water meter reading */
    AGSYS_MSG_VALVE_STATUS      = 0x03,  /* Valve state report */
    AGSYS_MSG_HEARTBEAT         = 0x04,  /* Device alive */
    AGSYS_MSG_DISCOVERY         = 0x05,  /* New device announcement */
    AGSYS_MSG_ACK               = 0x06,  /* Command acknowledgment */
    AGSYS_MSG_NACK              = 0x07,  /* Command rejection */
    
    /* Controller → Device */
    AGSYS_MSG_VALVE_CMD         = 0x10,  /* Open/close valve */
    AGSYS_MSG_CONFIG_UPDATE     = 0x11,  /* Configuration change */
    AGSYS_MSG_TIME_SYNC         = 0x12,  /* RTC synchronization */
    AGSYS_MSG_PING              = 0x13,  /* Connectivity check */
    AGSYS_MSG_REBOOT            = 0x14,  /* Request device reboot */
} agsys_msg_type_t;

/* ==========================================================================
 * MESSAGE HEADER
 * ========================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t     version;        /* Protocol version */
    uint8_t     msg_type;       /* Message type (agsys_msg_type_t) */
    uint16_t    seq_num;        /* Sequence number (for dedup/ordering) */
} agsys_msg_header_t;

#define AGSYS_MSG_HEADER_SIZE   sizeof(agsys_msg_header_t)

/* ==========================================================================
 * PAYLOAD STRUCTURES
 * ========================================================================== */

/* Soil Moisture Sensor Data (AGSYS_MSG_SENSOR_DATA) */
typedef struct __attribute__((packed)) {
    uint32_t    device_id;          /* Short device ID */
    uint16_t    moisture_pct_x10;   /* Moisture % * 10 (0-1000 = 0-100.0%) */
    int16_t     temperature_x10;    /* Temperature °C * 10 */
    uint16_t    battery_mv;         /* Battery voltage in mV */
    uint8_t     zone_id;            /* Assigned zone (0 = unassigned) */
    uint8_t     flags;              /* Status flags */
} agsys_sensor_data_t;

/* Water Meter Data (AGSYS_MSG_METER_DATA) */
typedef struct __attribute__((packed)) {
    uint32_t    device_id;          /* Short device ID */
    int32_t     flow_rate_x100;     /* Flow rate * 100 (signed for direction) */
    uint32_t    total_volume;       /* Total volume (units per config) */
    uint16_t    coil_current_ma;    /* Coil current in mA */
    int16_t     temperature_x10;    /* Temperature °C * 10 */
    uint8_t     unit_type;          /* 0=L/min, 1=GPM, 2=acre-ft/hr */
    uint8_t     flags;              /* Status flags */
} agsys_meter_data_t;

/* Valve Status Report (AGSYS_MSG_VALVE_STATUS) */
typedef struct __attribute__((packed)) {
    uint32_t    device_id;          /* Short device ID */
    uint8_t     valve_id;           /* Valve/actuator ID (0-63) */
    uint8_t     state;              /* 0=closed, 1=open, 2=opening, 3=closing, 4=error */
    uint16_t    current_ma;         /* Motor current in mA */
    uint32_t    last_change_time;   /* Unix timestamp of last state change */
    uint8_t     error_code;         /* Error code if state=4 */
    uint8_t     flags;              /* Status flags */
} agsys_valve_status_t;

/* Valve Command (AGSYS_MSG_VALVE_CMD) */
typedef struct __attribute__((packed)) {
    uint32_t    target_device_id;   /* Target device ID */
    uint8_t     valve_id;           /* Valve/actuator ID */
    uint8_t     command;            /* 0=close, 1=open */
    uint16_t    duration_sec;       /* Auto-close after N seconds (0=manual) */
    uint32_t    command_id;         /* Unique command ID for ACK */
} agsys_valve_cmd_t;

/* Heartbeat (AGSYS_MSG_HEARTBEAT) */
typedef struct __attribute__((packed)) {
    uint32_t    device_id;          /* Short device ID */
    uint8_t     device_type;        /* Device type */
    uint8_t     fw_version_major;   /* Firmware version */
    uint8_t     fw_version_minor;
    uint8_t     fw_version_patch;
    uint16_t    battery_mv;         /* Battery voltage (0 if mains powered) */
    int8_t      rssi;               /* Last received RSSI */
    uint8_t     flags;              /* Status flags */
} agsys_heartbeat_t;

/* Discovery (AGSYS_MSG_DISCOVERY) */
typedef struct __attribute__((packed)) {
    uint8_t     device_uid[8];      /* Full 8-byte device UID */
    uint8_t     device_type;        /* Device type */
    uint8_t     fw_version_major;
    uint8_t     fw_version_minor;
    uint8_t     fw_version_patch;
    uint8_t     capabilities;       /* Capability flags */
    uint8_t     reserved[3];
} agsys_discovery_t;

/* Command ACK/NACK (AGSYS_MSG_ACK, AGSYS_MSG_NACK) */
typedef struct __attribute__((packed)) {
    uint32_t    device_id;          /* Responding device ID */
    uint32_t    command_id;         /* Command being acknowledged */
    uint8_t     error_code;         /* Error code (for NACK) */
    uint8_t     reserved[3];
} agsys_cmd_response_t;

/* Time Sync (AGSYS_MSG_TIME_SYNC) */
typedef struct __attribute__((packed)) {
    uint32_t    unix_timestamp;     /* Current Unix timestamp */
    int16_t     utc_offset_min;     /* UTC offset in minutes */
    uint8_t     reserved[2];
} agsys_time_sync_t;

/* ==========================================================================
 * FLAGS
 * ========================================================================== */

/* Common flags (used in multiple message types) */
#define AGSYS_FLAG_LOW_BATTERY      (1 << 0)
#define AGSYS_FLAG_ERROR            (1 << 1)
#define AGSYS_FLAG_CALIBRATION_DUE  (1 << 2)
#define AGSYS_FLAG_FIRST_BOOT       (1 << 3)

/* Valve-specific flags */
#define AGSYS_FLAG_VALVE_MANUAL     (1 << 4)  /* Manually controlled */
#define AGSYS_FLAG_VALVE_SCHEDULED  (1 << 5)  /* Schedule-controlled */

/* ==========================================================================
 * MAXIMUM SIZES
 * ========================================================================== */

#define AGSYS_MAX_PAYLOAD_SIZE      64
#define AGSYS_MAX_MESSAGE_SIZE      (AGSYS_MSG_HEADER_SIZE + AGSYS_MAX_PAYLOAD_SIZE + \
                                     AGSYS_CRYPTO_IV_SIZE + AGSYS_CRYPTO_TAG_SIZE)

/* ==========================================================================
 * ENCODING / DECODING
 * ========================================================================== */

/**
 * @brief Encode a message for transmission
 * 
 * @param header        Message header
 * @param payload       Payload data
 * @param payload_len   Payload length
 * @param crypto_ctx    Crypto context for encryption
 * @param out_buf       Output buffer
 * @param out_len       Output: actual encoded length
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_protocol_encode(const agsys_msg_header_t *header,
                                   const void *payload,
                                   size_t payload_len,
                                   const void *crypto_ctx,
                                   uint8_t *out_buf,
                                   size_t *out_len);

/**
 * @brief Decode a received message
 * 
 * @param in_buf        Input buffer
 * @param in_len        Input length
 * @param crypto_ctx    Crypto context for decryption
 * @param header        Output: decoded header
 * @param payload       Output: decrypted payload
 * @param payload_len   Output: payload length
 * @return AGSYS_OK on success, AGSYS_ERR_CRYPTO if auth fails
 */
agsys_err_t agsys_protocol_decode(const uint8_t *in_buf,
                                   size_t in_len,
                                   const void *crypto_ctx,
                                   agsys_msg_header_t *header,
                                   void *payload,
                                   size_t *payload_len);

#endif /* AGSYS_PROTOCOL_H */
