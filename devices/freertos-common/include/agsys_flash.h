/**
 * @file agsys_flash.h
 * @brief W25Q16 SPI NOR Flash Driver for FreeRTOS
 * 
 * Low-level driver for Winbond W25Q16 (2MB) SPI flash.
 * Used for encrypted log storage and firmware backup.
 * 
 * Flash Specifications:
 * - Capacity: 2MB (16 Mbit)
 * - Page size: 256 bytes
 * - Sector size: 4KB (smallest erasable unit)
 * - Block size: 64KB
 * - Erase cycles: 100,000 per sector
 */

#ifndef AGSYS_FLASH_H
#define AGSYS_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * FLASH SPECIFICATIONS
 * ========================================================================== */

#define AGSYS_FLASH_SIZE            (2 * 1024 * 1024)   /* 2MB */
#define AGSYS_FLASH_PAGE_SIZE       256
#define AGSYS_FLASH_SECTOR_SIZE     4096                /* 4KB */
#define AGSYS_FLASH_BLOCK_SIZE      65536               /* 64KB */
#define AGSYS_FLASH_SECTOR_COUNT    512
#define AGSYS_FLASH_BLOCK_COUNT     32

/* ==========================================================================
 * W25Q16 COMMANDS
 * ========================================================================== */

#define W25Q_CMD_WRITE_ENABLE       0x06
#define W25Q_CMD_WRITE_DISABLE      0x04
#define W25Q_CMD_READ_STATUS_1      0x05
#define W25Q_CMD_READ_STATUS_2      0x35
#define W25Q_CMD_WRITE_STATUS       0x01
#define W25Q_CMD_READ_DATA          0x03
#define W25Q_CMD_FAST_READ          0x0B
#define W25Q_CMD_PAGE_PROGRAM       0x02
#define W25Q_CMD_SECTOR_ERASE       0x20    /* 4KB */
#define W25Q_CMD_BLOCK_ERASE_32K    0x52
#define W25Q_CMD_BLOCK_ERASE_64K    0xD8
#define W25Q_CMD_CHIP_ERASE         0xC7
#define W25Q_CMD_POWER_DOWN         0xB9
#define W25Q_CMD_RELEASE_PD         0xAB
#define W25Q_CMD_DEVICE_ID          0x90
#define W25Q_CMD_JEDEC_ID           0x9F

/* Status register bits */
#define W25Q_STATUS_BUSY            0x01
#define W25Q_STATUS_WEL             0x02

/* Device IDs */
#define W25Q16_MANUFACTURER_ID      0xEF
#define W25Q16_DEVICE_ID            0x14

/* ==========================================================================
 * FLASH CONTEXT
 * ========================================================================== */

typedef struct {
    uint8_t  spi_handle;        /**< SPI bus handle */
    uint8_t  cs_pin;            /**< Chip select pin */
    bool     initialized;       /**< Initialization status */
    uint8_t  manufacturer_id;   /**< Read from device */
    uint8_t  device_id;         /**< Read from device */
    uint32_t capacity;          /**< Flash capacity in bytes */
} agsys_flash_ctx_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize flash driver
 * @param ctx Flash context
 * @param cs_pin Chip select GPIO pin
 * @return AGSYS_OK on success
 */
int agsys_flash_init(agsys_flash_ctx_t *ctx, uint8_t cs_pin);

/**
 * @brief Read device ID
 * @param ctx Flash context
 * @param manufacturer_id Output: manufacturer ID
 * @param device_id Output: device ID
 * @return true if valid W25Q device detected
 */
bool agsys_flash_read_id(agsys_flash_ctx_t *ctx, uint8_t *manufacturer_id, uint8_t *device_id);

/**
 * @brief Read data from flash
 * @param ctx Flash context
 * @param addr Start address
 * @param data Output buffer
 * @param len Number of bytes to read
 * @return true on success
 */
bool agsys_flash_read(agsys_flash_ctx_t *ctx, uint32_t addr, uint8_t *data, size_t len);

/**
 * @brief Write data to flash (must be erased first)
 * 
 * Handles page boundary crossing automatically.
 * Data must be written to erased (0xFF) locations.
 * 
 * @param ctx Flash context
 * @param addr Start address
 * @param data Data to write
 * @param len Number of bytes to write
 * @return true on success
 */
bool agsys_flash_write(agsys_flash_ctx_t *ctx, uint32_t addr, const uint8_t *data, size_t len);

/**
 * @brief Erase a 4KB sector
 * @param ctx Flash context
 * @param sector_num Sector number (0-511)
 * @return true on success
 */
bool agsys_flash_erase_sector(agsys_flash_ctx_t *ctx, uint16_t sector_num);

/**
 * @brief Erase a 64KB block
 * @param ctx Flash context
 * @param block_num Block number (0-31)
 * @return true on success
 */
bool agsys_flash_erase_block(agsys_flash_ctx_t *ctx, uint8_t block_num);

/**
 * @brief Erase entire chip
 * @param ctx Flash context
 * @return true on success
 */
bool agsys_flash_erase_chip(agsys_flash_ctx_t *ctx);

/**
 * @brief Check if flash is busy
 * @param ctx Flash context
 * @return true if busy
 */
bool agsys_flash_is_busy(agsys_flash_ctx_t *ctx);

/**
 * @brief Wait for flash to be ready
 * @param ctx Flash context
 * @param timeout_ms Maximum wait time in milliseconds
 * @return true if ready, false if timeout
 */
bool agsys_flash_wait_ready(agsys_flash_ctx_t *ctx, uint32_t timeout_ms);

/**
 * @brief Enter power-down mode
 * @param ctx Flash context
 */
void agsys_flash_power_down(agsys_flash_ctx_t *ctx);

/**
 * @brief Release from power-down mode
 * @param ctx Flash context
 */
void agsys_flash_power_up(agsys_flash_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_FLASH_H */
