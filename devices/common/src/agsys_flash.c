/**
 * @file agsys_flash.c
 * @brief W25Q16 SPI NOR Flash Driver for FreeRTOS
 * 
 * Uses agsys_spi abstraction for thread-safe SPI access.
 */

#include "agsys_flash.h"
#include "agsys_common.h"
#include "agsys_spi.h"
#include "nrf_delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ==========================================================================
 * LOW-LEVEL SPI HELPERS
 * ========================================================================== */

static uint8_t flash_read_status(agsys_flash_ctx_t *ctx)
{
    uint8_t cmd = W25Q_CMD_READ_STATUS_1;
    uint8_t status = 0xFF;
    
    agsys_spi_xfer_t xfers[2] = {
        { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 },
        { .tx_buf = NULL, .rx_buf = &status, .length = 1 }
    };
    
    if (agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2) != AGSYS_OK) {
        return 0xFF;
    }
    
    return status;
}

static void flash_write_enable(agsys_flash_ctx_t *ctx)
{
    uint8_t cmd = W25Q_CMD_WRITE_ENABLE;
    agsys_spi_xfer_t xfer = { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 };
    agsys_spi_transfer(ctx->spi_handle, &xfer);
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

int agsys_flash_init(agsys_flash_ctx_t *ctx, uint8_t cs_pin)
{
    if (ctx == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    
    memset(ctx, 0, sizeof(agsys_flash_ctx_t));
    ctx->cs_pin = cs_pin;
    
    /* Register with SPI manager */
    agsys_spi_config_t spi_config = {
        .cs_pin = cs_pin,
        .cs_active_low = true,
        .frequency = NRF_SPIM_FREQ_4M,
        .mode = 0,
    };
    
    agsys_err_t err = agsys_spi_register(&spi_config, &ctx->spi_handle);
    if (err != AGSYS_OK) {
        return err;
    }
    
    /* Release from power-down if sleeping */
    agsys_flash_power_up(ctx);
    nrf_delay_us(50);  /* tRES1 = 3us typical */
    
    /* Read and verify device ID */
    if (!agsys_flash_read_id(ctx, &ctx->manufacturer_id, &ctx->device_id)) {
        return AGSYS_ERR_SPI;
    }
    
    /* Verify it's a W25Q device */
    if (ctx->manufacturer_id != W25Q16_MANUFACTURER_ID) {
        return AGSYS_ERR_SPI;
    }
    
    /* Set capacity based on device ID */
    switch (ctx->device_id) {
        case 0x14:  /* W25Q16 */
            ctx->capacity = 2 * 1024 * 1024;
            break;
        case 0x15:  /* W25Q32 */
            ctx->capacity = 4 * 1024 * 1024;
            break;
        case 0x16:  /* W25Q64 */
            ctx->capacity = 8 * 1024 * 1024;
            break;
        default:
            ctx->capacity = 2 * 1024 * 1024;  /* Assume W25Q16 */
            break;
    }
    
    ctx->initialized = true;
    return AGSYS_OK;
}

bool agsys_flash_read_id(agsys_flash_ctx_t *ctx, uint8_t *manufacturer_id, uint8_t *device_id)
{
    if (ctx == NULL) {
        return false;
    }
    
    uint8_t cmd[4] = {W25Q_CMD_DEVICE_ID, 0x00, 0x00, 0x00};
    uint8_t rx[2] = {0};
    
    agsys_spi_xfer_t xfers[2] = {
        { .tx_buf = cmd, .rx_buf = NULL, .length = 4 },
        { .tx_buf = NULL, .rx_buf = rx, .length = 2 }
    };
    
    if (agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2) != AGSYS_OK) {
        return false;
    }
    
    if (manufacturer_id) *manufacturer_id = rx[0];
    if (device_id) *device_id = rx[1];
    
    return true;
}

bool agsys_flash_read(agsys_flash_ctx_t *ctx, uint32_t addr, uint8_t *data, size_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL || len == 0) {
        return false;
    }
    
    if (addr + len > ctx->capacity) {
        return false;
    }
    
    uint8_t cmd[4] = {
        W25Q_CMD_READ_DATA,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    agsys_spi_xfer_t xfers[2] = {
        { .tx_buf = cmd, .rx_buf = NULL, .length = 4 },
        { .tx_buf = NULL, .rx_buf = data, .length = len }
    };
    
    return (agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2) == AGSYS_OK);
}

bool agsys_flash_write(agsys_flash_ctx_t *ctx, uint32_t addr, const uint8_t *data, size_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL || len == 0) {
        return false;
    }
    
    if (addr + len > ctx->capacity) {
        return false;
    }
    
    size_t remaining = len;
    size_t offset = 0;
    
    while (remaining > 0) {
        /* Calculate bytes until page boundary */
        size_t page_offset = (addr + offset) % AGSYS_FLASH_PAGE_SIZE;
        size_t page_remaining = AGSYS_FLASH_PAGE_SIZE - page_offset;
        size_t chunk = (remaining < page_remaining) ? remaining : page_remaining;
        
        /* Enable write */
        flash_write_enable(ctx);
        
        /* Page program command */
        uint32_t write_addr = addr + offset;
        uint8_t cmd[4] = {
            W25Q_CMD_PAGE_PROGRAM,
            (write_addr >> 16) & 0xFF,
            (write_addr >> 8) & 0xFF,
            write_addr & 0xFF
        };
        
        agsys_spi_xfer_t xfers[2] = {
            { .tx_buf = cmd, .rx_buf = NULL, .length = 4 },
            { .tx_buf = data + offset, .rx_buf = NULL, .length = chunk }
        };
        
        if (agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2) != AGSYS_OK) {
            return false;
        }
        
        /* Wait for write to complete (typ 0.7ms, max 3ms per page) */
        if (!agsys_flash_wait_ready(ctx, 10)) {
            return false;
        }
        
        offset += chunk;
        remaining -= chunk;
    }
    
    return true;
}

bool agsys_flash_erase_sector(agsys_flash_ctx_t *ctx, uint16_t sector_num)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (sector_num >= AGSYS_FLASH_SECTOR_COUNT) {
        return false;
    }
    
    uint32_t addr = sector_num * AGSYS_FLASH_SECTOR_SIZE;
    
    flash_write_enable(ctx);
    
    uint8_t cmd[4] = {
        W25Q_CMD_SECTOR_ERASE,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    agsys_spi_xfer_t xfer = { .tx_buf = cmd, .rx_buf = NULL, .length = 4 };
    
    if (agsys_spi_transfer(ctx->spi_handle, &xfer) != AGSYS_OK) {
        return false;
    }
    
    /* Wait for erase to complete (typ 45ms, max 400ms) */
    return agsys_flash_wait_ready(ctx, 500);
}

bool agsys_flash_erase_block(agsys_flash_ctx_t *ctx, uint8_t block_num)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (block_num >= AGSYS_FLASH_BLOCK_COUNT) {
        return false;
    }
    
    uint32_t addr = block_num * AGSYS_FLASH_BLOCK_SIZE;
    
    flash_write_enable(ctx);
    
    uint8_t cmd[4] = {
        W25Q_CMD_BLOCK_ERASE_64K,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    agsys_spi_xfer_t xfer = { .tx_buf = cmd, .rx_buf = NULL, .length = 4 };
    
    if (agsys_spi_transfer(ctx->spi_handle, &xfer) != AGSYS_OK) {
        return false;
    }
    
    /* Wait for erase to complete (typ 150ms, max 2000ms) */
    return agsys_flash_wait_ready(ctx, 3000);
}

bool agsys_flash_erase_chip(agsys_flash_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    flash_write_enable(ctx);
    
    uint8_t cmd = W25Q_CMD_CHIP_ERASE;
    agsys_spi_xfer_t xfer = { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 };
    
    if (agsys_spi_transfer(ctx->spi_handle, &xfer) != AGSYS_OK) {
        return false;
    }
    
    /* Wait for erase to complete (typ 25s, max 50s for W25Q16) */
    return agsys_flash_wait_ready(ctx, 60000);
}

bool agsys_flash_is_busy(agsys_flash_ctx_t *ctx)
{
    if (ctx == NULL) {
        return true;
    }
    
    return (flash_read_status(ctx) & W25Q_STATUS_BUSY) != 0;
}

bool agsys_flash_wait_ready(agsys_flash_ctx_t *ctx, uint32_t timeout_ms)
{
    if (ctx == NULL) {
        return false;
    }
    
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    
    while ((flash_read_status(ctx) & W25Q_STATUS_BUSY) != 0) {
        if ((xTaskGetTickCount() - start) >= timeout) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    return true;
}

void agsys_flash_power_down(agsys_flash_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    uint8_t cmd = W25Q_CMD_POWER_DOWN;
    agsys_spi_xfer_t xfer = { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 };
    agsys_spi_transfer(ctx->spi_handle, &xfer);
}

void agsys_flash_power_up(agsys_flash_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    
    uint8_t cmd = W25Q_CMD_RELEASE_PD;
    agsys_spi_xfer_t xfer = { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 };
    agsys_spi_transfer(ctx->spi_handle, &xfer);
    
    nrf_delay_us(5);  /* tRES2 = 3us typical */
}
