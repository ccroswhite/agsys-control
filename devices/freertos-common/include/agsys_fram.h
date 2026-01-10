/**
 * @file agsys_fram.h
 * @brief FRAM driver for FM25V02 (256Kbit SPI F-RAM)
 * 
 * Provides persistent storage for settings, calibration, and BLE PIN.
 * Event logs are stored in W25Q16 flash (see agsys_flash_log.h).
 * Uses the SPI bus manager for thread-safe access.
 */

#ifndef AGSYS_FRAM_H
#define AGSYS_FRAM_H

#include "agsys_common.h"
#include "agsys_spi.h"

/* ==========================================================================
 * DEVICE SPECIFICATIONS
 * ========================================================================== */

#define AGSYS_FRAM_SIZE             32768   /* 256Kbit = 32KB */
#define AGSYS_FRAM_PAGE_SIZE        64      /* No actual pages, but useful for alignment */

/* FM25V02 Commands */
#define AGSYS_FRAM_CMD_WREN         0x06    /* Write Enable */
#define AGSYS_FRAM_CMD_WRDI         0x04    /* Write Disable */
#define AGSYS_FRAM_CMD_RDSR         0x05    /* Read Status Register */
#define AGSYS_FRAM_CMD_WRSR         0x01    /* Write Status Register */
#define AGSYS_FRAM_CMD_READ         0x03    /* Read Memory */
#define AGSYS_FRAM_CMD_WRITE        0x02    /* Write Memory */
#define AGSYS_FRAM_CMD_RDID         0x9F    /* Read Device ID */

/* ==========================================================================
 * MEMORY MAP
 * ========================================================================== */

/* Define memory regions for different data types */
#define AGSYS_FRAM_REGION_SETTINGS      0x0000  /* Device settings */
#define AGSYS_FRAM_REGION_SETTINGS_SIZE 0x0200  /* 512 bytes */

#define AGSYS_FRAM_REGION_CALIB         0x0200  /* Calibration data */
#define AGSYS_FRAM_REGION_CALIB_SIZE    0x0100  /* 256 bytes */

#define AGSYS_FRAM_REGION_CRYPTO        0x0300  /* Crypto keys/salt */
#define AGSYS_FRAM_REGION_CRYPTO_SIZE   0x0040  /* 64 bytes */

/* Specific addresses within regions */
#define AGSYS_FRAM_ADDR_BLE_PIN         0x0010  /* BLE PIN (6 bytes) */
#define AGSYS_FRAM_ADDR_BOOT_COUNT      0x0020  /* Boot count (4 bytes) */
#define AGSYS_FRAM_ADDR_LAST_ERROR      0x0024  /* Last error code (2 bytes) */

/* Note: Event logs are stored in W25Q16 flash, not FRAM (see agsys_flash_log.h) */

/* ==========================================================================
 * TYPES
 * ========================================================================== */

/**
 * @brief FRAM context
 */
typedef struct {
    agsys_spi_handle_t  spi_handle;
    bool                initialized;
} agsys_fram_ctx_t;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize the FRAM driver
 * 
 * @param ctx       FRAM context
 * @param cs_pin    Chip select GPIO pin
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_fram_init(agsys_fram_ctx_t *ctx, uint8_t cs_pin);

/**
 * @brief Deinitialize the FRAM driver
 * 
 * @param ctx       FRAM context
 */
void agsys_fram_deinit(agsys_fram_ctx_t *ctx);

/**
 * @brief Verify FRAM is present and responding
 * 
 * Reads device ID and verifies it matches FM25V02.
 * 
 * @param ctx       FRAM context
 * @return AGSYS_OK if FRAM is present
 */
agsys_err_t agsys_fram_verify(agsys_fram_ctx_t *ctx);

/* ==========================================================================
 * READ / WRITE
 * ========================================================================== */

/**
 * @brief Read data from FRAM
 * 
 * @param ctx       FRAM context
 * @param addr      Start address (0 - 32767)
 * @param data      Output buffer
 * @param len       Number of bytes to read
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_fram_read(agsys_fram_ctx_t *ctx,
                             uint16_t addr,
                             uint8_t *data,
                             size_t len);

/**
 * @brief Write data to FRAM
 * 
 * @param ctx       FRAM context
 * @param addr      Start address (0 - 32767)
 * @param data      Data to write
 * @param len       Number of bytes to write
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_fram_write(agsys_fram_ctx_t *ctx,
                              uint16_t addr,
                              const uint8_t *data,
                              size_t len);

/**
 * @brief Erase a region (fill with 0xFF)
 * 
 * @param ctx       FRAM context
 * @param addr      Start address
 * @param len       Number of bytes to erase
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_fram_erase(agsys_fram_ctx_t *ctx,
                              uint16_t addr,
                              size_t len);

/* ==========================================================================
 * CONVENIENCE FUNCTIONS
 * ========================================================================== */

/**
 * @brief Read a structure from FRAM with CRC validation
 * 
 * @param ctx       FRAM context
 * @param addr      Start address
 * @param data      Output buffer
 * @param len       Structure size (excluding CRC)
 * @return AGSYS_OK on success, AGSYS_ERR_FRAM if CRC mismatch
 */
agsys_err_t agsys_fram_read_checked(agsys_fram_ctx_t *ctx,
                                     uint16_t addr,
                                     void *data,
                                     size_t len);

/**
 * @brief Write a structure to FRAM with CRC
 * 
 * @param ctx       FRAM context
 * @param addr      Start address
 * @param data      Data to write
 * @param len       Structure size (excluding CRC)
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_fram_write_checked(agsys_fram_ctx_t *ctx,
                                      uint16_t addr,
                                      const void *data,
                                      size_t len);

#endif /* AGSYS_FRAM_H */
