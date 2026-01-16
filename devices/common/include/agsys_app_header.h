/**
 * @file agsys_app_header.h
 * @brief Application Header for Bootloader Validation
 * 
 * This header defines the application header structure that must be embedded
 * in every AgSys firmware image. The bootloader reads this header to:
 * - Validate the firmware is genuine (magic number)
 * - Check firmware integrity (CRC)
 * - Verify hardware compatibility
 * - Track firmware versions
 * 
 * The header is placed at a fixed offset (0x200) after the vector table,
 * allowing the bootloader to locate it without parsing the entire image.
 * 
 * @see OTA_INTEGRATION.md for integration instructions
 */

#ifndef AGSYS_APP_HEADER_H
#define AGSYS_APP_HEADER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * HEADER LOCATION
 * ========================================================================== */

/**
 * @brief Header location strategy
 * 
 * The app header is placed in a dedicated .app_header section by the linker.
 * At runtime, use agsys_app_header_get() to access it.
 * 
 * The bootloader locates the header by:
 * 1. Reading the header address from a fixed location (end of vector table)
 * 2. Or scanning for the magic number
 * 
 * For post-build patching, the header location is determined from the ELF.
 */
#define AGSYS_APP_HEADER_SECTION    ".app_header"

/**
 * @brief Magic number identifying valid AgSys firmware
 * 
 * ASCII "AGSY" = 0x41 0x47 0x53 0x59
 */
#define AGSYS_APP_HEADER_MAGIC      0x59534741  /* "AGSY" little-endian */

/**
 * @brief Current header format version
 */
#define AGSYS_APP_HEADER_VERSION    1

/* ==========================================================================
 * DEVICE TYPES
 * ========================================================================== */

typedef enum {
    AGSYS_DEVICE_TYPE_UNKNOWN       = 0x00,
    AGSYS_DEVICE_TYPE_SOIL_MOISTURE = 0x01,
    AGSYS_DEVICE_TYPE_VALVE_CONTROL = 0x02,
    AGSYS_DEVICE_TYPE_VALVE_ACTUATOR= 0x03,
    AGSYS_DEVICE_TYPE_WATER_METER   = 0x04,
} agsys_device_type_t;

/* ==========================================================================
 * APPLICATION HEADER STRUCTURE
 * ========================================================================== */

/**
 * @brief Application header structure (48 bytes)
 * 
 * This structure is embedded in the firmware binary at AGSYS_APP_HEADER_OFFSET.
 * The bootloader reads and validates this before jumping to the application.
 * 
 * Fields are ordered for alignment and future extensibility.
 */
typedef struct __attribute__((packed)) {
    /* Identification (8 bytes) */
    uint32_t magic;             /**< Must be AGSYS_APP_HEADER_MAGIC */
    uint8_t  header_version;    /**< Header format version */
    uint8_t  device_type;       /**< agsys_device_type_t */
    uint8_t  hw_revision_min;   /**< Minimum hardware revision supported */
    uint8_t  hw_revision_max;   /**< Maximum hardware revision supported */
    
    /* Firmware Version (4 bytes) */
    uint8_t  fw_version_major;  /**< Semantic version major */
    uint8_t  fw_version_minor;  /**< Semantic version minor */
    uint8_t  fw_version_patch;  /**< Semantic version patch */
    uint8_t  fw_flags;          /**< Firmware flags (debug, etc.) */
    
    /* Firmware Integrity (12 bytes) */
    uint32_t fw_size;           /**< Firmware size in bytes (excluding header) */
    uint32_t fw_crc32;          /**< CRC32 of firmware (from start to fw_size) */
    uint32_t fw_load_addr;      /**< Load address (for verification) */
    
    /* Build Information (20 bytes) */
    uint32_t build_timestamp;   /**< Unix timestamp of build */
    char     build_id[16];      /**< Git hash or build identifier */
    
    /* Header Integrity (4 bytes) */
    uint32_t header_crc32;      /**< CRC32 of header (excluding this field) */
} agsys_app_header_t;

_Static_assert(sizeof(agsys_app_header_t) == 48, "App header must be 48 bytes");

/* ==========================================================================
 * FIRMWARE FLAGS
 * ========================================================================== */

#define AGSYS_FW_FLAG_DEBUG         0x01    /**< Debug build */
#define AGSYS_FW_FLAG_DEVELOPMENT   0x02    /**< Development build (not release) */
#define AGSYS_FW_FLAG_SIGNED        0x04    /**< Firmware is signed (future) */

/* ==========================================================================
 * HELPER MACROS FOR HEADER INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize app header with build-time values
 * 
 * Usage in app_header.c:
 * @code
 * const agsys_app_header_t __attribute__((section(".app_header"))) 
 * g_app_header = AGSYS_APP_HEADER_INIT(
 *     AGSYS_DEVICE_TYPE_SOIL_MOISTURE,
 *     1, 0, 0,    // version 1.0.0
 *     0, 255      // hw rev 0-255
 * );
 * @endcode
 * 
 * Note: fw_size, fw_crc32, and header_crc32 are filled by post-build script.
 */
#define AGSYS_APP_HEADER_INIT(device_type, major, minor, patch, hw_min, hw_max) \
    {                                                                           \
        .magic = AGSYS_APP_HEADER_MAGIC,                                        \
        .header_version = AGSYS_APP_HEADER_VERSION,                             \
        .device_type = (device_type),                                           \
        .hw_revision_min = (hw_min),                                            \
        .hw_revision_max = (hw_max),                                            \
        .fw_version_major = (major),                                            \
        .fw_version_minor = (minor),                                            \
        .fw_version_patch = (patch),                                            \
        .fw_flags = AGSYS_FW_FLAG_DEVELOPMENT,                                  \
        .fw_size = 0xFFFFFFFF,          /* Filled by post-build */              \
        .fw_crc32 = 0xFFFFFFFF,         /* Filled by post-build */              \
        .fw_load_addr = 0x00026000,     /* Default app start (after SoftDevice) */ \
        .build_timestamp = BUILD_TIMESTAMP,                                     \
        .build_id = BUILD_ID,                                                   \
        .header_crc32 = 0xFFFFFFFF,     /* Filled by post-build */              \
    }

/* ==========================================================================
 * RUNTIME API
 * ========================================================================== */

/**
 * @brief Get pointer to application header
 * @return Pointer to app header in flash
 */
const agsys_app_header_t* agsys_app_header_get(void);

/**
 * @brief Get firmware version as packed uint32
 * @return Version as (major << 16) | (minor << 8) | patch
 */
uint32_t agsys_app_header_get_version(void);

/**
 * @brief Get firmware version components
 */
void agsys_app_header_get_version_parts(uint8_t *major, uint8_t *minor, uint8_t *patch);

/**
 * @brief Validate header integrity
 * @return true if header is valid
 */
bool agsys_app_header_validate(void);

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_APP_HEADER_H */
