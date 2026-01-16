/**
 * @file bl_flash.c
 * @brief External SPI Flash driver implementation
 *
 * Supports W25Q series (W25Q128, W25Q16, etc.)
 * Bare-metal implementation for bootloader.
 */

#include "bl_flash.h"
#include "bl_hal.h"
#include "bl_ed25519.h"
#include "bl_log.h"
#include <string.h>

/*******************************************************************************
 * W25Q Flash Commands
 ******************************************************************************/

#define FLASH_CMD_WRITE_ENABLE      0x06
#define FLASH_CMD_WRITE_DISABLE     0x04
#define FLASH_CMD_READ_STATUS1      0x05
#define FLASH_CMD_READ_STATUS2      0x35
#define FLASH_CMD_WRITE_STATUS      0x01
#define FLASH_CMD_READ_DATA         0x03
#define FLASH_CMD_FAST_READ         0x0B
#define FLASH_CMD_PAGE_PROGRAM      0x02
#define FLASH_CMD_SECTOR_ERASE      0x20    /* 4KB */
#define FLASH_CMD_BLOCK_ERASE_32K   0x52    /* 32KB */
#define FLASH_CMD_BLOCK_ERASE_64K   0xD8    /* 64KB */
#define FLASH_CMD_CHIP_ERASE        0xC7
#define FLASH_CMD_READ_ID           0x9F
#define FLASH_CMD_READ_UNIQUE_ID    0x4B
#define FLASH_CMD_POWER_DOWN        0xB9
#define FLASH_CMD_RELEASE_PD        0xAB

/* Status register bits */
#define FLASH_STATUS_BUSY           0x01
#define FLASH_STATUS_WEL            0x02

/* Timing */
#define FLASH_PAGE_SIZE             256
#define FLASH_SECTOR_SIZE           4096
#define FLASH_BLOCK_SIZE            65536

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

static void flash_write_enable(void)
{
    uint8_t cmd = FLASH_CMD_WRITE_ENABLE;
    bl_flash_select();
    bl_spi_transfer(&cmd, NULL, 1);
    bl_flash_deselect();
}

static uint8_t flash_read_status(void)
{
    uint8_t cmd = FLASH_CMD_READ_STATUS1;
    uint8_t status;
    
    bl_flash_select();
    bl_spi_transfer(&cmd, NULL, 1);
    bl_spi_transfer(NULL, &status, 1);
    bl_flash_deselect();
    
    return status;
}

static void flash_wait_busy(void)
{
    while (flash_read_status() & FLASH_STATUS_BUSY) {
        /* Busy wait - could add timeout */
    }
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

bool bl_ext_flash_init(void)
{
    /* Release from power down (in case it was sleeping) */
    uint8_t cmd = FLASH_CMD_RELEASE_PD;
    bl_flash_select();
    bl_spi_transfer(&cmd, NULL, 1);
    bl_flash_deselect();
    
    bl_delay_ms(1);  /* tRES1 = 3us, but be safe */
    
    /* Verify device responds */
    uint8_t mfr;
    uint16_t dev;
    if (!bl_ext_flash_read_id(&mfr, &dev)) {
        return false;
    }
    
    /* Check for known W25Q devices */
    /* Winbond manufacturer ID = 0xEF */
    if (mfr != 0xEF) {
        return false;
    }
    
    return true;
}

bool bl_ext_flash_read_id(uint8_t *manufacturer, uint16_t *device)
{
    uint8_t cmd = FLASH_CMD_READ_ID;
    uint8_t id[3];
    
    bl_flash_select();
    bl_spi_transfer(&cmd, NULL, 1);
    bl_spi_transfer(NULL, id, 3);
    bl_flash_deselect();
    
    if (manufacturer) *manufacturer = id[0];
    if (device) *device = ((uint16_t)id[1] << 8) | id[2];
    
    /* Check for valid response (not all 0xFF or 0x00) */
    if (id[0] == 0xFF || id[0] == 0x00) {
        return false;
    }
    
    return true;
}

bool bl_ext_flash_read(uint32_t addr, uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return false;
    }
    
    /* Command + 24-bit address */
    uint8_t cmd[4] = {
        FLASH_CMD_READ_DATA,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };
    
    bl_flash_select();
    bl_spi_transfer(cmd, NULL, 4);
    bl_spi_transfer(NULL, data, len);
    bl_flash_deselect();
    
    return true;
}

bool bl_ext_flash_write(uint32_t addr, const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return false;
    }
    
    /* Write in page-sized chunks */
    while (len > 0) {
        /* Calculate bytes to write in this page */
        size_t page_offset = addr & (FLASH_PAGE_SIZE - 1);
        size_t page_remaining = FLASH_PAGE_SIZE - page_offset;
        size_t chunk = (len < page_remaining) ? len : page_remaining;
        
        flash_write_enable();
        
        uint8_t cmd[4] = {
            FLASH_CMD_PAGE_PROGRAM,
            (uint8_t)(addr >> 16),
            (uint8_t)(addr >> 8),
            (uint8_t)(addr & 0xFF),
        };
        
        bl_flash_select();
        bl_spi_transfer(cmd, NULL, 4);
        bl_spi_transfer(data, NULL, chunk);
        bl_flash_deselect();
        
        flash_wait_busy();
        
        addr += chunk;
        data += chunk;
        len -= chunk;
    }
    
    return true;
}

bool bl_ext_flash_erase_sector(uint32_t addr)
{
    /* Align to sector boundary */
    addr &= ~(FLASH_SECTOR_SIZE - 1);
    
    flash_write_enable();
    
    uint8_t cmd[4] = {
        FLASH_CMD_SECTOR_ERASE,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };
    
    bl_flash_select();
    bl_spi_transfer(cmd, NULL, 4);
    bl_flash_deselect();
    
    flash_wait_busy();
    
    return true;
}

bool bl_ext_flash_erase_block(uint32_t addr)
{
    /* Align to block boundary */
    addr &= ~(FLASH_BLOCK_SIZE - 1);
    
    flash_write_enable();
    
    uint8_t cmd[4] = {
        FLASH_CMD_BLOCK_ERASE_64K,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };
    
    bl_flash_select();
    bl_spi_transfer(cmd, NULL, 4);
    bl_flash_deselect();
    
    flash_wait_busy();
    
    return true;
}

bool bl_ext_flash_read_slot_header(uint8_t slot, bl_fw_slot_header_t *header)
{
    if (header == NULL || slot > 1) {
        return false;
    }
    
    uint32_t addr = (slot == 0) ? BL_FLASH_SLOT_A_HEADER_ADDR : BL_FLASH_SLOT_B_HEADER_ADDR;
    
    if (!bl_ext_flash_read(addr, (uint8_t *)header, sizeof(*header))) {
        return false;
    }
    
    /* Validate magic */
    if (header->magic != BL_FW_SLOT_MAGIC) {
        return false;
    }
    
    /* Check valid flag */
    if (!(header->flags & BL_FW_SLOT_FLAG_VALID)) {
        return false;
    }
    
    return true;
}

bool bl_ext_flash_validate_slot(uint8_t slot)
{
    bl_fw_slot_header_t header;
    
    if (!bl_ext_flash_read_slot_header(slot, &header)) {
        return false;
    }
    
    /* Check firmware size is reasonable */
    uint32_t max_size = (slot == 0) ? BL_FLASH_SLOT_A_FW_SIZE : BL_FLASH_SLOT_B_FW_SIZE;
    if (header.size == 0 || header.size > max_size) {
        return false;
    }
    
    /* Calculate CRC of firmware data */
    uint32_t fw_addr = (slot == 0) ? BL_FLASH_SLOT_A_FW_ADDR : BL_FLASH_SLOT_B_FW_ADDR;
    uint8_t buf[256];
    uint32_t remaining = header.size;
    uint32_t addr = fw_addr;
    
    /* Use streaming CRC calculation */
    uint32_t calc_crc = 0xFFFFFFFF;
    while (remaining > 0) {
        size_t chunk = (remaining < sizeof(buf)) ? remaining : sizeof(buf);
        
        if (!bl_ext_flash_read(addr, buf, chunk)) {
            return false;
        }
        
        /* Update CRC incrementally */
        for (size_t i = 0; i < chunk; i++) {
            calc_crc ^= buf[i];
            for (int j = 0; j < 8; j++) {
                if (calc_crc & 1) {
                    calc_crc = (calc_crc >> 1) ^ 0xEDB88320;
                } else {
                    calc_crc >>= 1;
                }
            }
        }
        
        addr += chunk;
        remaining -= chunk;
    }
    calc_crc ^= 0xFFFFFFFF;
    
    return (calc_crc == header.crc32);
}

bool bl_ext_flash_verify_signature(uint8_t slot)
{
    bl_fw_slot_header_t header;
    
    /* Read slot header */
    if (!bl_ext_flash_read_slot_header(slot, &header)) {
        return false;
    }
    
    /* Check if firmware is marked as signed */
    if (!(header.flags & BL_FW_SLOT_FLAG_SIGNED)) {
        /* Unsigned firmware - fail verification */
        return false;
    }
    
    (void)slot;  /* Signature verified after restore to internal flash */
    
    /* Actual signature verification is done in restore_firmware()
     * after the firmware is copied to internal flash, since we can
     * then verify directly from the internal flash memory. */
    return true;
}

bool bl_ext_flash_restore_firmware(uint8_t slot)
{
    bl_fw_slot_header_t header;
    
    /* Read and validate slot header */
    if (!bl_ext_flash_read_slot_header(slot, &header)) {
        return false;
    }
    
    /* Validate firmware CRC before restore */
    if (!bl_ext_flash_validate_slot(slot)) {
        return false;
    }
    
    uint32_t fw_addr = (slot == 0) ? BL_FLASH_SLOT_A_FW_ADDR : BL_FLASH_SLOT_B_FW_ADDR;
    uint32_t dest_addr = BL_FLASH_APP_ADDR;
    uint32_t remaining = header.size;
    uint8_t buf[256];
    
    /* Erase application area page by page */
    uint32_t pages_needed = (header.size + BL_FLASH_PAGE_SIZE - 1) / BL_FLASH_PAGE_SIZE;
    for (uint32_t i = 0; i < pages_needed; i++) {
        bl_nvmc_erase_page(BL_FLASH_APP_ADDR + (i * BL_FLASH_PAGE_SIZE));
    }
    
    /* Copy firmware from external flash to internal flash */
    uint32_t src_addr = fw_addr;
    dest_addr = BL_FLASH_APP_ADDR;
    remaining = header.size;
    
    while (remaining > 0) {
        size_t chunk = (remaining < sizeof(buf)) ? remaining : sizeof(buf);
        
        /* Ensure chunk is word-aligned for NVMC */
        chunk = (chunk + 3) & ~3;
        if (chunk > remaining) {
            /* Pad last chunk */
            memset(buf, 0xFF, sizeof(buf));
            chunk = (remaining + 3) & ~3;
        }
        
        if (!bl_ext_flash_read(src_addr, buf, chunk)) {
            return false;
        }
        
        bl_nvmc_write(dest_addr, buf, chunk);
        
        src_addr += chunk;
        dest_addr += chunk;
        remaining -= (remaining < chunk) ? remaining : chunk;
    }
    
    /* Verify restored firmware CRC */
    uint32_t calc_crc = bl_crc32((const void *)BL_FLASH_APP_ADDR, header.size);
    if (calc_crc != header.crc32) {
        return false;
    }
    
    /* Verify signature if firmware is marked as signed */
    if (header.flags & BL_FW_SLOT_FLAG_SIGNED) {
        if (!bl_verify_firmware_signature(
                (const uint8_t *)BL_FLASH_APP_ADDR,
                header.size,
                header.signature)) {
#ifdef BL_DEV_MODE
            /* Dev mode: log warning but allow bad signature */
            bl_log_write(BL_LOG_APP_SIG_FAIL, 0, 0);
            /* Continue anyway in dev mode */
#else
            bl_log_write(BL_LOG_APP_SIG_FAIL, 0, 0);
            return false;
#endif
        }
    }
#ifdef BL_DEV_MODE
    else {
        /* Dev mode: allow unsigned firmware with warning */
        bl_log_write(BL_LOG_APP_UNSIGNED, 0, 0);
    }
#endif
    
    return true;
}
