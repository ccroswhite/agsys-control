/**
 * @file agsys_app_header.c
 * @brief Application Header Runtime Functions
 * 
 * Provides runtime access to the application header embedded in firmware.
 */

#include "sdk_config.h"
#include "agsys_app_header.h"
#include <stdbool.h>
#include <string.h>

/* ==========================================================================
 * LINKER SYMBOL FOR APP HEADER
 * ========================================================================== */

/* The app header is placed in .app_header section by the linker */
extern const agsys_app_header_t g_app_header;

/* ==========================================================================
 * CRC32 (same algorithm as bootloader)
 * ========================================================================== */

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    static const uint32_t crc_table[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };
    
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc = crc_table[(crc ^ data[i]) & 0x0F] ^ (crc >> 4);
        crc = crc_table[(crc ^ (data[i] >> 4)) & 0x0F] ^ (crc >> 4);
    }
    return ~crc;
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

const agsys_app_header_t* agsys_app_header_get(void)
{
    return &g_app_header;
}

uint32_t agsys_app_header_get_version(void)
{
    const agsys_app_header_t *hdr = agsys_app_header_get();
    return ((uint32_t)hdr->fw_version_major << 16) |
           ((uint32_t)hdr->fw_version_minor << 8) |
           (uint32_t)hdr->fw_version_patch;
}

void agsys_app_header_get_version_parts(uint8_t *major, uint8_t *minor, uint8_t *patch)
{
    const agsys_app_header_t *hdr = agsys_app_header_get();
    if (major) *major = hdr->fw_version_major;
    if (minor) *minor = hdr->fw_version_minor;
    if (patch) *patch = hdr->fw_version_patch;
}

bool agsys_app_header_validate(void)
{
    const agsys_app_header_t *hdr = agsys_app_header_get();
    
    /* Check magic */
    if (hdr->magic != AGSYS_APP_HEADER_MAGIC) {
        return false;
    }
    
    /* Check header version */
    if (hdr->header_version == 0 || hdr->header_version > AGSYS_APP_HEADER_VERSION) {
        return false;
    }
    
    /* Validate header CRC (excludes the CRC field itself) */
    uint32_t calc_crc = crc32_update(0, (const uint8_t *)hdr, 
                                      sizeof(agsys_app_header_t) - sizeof(hdr->header_crc32));
    if (calc_crc != hdr->header_crc32) {
        return false;
    }
    
    return true;
}
