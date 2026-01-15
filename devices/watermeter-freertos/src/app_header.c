/**
 * @file app_header.c
 * @brief Application Header for Water Meter
 */

#include "agsys_app_header.h"

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP 0
#endif

#ifndef BUILD_ID
#define BUILD_ID "dev"
#endif

#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION_PATCH    0

const agsys_app_header_t __attribute__((section(".app_header"), used))
g_app_header = {
    .magic              = AGSYS_APP_HEADER_MAGIC,
    .header_version     = AGSYS_APP_HEADER_VERSION,
    .device_type        = AGSYS_DEVICE_TYPE_WATER_METER,
    .hw_revision_min    = 0,
    .hw_revision_max    = 255,
    .fw_version_major   = FW_VERSION_MAJOR,
    .fw_version_minor   = FW_VERSION_MINOR,
    .fw_version_patch   = FW_VERSION_PATCH,
    .fw_flags           = AGSYS_FW_FLAG_DEVELOPMENT,
    .fw_size            = 0xFFFFFFFF,
    .fw_crc32           = 0xFFFFFFFF,
    .fw_load_addr       = 0x00027000,       /* After SoftDevice S140 */
    .build_timestamp    = BUILD_TIMESTAMP,
    .build_id           = BUILD_ID,
    .header_crc32       = 0xFFFFFFFF,
};
