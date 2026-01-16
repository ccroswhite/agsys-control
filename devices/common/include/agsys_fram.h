/**
 * @file agsys_fram.h
 * @brief FRAM driver for MB85RS1MT (1Mbit SPI F-RAM)
 * 
 * Provides persistent storage for settings, calibration, BLE PIN, and runtime logs.
 * Uses the SPI bus manager for thread-safe access.
 * 
 * Memory layout is defined in agsys_memory_layout.h (shared across all devices).
 * See that header for the canonical memory map and layout versioning details.
 */

#ifndef AGSYS_FRAM_H
#define AGSYS_FRAM_H

#include "agsys_common.h"
#include "agsys_spi.h"
#include "agsys_memory_layout.h"

/* ==========================================================================
 * DEVICE SPECIFICATIONS
 * ========================================================================== */

#define AGSYS_FRAM_SIZE             131072  /* 1Mbit = 128KB */
#define AGSYS_FRAM_PAGE_SIZE        64      /* No actual pages, but useful for alignment */

/* MB85RS1MT SPI Commands */
#define AGSYS_FRAM_CMD_WREN         0x06    /* Write Enable */
#define AGSYS_FRAM_CMD_WRDI         0x04    /* Write Disable */
#define AGSYS_FRAM_CMD_RDSR         0x05    /* Read Status Register */
#define AGSYS_FRAM_CMD_WRSR         0x01    /* Write Status Register */
#define AGSYS_FRAM_CMD_READ         0x03    /* Read Memory */
#define AGSYS_FRAM_CMD_WRITE        0x02    /* Write Memory */
#define AGSYS_FRAM_CMD_RDID         0x9F    /* Read Device ID */

/* ==========================================================================
 * MEMORY LAYOUT ALIASES
 * 
 * Memory layout is defined in agsys_memory_layout.h.
 * These aliases provide backward compatibility with existing code.
 * ========================================================================== */

/* Legacy region aliases - use AGSYS_FRAM_*_ADDR from agsys_memory_layout.h */
#define AGSYS_FRAM_REGION_HEADER        AGSYS_FRAM_LAYOUT_HEADER_ADDR
#define AGSYS_FRAM_REGION_HEADER_SIZE   AGSYS_FRAM_LAYOUT_HEADER_SIZE
#define AGSYS_FRAM_REGION_BOOT_INFO     AGSYS_FRAM_BOOT_INFO_ADDR
#define AGSYS_FRAM_REGION_BOOT_INFO_SIZE    AGSYS_FRAM_BOOT_INFO_SIZE
#define AGSYS_FRAM_REGION_BL_INFO       AGSYS_FRAM_BL_INFO_ADDR
#define AGSYS_FRAM_REGION_BL_INFO_SIZE  AGSYS_FRAM_BL_INFO_SIZE
#define AGSYS_FRAM_REGION_CONFIG        AGSYS_FRAM_CONFIG_ADDR
#define AGSYS_FRAM_REGION_CONFIG_SIZE   AGSYS_FRAM_CONFIG_SIZE
#define AGSYS_FRAM_REGION_CALIB         AGSYS_FRAM_CALIB_ADDR
#define AGSYS_FRAM_REGION_CALIB_SIZE    AGSYS_FRAM_CALIB_SIZE
#define AGSYS_FRAM_REGION_APP_DATA      AGSYS_FRAM_APP_DATA_ADDR
#define AGSYS_FRAM_REGION_APP_DATA_SIZE AGSYS_FRAM_APP_DATA_SIZE
#define AGSYS_FRAM_REGION_LOG           AGSYS_FRAM_LOG_ADDR
#define AGSYS_FRAM_REGION_LOG_SIZE      AGSYS_FRAM_LOG_SIZE

/* Legacy address aliases */
#define AGSYS_FRAM_REGION_SETTINGS      AGSYS_FRAM_CONFIG_ADDR
#define AGSYS_FRAM_REGION_SETTINGS_SIZE AGSYS_FRAM_CONFIG_SIZE
#define AGSYS_FRAM_REGION_CRYPTO        AGSYS_FRAM_CRYPTO_ADDR
#define AGSYS_FRAM_REGION_CRYPTO_SIZE   AGSYS_FRAM_CRYPTO_SIZE
#define AGSYS_FRAM_ADDR_BLE_PIN         AGSYS_FRAM_BLE_PIN_ADDR
#define AGSYS_FRAM_ADDR_BOOT_COUNT      AGSYS_FRAM_BOOT_COUNT_ADDR
#define AGSYS_FRAM_ADDR_LAST_ERROR      AGSYS_FRAM_LAST_ERROR_ADDR

/* Layout version/magic aliases */
#define AGSYS_FRAM_LAYOUT_VERSION       AGSYS_LAYOUT_VERSION
#define AGSYS_FRAM_LAYOUT_MAGIC         AGSYS_LAYOUT_MAGIC

/* Layout header type alias */
typedef agsys_layout_header_t agsys_fram_layout_header_t;

/**
 * @brief FRAM context
 */
typedef struct {
    agsys_spi_handle_t  spi_handle;
    bool                initialized;
    uint8_t             layout_version;  /* Cached from header */
} agsys_fram_ctx_t;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize the FRAM driver on a specific SPI bus
 * 
 * @param ctx       FRAM context
 * @param cs_pin    Chip select GPIO pin
 * @param bus       SPI bus (AGSYS_SPI_BUS_0, AGSYS_SPI_BUS_1, etc.)
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_fram_init_on_bus(agsys_fram_ctx_t *ctx, uint8_t cs_pin, agsys_spi_bus_t bus);

/**
 * @brief Initialize the FRAM driver (uses default bus 0)
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
 * Reads device ID and verifies it matches MB85RS1MT (128KB).
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
 * @param addr      Start address (0 - 131071)
 * @param data      Output buffer
 * @param len       Number of bytes to read
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_fram_read(agsys_fram_ctx_t *ctx,
                             uint32_t addr,
                             uint8_t *data,
                             size_t len);

/**
 * @brief Write data to FRAM
 * 
 * @param ctx       FRAM context
 * @param addr      Start address (0 - 131071)
 * @param data      Data to write
 * @param len       Number of bytes to write
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_fram_write(agsys_fram_ctx_t *ctx,
                              uint32_t addr,
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
                              uint32_t addr,
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
                                     uint32_t addr,
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
                                      uint32_t addr,
                                      const void *data,
                                      size_t len);

#endif /* AGSYS_FRAM_H */
