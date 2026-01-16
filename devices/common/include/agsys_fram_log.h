/**
 * @file agsys_fram_log.h
 * @brief FRAM-based Log Storage for MB85RS1MT
 * 
 * Provides ring-buffer log storage in FRAM for unlimited write endurance.
 * FRAM has ~10^14 write cycles vs flash's ~10^4, making it ideal for
 * frequent logging operations over the 10-year product lifetime.
 * 
 * FRAM Layout (in log region @ 0x05300):
 *   - Header (64 bytes): Log metadata, head/tail pointers
 *   - Data: Log entries (128 bytes each)
 * 
 * Capacity: 16KB = ~126 entries @ 128 bytes each
 * 
 * NOTE: This replaces agsys_flash_log.h for all logging operations.
 * External flash should only be used for OTA firmware storage.
 */

#ifndef AGSYS_FRAM_LOG_H
#define AGSYS_FRAM_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "agsys_fram.h"
#include "agsys_memory_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * LOG CONFIGURATION
 * ========================================================================== */

/* FRAM region for logs (defined in agsys_memory_layout.h) */
#define AGSYS_LOG_FRAM_START        AGSYS_FRAM_LOG_ADDR
#define AGSYS_LOG_FRAM_SIZE         AGSYS_FRAM_LOG_SIZE

/* Log entry sizes */
#define AGSYS_LOG_HEADER_SIZE       64          /* Header at start of region */
#define AGSYS_LOG_ENTRY_SIZE        128         /* Fixed entry size */
#define AGSYS_LOG_DATA_START        (AGSYS_LOG_FRAM_START + AGSYS_LOG_HEADER_SIZE)
#define AGSYS_LOG_DATA_SIZE         (AGSYS_LOG_FRAM_SIZE - AGSYS_LOG_HEADER_SIZE)
#define AGSYS_LOG_MAX_ENTRIES       (AGSYS_LOG_DATA_SIZE / AGSYS_LOG_ENTRY_SIZE)

/* Header magic */
#define AGSYS_LOG_MAGIC             0x464C4F47  /* "FLOG" - FRAM LOG */
#define AGSYS_LOG_VERSION           1

/* ==========================================================================
 * LOG ENTRY TYPES
 * ========================================================================== */

typedef enum {
    AGSYS_LOG_TYPE_SENSOR_READING   = 0x01,
    AGSYS_LOG_TYPE_METER_READING    = 0x02,
    AGSYS_LOG_TYPE_VALVE_EVENT      = 0x03,
    AGSYS_LOG_TYPE_ALARM            = 0x04,
    AGSYS_LOG_TYPE_CONFIG_CHANGE    = 0x05,
    AGSYS_LOG_TYPE_BOOT             = 0x06,
    AGSYS_LOG_TYPE_ERROR            = 0x07,
    AGSYS_LOG_TYPE_DEBUG            = 0x08,
    AGSYS_LOG_TYPE_OTA              = 0x09
} agsys_log_type_t;

/* ==========================================================================
 * LOG ENTRY STRUCTURE
 * ========================================================================== */

/**
 * @brief Log entry header (common to all entry types)
 * 
 * Total entry size: 128 bytes
 * - Header: 16 bytes
 * - Payload: 96 bytes
 * - CRC: 4 bytes
 * - Reserved: 12 bytes
 */
typedef struct {
    uint32_t timestamp;         /**< Unix timestamp */
    uint32_t sequence;          /**< Monotonic sequence number */
    uint8_t  type;              /**< Log entry type */
    uint8_t  flags;             /**< Entry flags (synced, etc.) */
    uint16_t payload_len;       /**< Actual payload length */
} __attribute__((packed)) agsys_log_entry_header_t;

#define AGSYS_LOG_FLAG_SYNCED       0x01    /* Entry has been synced to cloud */
#define AGSYS_LOG_FLAG_VALID        0x80    /* Entry is valid (not erased) */

#define AGSYS_LOG_PAYLOAD_SIZE      96      /* Max payload size */

/**
 * @brief Complete log entry structure
 */
typedef struct {
    agsys_log_entry_header_t header;  /* 12 bytes */
    uint8_t  payload[AGSYS_LOG_PAYLOAD_SIZE];  /* 96 bytes */
    uint32_t crc;               /**< CRC32 of header + payload */  /* 4 bytes */
    uint8_t  reserved[16];      /* 16 bytes -> total 128 */
} __attribute__((packed)) agsys_log_entry_t;

/* Verify entry size */
_Static_assert(sizeof(agsys_log_entry_t) == AGSYS_LOG_ENTRY_SIZE,
               "Log entry size mismatch");

/**
 * @brief Sensor reading log payload
 */
typedef struct {
    uint8_t  device_type;       /* 1 */
    uint8_t  probe_count;       /* 1 */
    uint16_t battery_mv;        /* 2 */
    uint16_t readings[4];       /* 8 - Up to 4 probe readings */
    uint8_t  reserved[84];      /* 84 -> total 96 */
} __attribute__((packed)) agsys_log_sensor_t;

/**
 * @brief Meter reading log payload
 */
typedef struct {
    uint32_t flow_rate_mlpm;    /* 4 - Flow rate in mL/min */
    uint32_t total_volume_ml;   /* 4 - Total volume in mL */
    uint8_t  alarm_flags;       /* 1 */
    uint8_t  direction;         /* 1 - 0=forward, 1=reverse */
    uint8_t  reserved[86];      /* 86 -> total 96 */
} __attribute__((packed)) agsys_log_meter_t;

/**
 * @brief Valve event log payload
 */
typedef struct {
    uint8_t  valve_id;          /* 1 */
    uint8_t  event_type;        /* 1 - open, close, fault, etc. */
    uint8_t  position;          /* 1 - 0-100% */
    uint8_t  fault_code;        /* 1 */
    uint32_t duration_ms;       /* 4 */
    uint8_t  reserved[88];      /* 88 -> total 96 */
} __attribute__((packed)) agsys_log_valve_t;

/**
 * @brief Alarm log payload
 */
typedef struct {
    uint8_t  alarm_type;        /* 1 */
    uint8_t  severity;          /* 1 */
    uint16_t alarm_code;        /* 2 */
    uint32_t value;             /* 4 - Associated value */
    char     message[32];       /* 32 - Short message */
    uint8_t  reserved[56];      /* 56 -> total 96 */
} __attribute__((packed)) agsys_log_alarm_t;

/* Verify payload sizes */
_Static_assert(sizeof(agsys_log_sensor_t) == AGSYS_LOG_PAYLOAD_SIZE,
               "Sensor payload size mismatch");
_Static_assert(sizeof(agsys_log_meter_t) == AGSYS_LOG_PAYLOAD_SIZE,
               "Meter payload size mismatch");
_Static_assert(sizeof(agsys_log_valve_t) == AGSYS_LOG_PAYLOAD_SIZE,
               "Valve payload size mismatch");
_Static_assert(sizeof(agsys_log_alarm_t) == AGSYS_LOG_PAYLOAD_SIZE,
               "Alarm payload size mismatch");

/* ==========================================================================
 * LOG HEADER (stored in FRAM at start of log region)
 * ========================================================================== */

typedef struct {
    uint32_t magic;             /**< AGSYS_LOG_MAGIC */
    uint32_t version;           /**< Header version */
    uint32_t head_index;        /**< Next entry index to write */
    uint32_t tail_index;        /**< Oldest unsynced entry index */
    uint32_t total_entries;     /**< Total entries written (including wrapped) */
    uint32_t unsynced_count;    /**< Entries not yet synced */
    uint32_t sequence;          /**< Next sequence number */
    uint32_t wrap_count;        /**< Number of times log wrapped */
    uint8_t  reserved[28];
    uint32_t crc;               /**< CRC32 of header (excluding this field) */
} __attribute__((packed)) agsys_log_fram_header_t;

/* Verify header size */
_Static_assert(sizeof(agsys_log_fram_header_t) == AGSYS_LOG_HEADER_SIZE,
               "Log header size mismatch");

/* ==========================================================================
 * LOG CONTEXT
 * ========================================================================== */

typedef struct {
    agsys_fram_ctx_t *fram;     /**< FRAM driver context */
    agsys_log_fram_header_t header;
    bool     initialized;
} agsys_log_ctx_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize log storage
 * @param ctx Log context
 * @param fram FRAM driver context (must be initialized)
 * @return true on success
 */
bool agsys_log_init(agsys_log_ctx_t *ctx, agsys_fram_ctx_t *fram);

/**
 * @brief Write a log entry
 * @param ctx Log context
 * @param type Entry type
 * @param payload Entry payload (type-specific)
 * @param payload_len Payload length (max AGSYS_LOG_PAYLOAD_SIZE)
 * @return true on success
 */
bool agsys_log_write(agsys_log_ctx_t *ctx, agsys_log_type_t type,
                     const void *payload, size_t payload_len);

/**
 * @brief Read the oldest unsynced entry
 * @param ctx Log context
 * @param entry Output: complete log entry
 * @return true if entry available
 */
bool agsys_log_read_oldest(agsys_log_ctx_t *ctx, agsys_log_entry_t *entry);

/**
 * @brief Mark oldest entry as synced
 * @param ctx Log context
 * @return true on success
 */
bool agsys_log_mark_synced(agsys_log_ctx_t *ctx);

/**
 * @brief Get number of unsynced entries
 * @param ctx Log context
 * @return Number of entries waiting to be synced
 */
uint32_t agsys_log_get_unsynced_count(agsys_log_ctx_t *ctx);

/**
 * @brief Get total entry count
 * @param ctx Log context
 * @return Total entries written (including wrapped)
 */
uint32_t agsys_log_get_total_count(agsys_log_ctx_t *ctx);

/**
 * @brief Erase all log data
 * @param ctx Log context
 * @return true on success
 */
bool agsys_log_erase_all(agsys_log_ctx_t *ctx);

/**
 * @brief Convenience: Log a sensor reading
 */
bool agsys_log_sensor_reading(agsys_log_ctx_t *ctx, uint8_t device_type,
                               const uint16_t *readings, uint8_t count,
                               uint16_t battery_mv);

/**
 * @brief Convenience: Log a meter reading
 */
bool agsys_log_meter_reading(agsys_log_ctx_t *ctx, uint32_t flow_rate_mlpm,
                              uint32_t total_volume_ml, uint8_t alarm_flags);

/**
 * @brief Convenience: Log a valve event
 */
bool agsys_log_valve_event(agsys_log_ctx_t *ctx, uint8_t valve_id,
                            uint8_t event_type, uint8_t position);

/**
 * @brief Convenience: Log an alarm
 */
bool agsys_log_alarm(agsys_log_ctx_t *ctx, uint8_t alarm_type,
                      uint8_t severity, uint16_t code, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_FRAM_LOG_H */
