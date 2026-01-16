/**
 * @file agsys_flash_log.h
 * @brief Encrypted Log Storage for W25Q16 Flash
 * 
 * Provides encrypted ring-buffer log storage for offline operation.
 * Logs are stored when LoRa communication fails and synced later.
 * 
 * Flash Layout (in log region):
 *   - Header sector (4KB): Log metadata, head/tail pointers
 *   - Data sectors: Encrypted log entries
 * 
 * Encryption: AES-128-GCM with device-specific key
 * Key derivation: SHA-256(SECRET_SALT || DEVICE_ID)[0:16]
 */

#ifndef AGSYS_FLASH_LOG_H
#define AGSYS_FLASH_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "agsys_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * LOG CONFIGURATION
 * ========================================================================== */

/* Flash region for logs (1MB, sectors 256-511) */
#define AGSYS_LOG_FLASH_START       0x100000    /* 1MB offset */
#define AGSYS_LOG_FLASH_SIZE        0x100000    /* 1MB for logs */
#define AGSYS_LOG_HEADER_SECTOR     256         /* First sector for header */
#define AGSYS_LOG_DATA_START        0x101000    /* After header sector */

/* Log entry sizes */
#define AGSYS_LOG_ENTRY_SIZE        64          /* Fixed entry size */
#define AGSYS_LOG_ENTRIES_PER_SECTOR (AGSYS_FLASH_SECTOR_SIZE / AGSYS_LOG_ENTRY_SIZE)

/* Encryption */
#define AGSYS_LOG_KEY_SIZE          16          /* AES-128 */
#define AGSYS_LOG_IV_SIZE           12          /* GCM IV */
#define AGSYS_LOG_TAG_SIZE          16          /* GCM auth tag */

/* Header magic */
#define AGSYS_LOG_MAGIC             0x4C4F4753  /* "LOGS" */

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
    AGSYS_LOG_TYPE_DEBUG            = 0x08
} agsys_log_type_t;

/* ==========================================================================
 * LOG ENTRY STRUCTURE
 * ========================================================================== */

/**
 * @brief Log entry header (common to all entry types)
 * 
 * Total entry size: 64 bytes (encrypted)
 * - Header: 16 bytes
 * - Payload: 32 bytes
 * - Auth tag: 16 bytes
 */
typedef struct {
    uint32_t timestamp;         /**< Unix timestamp */
    uint32_t sequence;          /**< Monotonic sequence number */
    uint8_t  type;              /**< Log entry type */
    uint8_t  flags;             /**< Entry flags (synced, etc.) */
    uint16_t payload_len;       /**< Actual payload length */
    uint32_t reserved;          /**< Reserved for future use */
} __attribute__((packed)) agsys_log_header_t;

#define AGSYS_LOG_FLAG_SYNCED       0x01    /* Entry has been synced */
#define AGSYS_LOG_FLAG_ENCRYPTED    0x02    /* Entry is encrypted */

/**
 * @brief Sensor reading log payload
 */
typedef struct {
    uint8_t  device_type;
    uint8_t  probe_count;
    uint16_t battery_mv;
    uint16_t readings[4];       /**< Up to 4 probe readings */
    uint8_t  reserved[20];
} __attribute__((packed)) agsys_log_sensor_t;

/**
 * @brief Meter reading log payload
 */
typedef struct {
    uint32_t flow_rate_mlpm;    /**< Flow rate in mL/min */
    uint32_t total_volume_ml;   /**< Total volume in mL */
    uint8_t  alarm_flags;
    uint8_t  direction;         /**< 0=forward, 1=reverse */
    uint8_t  reserved[22];
} __attribute__((packed)) agsys_log_meter_t;

/**
 * @brief Valve event log payload
 */
typedef struct {
    uint8_t  valve_id;
    uint8_t  event_type;        /**< open, close, fault, etc. */
    uint8_t  position;          /**< 0-100% */
    uint8_t  fault_code;
    uint32_t duration_ms;
    uint8_t  reserved[24];
} __attribute__((packed)) agsys_log_valve_t;

/**
 * @brief Alarm log payload
 */
typedef struct {
    uint8_t  alarm_type;
    uint8_t  severity;
    uint16_t alarm_code;
    uint32_t value;             /**< Associated value */
    uint8_t  message[24];       /**< Short message */
} __attribute__((packed)) agsys_log_alarm_t;

/* ==========================================================================
 * LOG HEADER (stored in flash)
 * ========================================================================== */

typedef struct {
    uint32_t magic;             /**< AGSYS_LOG_MAGIC */
    uint32_t version;           /**< Header version */
    uint32_t head_sector;       /**< Next sector to write */
    uint32_t head_offset;       /**< Offset within head sector */
    uint32_t tail_sector;       /**< Oldest unsynced sector */
    uint32_t tail_offset;       /**< Offset within tail sector */
    uint32_t total_entries;     /**< Total entries written */
    uint32_t unsynced_entries;  /**< Entries not yet synced */
    uint32_t sequence;          /**< Next sequence number */
    uint32_t wrap_count;        /**< Number of times log wrapped */
    uint8_t  reserved[216];
    uint32_t crc;               /**< CRC32 of header */
} __attribute__((packed)) agsys_log_flash_header_t;

/* ==========================================================================
 * LOG CONTEXT
 * ========================================================================== */

typedef struct {
    agsys_flash_ctx_t *flash;   /**< Flash driver context */
    agsys_log_flash_header_t header;
    uint8_t  key[AGSYS_LOG_KEY_SIZE];
    bool     initialized;
} agsys_log_ctx_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize log storage
 * @param ctx Log context
 * @param flash Flash driver context (must be initialized)
 * @return true on success
 */
bool agsys_log_init(agsys_log_ctx_t *ctx, agsys_flash_ctx_t *flash);

/**
 * @brief Write a log entry
 * @param ctx Log context
 * @param type Entry type
 * @param payload Entry payload (type-specific)
 * @param payload_len Payload length
 * @return true on success
 */
bool agsys_log_write(agsys_log_ctx_t *ctx, agsys_log_type_t type,
                     const void *payload, size_t payload_len);

/**
 * @brief Read the oldest unsynced entry
 * @param ctx Log context
 * @param header Output: entry header
 * @param payload Output: entry payload buffer
 * @param payload_size Size of payload buffer
 * @return true if entry available
 */
bool agsys_log_read_oldest(agsys_log_ctx_t *ctx, agsys_log_header_t *header,
                           void *payload, size_t payload_size);

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

#endif /* AGSYS_FLASH_LOG_H */
