/**
 * @file bl_log.h
 * @brief FRAM-based logging for bootloader
 *
 * Stores boot events and errors in FRAM for post-mortem analysis.
 * Uses a simple ring buffer with fixed-size entries.
 */

#ifndef BL_LOG_H
#define BL_LOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Log Configuration
 * 
 * Log is stored in FRAM at a dedicated region.
 * For testing with 8KB FRAM (MB85RS64V), we use a smaller region.
 ******************************************************************************/

/* Log region in FRAM (after boot_info) */
#define BL_LOG_FRAM_ADDR        0x0100      /* Start after boot_info */
#define BL_LOG_ENTRY_SIZE       32          /* Bytes per entry */
#define BL_LOG_MAX_ENTRIES      16          /* 512 bytes total */

#define BL_LOG_HEADER_MAGIC     0x424C4C47  /* "BLLG" */

/*******************************************************************************
 * Log Entry Types
 ******************************************************************************/

typedef enum {
    BL_LOG_BOOT_START       = 0x01,
    BL_LOG_BOOT_SUCCESS     = 0x02,
    BL_LOG_BOOT_FAIL        = 0x03,
    BL_LOG_ROLLBACK_START   = 0x10,
    BL_LOG_ROLLBACK_SUCCESS = 0x11,
    BL_LOG_ROLLBACK_FAIL    = 0x12,
    BL_LOG_APP_INVALID      = 0x20,
    BL_LOG_APP_CRC_FAIL     = 0x21,
    BL_LOG_FRAM_ERROR       = 0x30,
    BL_LOG_FLASH_ERROR      = 0x31,
    BL_LOG_NVMC_ERROR       = 0x32,
    BL_LOG_PANIC            = 0xFF,
} bl_log_type_t;

/*******************************************************************************
 * Log Structures
 ******************************************************************************/

/**
 * @brief Log header (stored at BL_LOG_FRAM_ADDR)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /**< BL_LOG_HEADER_MAGIC */
    uint16_t write_index;       /**< Next write position (0 to MAX_ENTRIES-1) */
    uint16_t entry_count;       /**< Total entries written (saturates at MAX) */
    uint32_t boot_count;        /**< Total boot attempts */
    uint32_t rollback_count;    /**< Total rollbacks performed */
    uint32_t crc32;             /**< Header CRC */
} bl_log_header_t;

/**
 * @brief Log entry (32 bytes)
 */
typedef struct __attribute__((packed)) {
    uint32_t sequence;          /**< Monotonic sequence number */
    uint32_t timestamp;         /**< Uptime or RTC if available */
    uint8_t  type;              /**< bl_log_type_t */
    uint8_t  boot_state;        /**< Boot state at time of log */
    uint8_t  boot_count;        /**< Boot count at time of log */
    uint8_t  reserved1;
    uint8_t  version[3];        /**< Firmware version (major.minor.patch) */
    uint8_t  reserved2;
    uint32_t error_code;        /**< Additional error info */
    uint32_t error_addr;        /**< Address where error occurred */
    uint8_t  extra[8];          /**< Extra data (context-dependent) */
} bl_log_entry_t;

_Static_assert(sizeof(bl_log_entry_t) == BL_LOG_ENTRY_SIZE, "Log entry must be 32 bytes");

/*******************************************************************************
 * API Functions
 ******************************************************************************/

/**
 * @brief Initialize the log system
 * 
 * Reads log header from FRAM, initializes if invalid.
 * 
 * @return true on success
 */
bool bl_log_init(void);

/**
 * @brief Write a log entry
 * @param type Log entry type
 * @param error_code Optional error code (0 if none)
 * @param error_addr Optional error address (0 if none)
 */
void bl_log_write(bl_log_type_t type, uint32_t error_code, uint32_t error_addr);

/**
 * @brief Write a log entry with version info
 * @param type Log entry type
 * @param major Firmware major version
 * @param minor Firmware minor version
 * @param patch Firmware patch version
 * @param error_code Optional error code
 */
void bl_log_write_version(bl_log_type_t type, 
                          uint8_t major, uint8_t minor, uint8_t patch,
                          uint32_t error_code);

/**
 * @brief Increment boot count in log header
 */
void bl_log_increment_boot_count(void);

/**
 * @brief Increment rollback count in log header
 */
void bl_log_increment_rollback_count(void);

/**
 * @brief Get log statistics
 * @param boot_count Output: total boot count
 * @param rollback_count Output: total rollback count
 * @param entry_count Output: number of log entries
 */
void bl_log_get_stats(uint32_t *boot_count, uint32_t *rollback_count, uint16_t *entry_count);

/**
 * @brief Read a log entry
 * @param index Entry index (0 = oldest available)
 * @param entry Output: log entry
 * @return true if entry exists
 */
bool bl_log_read_entry(uint16_t index, bl_log_entry_t *entry);

#ifdef __cplusplus
}
#endif

#endif /* BL_LOG_H */
