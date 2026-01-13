/**
 * @file agsys_memory_layout.h
 * @brief Shared memory layout definitions for FRAM and External Flash
 * 
 * This header defines the canonical memory layout for all AgSys devices.
 * All devices MUST use these definitions to ensure consistent data storage
 * and enable safe firmware updates with layout migration.
 * 
 * IMPORTANT: The Layout Header at FRAM address 0x0000 is FROZEN FOREVER.
 * When any region address changes, increment AGSYS_LAYOUT_VERSION and
 * provide migration code in agsys_layout_migrate().
 * 
 * Memory Layout Philosophy:
 * - Growth buffers between regions allow expansion without shifting data
 * - Layout versioning enables safe migration between firmware versions
 * - Same layout across all device types for code reuse
 * 
 * Hardware:
 * - FRAM: MB85RS1MT (128KB) - Fujitsu/RAMXEED, 10^14 write cycles
 * - Flash: W25Q16 (2MB) - Winbond, 100K erase cycles
 */

#ifndef AGSYS_MEMORY_LAYOUT_H
#define AGSYS_MEMORY_LAYOUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * LAYOUT VERSION
 * 
 * MUST be incremented when any region address changes.
 * Migration code must be provided for each version transition.
 * ========================================================================== */

#define AGSYS_LAYOUT_VERSION        1
#define AGSYS_LAYOUT_MAGIC          0x41475359  /* "AGSY" in little-endian */

/* ==========================================================================
 * DEVICE TYPES
 * ========================================================================== */

#define AGSYS_DEVICE_TYPE_UNKNOWN           0
#define AGSYS_DEVICE_TYPE_SOIL_MOISTURE     1
#define AGSYS_DEVICE_TYPE_VALVE_CONTROLLER  2
#define AGSYS_DEVICE_TYPE_WATER_METER       3
#define AGSYS_DEVICE_TYPE_VALVE_ACTUATOR    4

/* ==========================================================================
 * FRAM MEMORY LAYOUT (MB85RS1MT - 128KB)
 * 
 * Each region has a growth buffer after it for future expansion.
 * This minimizes layout version changes when regions need to grow.
 * 
 * Layout:
 *   0x00000 - Layout Header (16B)     - FROZEN FOREVER
 *   0x00010 - Boot Info (256B)        - Growth: 240B reserved
 *   0x00200 - Bootloader Info (128B)  - Growth: 128B reserved
 *   0x00300 - Device Config (1KB)     - Growth: 1KB reserved
 *   0x00B00 - Calibration (1KB)       - Growth: 1KB reserved
 *   0x01300 - App Data (8KB)          - Growth: 8KB reserved
 *   0x05300 - Ring Buffer Log (16KB)  - Growth: 16KB reserved
 *   0x0D300 - Future Use (~76KB)      - Unallocated
 * ========================================================================== */

#define AGSYS_FRAM_SIZE                     131072  /* 128KB */

/* Layout Header - FROZEN FOREVER at address 0x0000 */
#define AGSYS_FRAM_LAYOUT_HEADER_ADDR       0x00000
#define AGSYS_FRAM_LAYOUT_HEADER_SIZE       0x00010  /* 16 bytes */

/* Boot Info - OTA state, versions, boot counters */
#define AGSYS_FRAM_BOOT_INFO_ADDR           0x00010
#define AGSYS_FRAM_BOOT_INFO_SIZE           0x00100  /* 256 bytes */
#define AGSYS_FRAM_BOOT_INFO_GROWTH         0x000F0  /* 240 bytes reserved */

/* Bootloader Info - CRC for Recovery Loader validation */
#define AGSYS_FRAM_BL_INFO_ADDR             0x00200
#define AGSYS_FRAM_BL_INFO_SIZE             0x00080  /* 128 bytes */
#define AGSYS_FRAM_BL_INFO_GROWTH           0x00080  /* 128 bytes reserved */

/* Device Config - Settings synced from cloud */
#define AGSYS_FRAM_CONFIG_ADDR              0x00300
#define AGSYS_FRAM_CONFIG_SIZE              0x00400  /* 1KB */
#define AGSYS_FRAM_CONFIG_GROWTH            0x00400  /* 1KB reserved */

/* Calibration - Sensor-specific calibration data */
#define AGSYS_FRAM_CALIB_ADDR               0x00B00
#define AGSYS_FRAM_CALIB_SIZE               0x00400  /* 1KB */
#define AGSYS_FRAM_CALIB_GROWTH             0x00400  /* 1KB reserved */

/* App Data - Device-specific runtime data (schedules, totals, etc.) */
#define AGSYS_FRAM_APP_DATA_ADDR            0x01300
#define AGSYS_FRAM_APP_DATA_SIZE            0x02000  /* 8KB */
#define AGSYS_FRAM_APP_DATA_GROWTH          0x02000  /* 8KB reserved */

/* Ring Buffer Log - Runtime logs for debugging */
#define AGSYS_FRAM_LOG_ADDR                 0x05300
#define AGSYS_FRAM_LOG_SIZE                 0x04000  /* 16KB (~128 entries @ 128B) */
#define AGSYS_FRAM_LOG_GROWTH               0x04000  /* 16KB reserved */

/* Future Use - Unallocated space for new features */
#define AGSYS_FRAM_FUTURE_ADDR              0x0D300
#define AGSYS_FRAM_FUTURE_SIZE              0x12D00  /* ~76KB */

/* Specific addresses within Boot Info region */
#define AGSYS_FRAM_BLE_PIN_ADDR             (AGSYS_FRAM_BOOT_INFO_ADDR + 0x0040)
#define AGSYS_FRAM_BLE_PIN_SIZE             6
#define AGSYS_FRAM_BOOT_COUNT_ADDR          (AGSYS_FRAM_BOOT_INFO_ADDR + 0x0050)
#define AGSYS_FRAM_BOOT_COUNT_SIZE          4
#define AGSYS_FRAM_LAST_ERROR_ADDR          (AGSYS_FRAM_BOOT_INFO_ADDR + 0x0054)
#define AGSYS_FRAM_LAST_ERROR_SIZE          2
#define AGSYS_FRAM_OTA_STATE_ADDR           (AGSYS_FRAM_BOOT_INFO_ADDR + 0x0060)
#define AGSYS_FRAM_OTA_STATE_SIZE           32

/* Crypto keys within Config region */
#define AGSYS_FRAM_CRYPTO_ADDR              (AGSYS_FRAM_CONFIG_ADDR + 0x0380)
#define AGSYS_FRAM_CRYPTO_SIZE              0x0040  /* 64 bytes */

/* Flow meter calibration within Calibration region */
#define AGSYS_FRAM_FLOW_CAL_ADDR            (AGSYS_FRAM_CALIB_ADDR + 0x0000)
#define AGSYS_FRAM_FLOW_CAL_SIZE            0x0080  /* 128 bytes */

/* ==========================================================================
 * EXTERNAL FLASH MEMORY LAYOUT (W25Q16 - 2MB)
 * 
 * A/B firmware slots sized for future larger MCUs (nRF52840).
 * External flash is only written during OTA updates (rare).
 * 
 * Layout:
 *   0x000000 - Slot A Header (4KB)
 *   0x001000 - Slot A Firmware (944KB)
 *   0x0ED000 - Slot B Header (4KB)
 *   0x0EE000 - Slot B Firmware (944KB)
 *   0x1DA000 - Bootloader Backup (16KB)
 *   0x1DE000 - Reserved (136KB)
 * ========================================================================== */

#define AGSYS_FLASH_SIZE                    (2 * 1024 * 1024)  /* 2MB */

/* Slot A - Firmware backup */
#define AGSYS_FLASH_SLOT_A_HEADER_ADDR      0x000000
#define AGSYS_FLASH_SLOT_A_HEADER_SIZE      0x001000  /* 4KB */
#define AGSYS_FLASH_SLOT_A_FW_ADDR          0x001000
#define AGSYS_FLASH_SLOT_A_FW_SIZE          0x0EC000  /* 944KB */

/* Slot B - OTA staging */
#define AGSYS_FLASH_SLOT_B_HEADER_ADDR      0x0ED000
#define AGSYS_FLASH_SLOT_B_HEADER_SIZE      0x001000  /* 4KB */
#define AGSYS_FLASH_SLOT_B_FW_ADDR          0x0EE000
#define AGSYS_FLASH_SLOT_B_FW_SIZE          0x0EC000  /* 944KB */

/* Bootloader backup for Recovery Loader */
#define AGSYS_FLASH_BL_BACKUP_ADDR          0x1DA000
#define AGSYS_FLASH_BL_BACKUP_SIZE          0x004000  /* 16KB */

/* Reserved for future use */
#define AGSYS_FLASH_RESERVED_ADDR           0x1DE000
#define AGSYS_FLASH_RESERVED_SIZE           0x022000  /* 136KB */

/* ==========================================================================
 * LAYOUT HEADER STRUCTURE
 * 
 * This structure is FROZEN FOREVER at FRAM address 0x0000.
 * It MUST NOT change size or field order across any firmware version.
 * ========================================================================== */

/**
 * @brief Layout header - FROZEN FOREVER at FRAM address 0x0000
 * 
 * Read by Recovery Loader and all firmware versions to determine layout.
 * Size: 16 bytes (must never change)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /**< 0x41475359 ("AGSY") */
    uint8_t  layout_version;    /**< Increment on any region change */
    uint8_t  device_type;       /**< AGSYS_DEVICE_TYPE_* */
    uint16_t reserved1;         /**< Reserved for future use */
    uint32_t crc32;             /**< CRC32 of bytes 0-7 */
    uint32_t reserved2;         /**< Reserved for future use */
} agsys_layout_header_t;

_Static_assert(sizeof(agsys_layout_header_t) == 16, "Layout header must be 16 bytes");

/* ==========================================================================
 * FIRMWARE SLOT HEADER STRUCTURE
 * 
 * Stored at the beginning of each firmware slot in external flash.
 * ========================================================================== */

/**
 * @brief Firmware slot header - stored in external flash
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /**< 0x41475346 ("AGSF") */
    uint32_t version;           /**< Firmware version (major.minor.patch encoded) */
    uint32_t size;              /**< Firmware size in bytes */
    uint32_t crc32;             /**< CRC32 of firmware data */
    uint8_t  device_type;       /**< Target device type */
    uint8_t  flags;             /**< Slot flags (valid, active, etc.) */
    uint16_t reserved;          /**< Reserved */
    uint32_t timestamp;         /**< Build timestamp (Unix epoch) */
    uint8_t  sha256[32];        /**< SHA-256 hash of firmware */
} agsys_fw_slot_header_t;

#define AGSYS_FW_SLOT_MAGIC         0x41475346  /* "AGSF" */
#define AGSYS_FW_SLOT_FLAG_VALID    0x01
#define AGSYS_FW_SLOT_FLAG_ACTIVE   0x02
#define AGSYS_FW_SLOT_FLAG_PENDING  0x04

/* ==========================================================================
 * OTA STATE STRUCTURE (stored in FRAM Boot Info region)
 * 
 * Persists OTA state across reboots for:
 * - Tracking OTA progress if interrupted
 * - Reporting OTA result (success/rollback) after reboot
 * - Providing error details for failed updates
 * ========================================================================== */

/**
 * @brief OTA state values
 */
#define AGSYS_OTA_STATE_NONE            0x00  /**< No OTA in progress or pending */
#define AGSYS_OTA_STATE_IN_PROGRESS     0x01  /**< OTA transfer in progress */
#define AGSYS_OTA_STATE_PENDING_REBOOT  0x02  /**< OTA complete, pending reboot */
#define AGSYS_OTA_STATE_PENDING_CONFIRM 0x03  /**< Rebooted, awaiting confirmation */
#define AGSYS_OTA_STATE_SUCCESS         0x04  /**< OTA confirmed successful */
#define AGSYS_OTA_STATE_FAILED          0x05  /**< OTA failed */
#define AGSYS_OTA_STATE_ROLLED_BACK     0x06  /**< Rolled back to previous firmware */

/**
 * @brief OTA state structure - stored in FRAM at AGSYS_FRAM_OTA_STATE_ADDR
 * 
 * This structure survives reboots and allows the device to:
 * - Resume interrupted OTA transfers
 * - Report OTA outcome (success/failure/rollback) on next wake
 * - Provide error details for debugging
 * 
 * Size: 32 bytes (must fit in AGSYS_FRAM_OTA_STATE_SIZE)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;                 /**< 0x4F544153 ("OTAS") - validates structure */
    uint8_t  state;                 /**< AGSYS_OTA_STATE_* */
    uint8_t  error_code;            /**< Error code if state is FAILED */
    uint8_t  target_version[3];     /**< Target firmware version (major, minor, patch) */
    uint8_t  previous_version[3];   /**< Previous firmware version (for rollback reporting) */
    uint16_t chunks_received;       /**< Number of chunks received (for resume) */
    uint16_t total_chunks;          /**< Total chunks expected */
    uint32_t firmware_size;         /**< Expected firmware size */
    uint32_t firmware_crc;          /**< Expected firmware CRC */
    uint32_t timestamp;             /**< When OTA started (uptime or Unix time) */
    uint8_t  reserved[4];           /**< Reserved for future use */
} agsys_ota_fram_state_t;

#define AGSYS_OTA_FRAM_MAGIC        0x4F544153  /* "OTAS" */

_Static_assert(sizeof(agsys_ota_fram_state_t) <= 32, "OTA state must fit in 32 bytes");

/* ==========================================================================
 * LAYOUT MIGRATION API
 * ========================================================================== */

/**
 * @brief Check if layout migration is needed
 * 
 * @param current_version Layout version read from FRAM header
 * @return true if migration is needed
 */
static inline bool agsys_layout_needs_migration(uint8_t current_version) {
    return (current_version != 0xFF) && (current_version < AGSYS_LAYOUT_VERSION);
}

/**
 * @brief Check if layout header is valid
 * 
 * @param header Pointer to layout header
 * @return true if header is valid
 */
static inline bool agsys_layout_header_valid(const agsys_layout_header_t *header) {
    return (header->magic == AGSYS_LAYOUT_MAGIC);
}

/**
 * @brief Check if this is a fresh/uninitialized FRAM
 * 
 * @param header Pointer to layout header
 * @return true if FRAM appears uninitialized (all 0xFF)
 */
static inline bool agsys_layout_is_fresh(const agsys_layout_header_t *header) {
    return (header->magic == 0xFFFFFFFF);
}

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_MEMORY_LAYOUT_H */
