/**
 * @file bl_flash.h
 * @brief External SPI Flash driver for bootloader
 *
 * Supports W25Q series flash (W25Q128, W25Q16, etc.)
 * Used for firmware backup storage and rollback.
 */

#ifndef BL_FLASH_H
#define BL_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Flash Layout (matches agsys_memory_layout.h)
 * 
 * We use the same layout regardless of flash size (W25Q16 2MB or W25Q128 16MB)
 ******************************************************************************/

#define BL_FLASH_SLOT_A_HEADER_ADDR     0x000000
#define BL_FLASH_SLOT_A_HEADER_SIZE     0x001000    /* 4KB */
#define BL_FLASH_SLOT_A_FW_ADDR         0x001000
#define BL_FLASH_SLOT_A_FW_SIZE         0x0EC000    /* 944KB */

#define BL_FLASH_SLOT_B_HEADER_ADDR     0x0ED000
#define BL_FLASH_SLOT_B_HEADER_SIZE     0x001000    /* 4KB */
#define BL_FLASH_SLOT_B_FW_ADDR         0x0EE000
#define BL_FLASH_SLOT_B_FW_SIZE         0x0EC000    /* 944KB */

/*******************************************************************************
 * Firmware Slot Header (matches agsys_memory_layout.h)
 ******************************************************************************/

#define BL_FW_SLOT_MAGIC            0x41475346  /* "AGSF" */
#define BL_FW_SLOT_FLAG_VALID       0x01
#define BL_FW_SLOT_FLAG_ACTIVE      0x02
#define BL_FW_SLOT_FLAG_PENDING     0x04

typedef struct __attribute__((packed)) {
    uint32_t magic;             /**< BL_FW_SLOT_MAGIC */
    uint32_t version;           /**< Firmware version (encoded) */
    uint32_t size;              /**< Firmware size in bytes */
    uint32_t crc32;             /**< CRC32 of firmware data */
    uint8_t  device_type;       /**< Target device type */
    uint8_t  flags;             /**< Slot flags */
    uint16_t reserved;          /**< Reserved */
    uint32_t timestamp;         /**< Build timestamp */
    uint8_t  sha256[32];        /**< SHA-256 hash (optional) */
} bl_fw_slot_header_t;

/*******************************************************************************
 * API Functions
 ******************************************************************************/

/**
 * @brief Initialize external flash
 * @return true on success
 */
bool bl_ext_flash_init(void);

/**
 * @brief Read flash device ID
 * @param manufacturer Output: manufacturer ID
 * @param device Output: device ID (2 bytes)
 * @return true on success
 */
bool bl_ext_flash_read_id(uint8_t *manufacturer, uint16_t *device);

/**
 * @brief Read data from external flash
 * @param addr Flash address
 * @param data Output buffer
 * @param len Number of bytes to read
 * @return true on success
 */
bool bl_ext_flash_read(uint32_t addr, uint8_t *data, size_t len);

/**
 * @brief Write data to external flash (must be erased first)
 * @param addr Flash address (must be page-aligned for best performance)
 * @param data Data to write
 * @param len Number of bytes to write
 * @return true on success
 */
bool bl_ext_flash_write(uint32_t addr, const uint8_t *data, size_t len);

/**
 * @brief Erase a 4KB sector
 * @param addr Sector-aligned address
 * @return true on success
 */
bool bl_ext_flash_erase_sector(uint32_t addr);

/**
 * @brief Erase a 64KB block
 * @param addr Block-aligned address
 * @return true on success
 */
bool bl_ext_flash_erase_block(uint32_t addr);

/**
 * @brief Read firmware slot header
 * @param slot Slot number (0=A, 1=B)
 * @param header Output: slot header
 * @return true if header is valid
 */
bool bl_ext_flash_read_slot_header(uint8_t slot, bl_fw_slot_header_t *header);

/**
 * @brief Validate firmware in a slot
 * @param slot Slot number (0=A, 1=B)
 * @return true if firmware is valid (magic, CRC check)
 */
bool bl_ext_flash_validate_slot(uint8_t slot);

/**
 * @brief Restore firmware from external flash slot to internal flash
 * @param slot Slot number (0=A, 1=B)
 * @return true on success
 */
bool bl_ext_flash_restore_firmware(uint8_t slot);

#ifdef __cplusplus
}
#endif

#endif /* BL_FLASH_H */
