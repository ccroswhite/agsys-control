/**
 * @file agsys_fram.c
 * @brief FRAM driver implementation for MB85RS1MT (128KB)
 * 
 * Using MB85RS1MT (128KB) to accommodate extensive runtime logging in FRAM,
 * extending external flash lifetime.
 */

#include "agsys_fram.h"
#include "agsys_debug.h"

/* ==========================================================================
 * PRIVATE HELPERS
 * ========================================================================== */

static agsys_err_t fram_write_enable(agsys_fram_ctx_t *ctx)
{
    uint8_t cmd = AGSYS_FRAM_CMD_WREN;
    agsys_spi_xfer_t xfer = {
        .tx_buf = &cmd,
        .rx_buf = NULL,
        .length = 1,
    };
    return agsys_spi_transfer(ctx->spi_handle, &xfer);
}

static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

agsys_err_t agsys_fram_init_on_bus(agsys_fram_ctx_t *ctx, uint8_t cs_pin, agsys_spi_bus_t bus)
{
    if (ctx == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Register with SPI manager on specified bus */
    agsys_spi_config_t spi_config = {
        .cs_pin = cs_pin,
        .cs_active_low = true,
        .frequency = NRF_SPIM_FREQ_4M,
        .mode = 0,
        .bus = bus,
    };

    agsys_err_t err = agsys_spi_register(&spi_config, &ctx->spi_handle);
    if (err != AGSYS_OK) {
        AGSYS_LOG_ERROR("FRAM: Failed to register SPI on bus %d", bus);
        return err;
    }

    ctx->initialized = true;

    /* Verify FRAM is present */
    err = agsys_fram_verify(ctx);
    if (err != AGSYS_OK) {
        AGSYS_LOG_WARNING("FRAM: Device not detected");
        /* Don't fail init - device might be absent in some configurations */
    }

    AGSYS_LOG_INFO("FRAM: Initialized (CS=%d, bus=%d)", cs_pin, bus);
    return AGSYS_OK;
}

agsys_err_t agsys_fram_init(agsys_fram_ctx_t *ctx, uint8_t cs_pin)
{
    /* Default to bus 0 for backward compatibility */
    return agsys_fram_init_on_bus(ctx, cs_pin, AGSYS_SPI_BUS_0);
}

void agsys_fram_deinit(agsys_fram_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }

    agsys_spi_unregister(ctx->spi_handle);
    ctx->initialized = false;
}

agsys_err_t agsys_fram_verify(agsys_fram_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    /* Read device ID */
    uint8_t cmd = AGSYS_FRAM_CMD_RDID;
    uint8_t id[9];

    agsys_spi_xfer_t xfers[2] = {
        { .tx_buf = &cmd, .rx_buf = NULL, .length = 1 },
        { .tx_buf = NULL, .rx_buf = id, .length = 9 },
    };

    agsys_err_t err = agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2);
    if (err != AGSYS_OK) {
        return err;
    }

    /* MB85RS1MT ID: 0x7F 0x7F 0x7F 0x7F 0x7F 0x7F 0x04 0x27 0x03 */
    /* Check manufacturer (Fujitsu/RAMXEED) and density */
    if (id[6] == 0x04 && id[7] == 0x27) {
        AGSYS_LOG_DEBUG("FRAM: MB85RS1MT (128KB) detected");
        return AGSYS_OK;
    }

    AGSYS_LOG_ERROR("FRAM: Unsupported device ID: %02X %02X (expected MB85RS1MT)", id[6], id[7]);
    return AGSYS_ERR_FRAM;
}

/* ==========================================================================
 * READ / WRITE
 * ========================================================================== */

agsys_err_t agsys_fram_read(agsys_fram_ctx_t *ctx,
                             uint32_t addr,
                             uint8_t *data,
                             size_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (addr + len > AGSYS_FRAM_SIZE) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Command: READ + 24-bit address (MB85RS1MT requires 3 address bytes) */
    uint8_t cmd[4] = {
        AGSYS_FRAM_CMD_READ,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };

    agsys_spi_xfer_t xfers[2] = {
        { .tx_buf = cmd, .rx_buf = NULL, .length = 4 },
        { .tx_buf = NULL, .rx_buf = data, .length = len },
    };

    return agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2);
}

agsys_err_t agsys_fram_write(agsys_fram_ctx_t *ctx,
                              uint32_t addr,
                              const uint8_t *data,
                              size_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (addr + len > AGSYS_FRAM_SIZE) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Enable write */
    agsys_err_t err = fram_write_enable(ctx);
    if (err != AGSYS_OK) {
        return err;
    }

    /* Command: WRITE + 24-bit address (MB85RS1MT requires 3 address bytes) */
    uint8_t cmd[4] = {
        AGSYS_FRAM_CMD_WRITE,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };

    agsys_spi_xfer_t xfers[2] = {
        { .tx_buf = cmd, .rx_buf = NULL, .length = 4 },
        { .tx_buf = data, .rx_buf = NULL, .length = len },
    };

    return agsys_spi_transfer_multi(ctx->spi_handle, xfers, 2);
}

agsys_err_t agsys_fram_erase(agsys_fram_ctx_t *ctx,
                              uint32_t addr,
                              size_t len)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (addr + len > AGSYS_FRAM_SIZE) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Erase in chunks to avoid large stack allocation */
    uint8_t ff_buf[64];
    memset(ff_buf, 0xFF, sizeof(ff_buf));

    while (len > 0) {
        size_t chunk = AGSYS_MIN(len, sizeof(ff_buf));
        agsys_err_t err = agsys_fram_write(ctx, addr, ff_buf, chunk);
        if (err != AGSYS_OK) {
            return err;
        }
        addr += chunk;
        len -= chunk;
    }

    return AGSYS_OK;
}

/* ==========================================================================
 * CONVENIENCE FUNCTIONS
 * ========================================================================== */

agsys_err_t agsys_fram_read_checked(agsys_fram_ctx_t *ctx,
                                     uint32_t addr,
                                     void *data,
                                     size_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Read data + CRC */
    uint8_t *buf = (uint8_t *)data;
    uint16_t stored_crc;

    agsys_err_t err = agsys_fram_read(ctx, addr, buf, len);
    if (err != AGSYS_OK) {
        return err;
    }

    err = agsys_fram_read(ctx, addr + len, (uint8_t *)&stored_crc, 2);
    if (err != AGSYS_OK) {
        return err;
    }

    /* Verify CRC */
    uint16_t calc_crc = crc16_ccitt(buf, len);
    if (calc_crc != stored_crc) {
        AGSYS_LOG_WARNING("FRAM: CRC mismatch at 0x%05lX (stored=%04X, calc=%04X)",
                          (unsigned long)addr, stored_crc, calc_crc);
        return AGSYS_ERR_FRAM;
    }

    return AGSYS_OK;
}

agsys_err_t agsys_fram_write_checked(agsys_fram_ctx_t *ctx,
                                      uint32_t addr,
                                      const void *data,
                                      size_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    /* Write data */
    agsys_err_t err = agsys_fram_write(ctx, addr, (const uint8_t *)data, len);
    if (err != AGSYS_OK) {
        return err;
    }

    /* Calculate and write CRC */
    uint16_t crc = crc16_ccitt((const uint8_t *)data, len);
    return agsys_fram_write(ctx, addr + len, (const uint8_t *)&crc, 2);
}
