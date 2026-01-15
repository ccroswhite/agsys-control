/**
 * @file app_header.c
 * @brief Application Header for Soil Moisture Sensor
 * 
 * This file embeds the application header at a fixed location in flash.
 * The bootloader reads this header to validate the firmware before execution.
 * 
 * BUILD_TIMESTAMP and BUILD_ID are defined by the Makefile.
 */

#include "agsys_app_header.h"

/* Provide defaults if not defined by Makefile */
#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP 0
#endif

#ifndef BUILD_ID
#define BUILD_ID "dev"
#endif

/* Firmware version - update these for each release */
#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION_PATCH    0

/**
 * @brief Application header placed at fixed offset 0x200
 * 
 * The .app_header section is defined in the linker script to be at
 * address 0x26200 (app start 0x26000 + offset 0x200).
 * 
 * Fields fw_size, fw_crc32, and header_crc32 are placeholders (0xFFFFFFFF)
 * that get patched by the post-build script (patch_app_header.py).
 */
const agsys_app_header_t __attribute__((section(".app_header"), used))
g_app_header = {
    .magic              = AGSYS_APP_HEADER_MAGIC,
    .header_version     = AGSYS_APP_HEADER_VERSION,
    .device_type        = AGSYS_DEVICE_TYPE_SOIL_MOISTURE,
    .hw_revision_min    = 0,
    .hw_revision_max    = 255,
    .fw_version_major   = FW_VERSION_MAJOR,
    .fw_version_minor   = FW_VERSION_MINOR,
    .fw_version_patch   = FW_VERSION_PATCH,
    .fw_flags           = AGSYS_FW_FLAG_DEVELOPMENT,
    .fw_size            = 0xFFFFFFFF,       /* Patched by post-build */
    .fw_crc32           = 0xFFFFFFFF,       /* Patched by post-build */
    .fw_load_addr       = 0x00026000,       /* After SoftDevice S132 */
    .build_timestamp    = BUILD_TIMESTAMP,
    .build_id           = BUILD_ID,
    .header_crc32       = 0xFFFFFFFF,       /* Patched by post-build */
};
