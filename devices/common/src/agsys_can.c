/**
 * @file agsys_can.c
 * @brief Shared MCP2515 CAN Controller Driver Implementation
 * 
 * Low-level driver for MCP2515 CAN controller used by both
 * valve controller and valve actuator devices.
 */

#include "sdk_config.h"
#include "agsys_can.h"
#include "SEGGER_RTT.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ==========================================================================
 * MCP2515 SPI COMMANDS
 * ========================================================================== */

#define MCP_CMD_RESET           0xC0
#define MCP_CMD_READ            0x03
#define MCP_CMD_WRITE           0x02
#define MCP_CMD_RTS_TX0         0x81
#define MCP_CMD_RTS_TX1         0x82
#define MCP_CMD_RTS_TX2         0x84
#define MCP_CMD_READ_STATUS     0xA0
#define MCP_CMD_RX_STATUS       0xB0
#define MCP_CMD_BIT_MODIFY      0x05
#define MCP_CMD_READ_RX0        0x90
#define MCP_CMD_READ_RX1        0x94

/* ==========================================================================
 * MCP2515 REGISTERS
 * ========================================================================== */

#define MCP_REG_CANSTAT         0x0E
#define MCP_REG_CANCTRL         0x0F
#define MCP_REG_CNF3            0x28
#define MCP_REG_CNF2            0x29
#define MCP_REG_CNF1            0x2A
#define MCP_REG_CANINTE         0x2B
#define MCP_REG_CANINTF         0x2C
#define MCP_REG_TXB0CTRL        0x30
#define MCP_REG_TXB0SIDH        0x31
#define MCP_REG_TXB0D0          0x36
#define MCP_REG_RXB0CTRL        0x60
#define MCP_REG_RXB0SIDH        0x61
#define MCP_REG_RXB0D0          0x66

/* Interrupt flags */
#define MCP_INT_RX0IF           0x01
#define MCP_INT_RX1IF           0x02
#define MCP_INT_TX0IF           0x04

/* ==========================================================================
 * SPI HELPERS
 * ========================================================================== */

static void spi_transfer(agsys_can_ctx_t *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    agsys_spi_xfer_t xfer = {
        .tx_buf = tx,
        .rx_buf = rx,
        .length = len,
    };
    agsys_spi_transfer(ctx->spi_handle, &xfer);
}

/* ==========================================================================
 * LOW-LEVEL REGISTER ACCESS
 * ========================================================================== */

void agsys_can_write_reg(agsys_can_ctx_t *ctx, uint8_t reg, uint8_t value)
{
    uint8_t tx[3] = { MCP_CMD_WRITE, reg, value };
    spi_transfer(ctx, tx, NULL, 3);
}

uint8_t agsys_can_read_reg(agsys_can_ctx_t *ctx, uint8_t reg)
{
    uint8_t tx[3] = { MCP_CMD_READ, reg, 0x00 };
    uint8_t rx[3];
    spi_transfer(ctx, tx, rx, 3);
    return rx[2];
}

void agsys_can_bit_modify(agsys_can_ctx_t *ctx, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t tx[4] = { MCP_CMD_BIT_MODIFY, reg, mask, value };
    spi_transfer(ctx, tx, NULL, 4);
}

uint8_t agsys_can_get_interrupts(agsys_can_ctx_t *ctx)
{
    return agsys_can_read_reg(ctx, MCP_REG_CANINTF);
}

void agsys_can_clear_interrupts(agsys_can_ctx_t *ctx, uint8_t flags)
{
    agsys_can_bit_modify(ctx, MCP_REG_CANINTF, flags, 0x00);
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

void agsys_can_reset(agsys_can_ctx_t *ctx)
{
    uint8_t cmd = MCP_CMD_RESET;
    spi_transfer(ctx, &cmd, NULL, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

bool agsys_can_set_mode(agsys_can_ctx_t *ctx, agsys_can_mode_t mode)
{
    agsys_can_bit_modify(ctx, MCP_REG_CANCTRL, 0xE0, (uint8_t)mode);
    
    /* Wait for mode change */
    for (int i = 0; i < 10; i++) {
        if ((agsys_can_read_reg(ctx, MCP_REG_CANSTAT) & 0xE0) == (uint8_t)mode) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    SEGGER_RTT_printf(0, "CAN: Failed to set mode 0x%02X\n", mode);
    return false;
}

bool agsys_can_init(agsys_can_ctx_t *ctx, agsys_spi_handle_t spi_handle)
{
    if (ctx == NULL || spi_handle == AGSYS_SPI_INVALID_HANDLE) {
        return false;
    }
    
    memset(ctx, 0, sizeof(agsys_can_ctx_t));
    ctx->spi_handle = spi_handle;
    
    /* Reset MCP2515 */
    agsys_can_reset(ctx);
    
    /* Enter config mode */
    if (!agsys_can_set_mode(ctx, AGSYS_CAN_MODE_CONFIG)) {
        SEGGER_RTT_printf(0, "CAN: Failed to enter config mode\n");
        return false;
    }
    
    /* Configure bit timing for 1 Mbps with 16 MHz crystal
     * TQ = 2/Fosc = 125ns
     * Sync = 1 TQ
     * Prop = 1 TQ
     * PS1 = 3 TQ
     * PS2 = 3 TQ
     * Total = 8 TQ = 1 µs = 1 Mbps
     */
    agsys_can_write_reg(ctx, MCP_REG_CNF1, 0x00);  /* BRP = 0, SJW = 1 */
    agsys_can_write_reg(ctx, MCP_REG_CNF2, 0x90);  /* BTLMODE=1, SAM=0, PHSEG1=2, PRSEG=0 */
    agsys_can_write_reg(ctx, MCP_REG_CNF3, 0x02);  /* PHSEG2=2 */
    
    /* Configure RX buffers - receive all messages */
    agsys_can_write_reg(ctx, MCP_REG_RXB0CTRL, 0x60);  /* RXM=11 (any message), BUKT=0 */
    
    /* Enable RX interrupts */
    agsys_can_write_reg(ctx, MCP_REG_CANINTE, MCP_INT_RX0IF | MCP_INT_RX1IF);
    
    /* Clear interrupt flags */
    agsys_can_write_reg(ctx, MCP_REG_CANINTF, 0x00);
    
    /* Enter normal mode */
    if (!agsys_can_set_mode(ctx, AGSYS_CAN_MODE_NORMAL)) {
        SEGGER_RTT_printf(0, "CAN: Failed to enter normal mode\n");
        return false;
    }
    
    ctx->initialized = true;
    SEGGER_RTT_printf(0, "CAN: MCP2515 initialized (1 Mbps)\n");
    return true;
}

/* ==========================================================================
 * MESSAGE OPERATIONS
 * ========================================================================== */

bool agsys_can_available(agsys_can_ctx_t *ctx)
{
    uint8_t status = agsys_can_read_reg(ctx, MCP_REG_CANINTF);
    return (status & (MCP_INT_RX0IF | MCP_INT_RX1IF)) != 0;
}

bool agsys_can_read(agsys_can_ctx_t *ctx, agsys_can_frame_t *frame)
{
    if (ctx == NULL || frame == NULL) {
        return false;
    }
    
    uint8_t status = agsys_can_read_reg(ctx, MCP_REG_CANINTF);
    
    if (status & MCP_INT_RX0IF) {
        /* Read from RX buffer 0 using fast read command */
        uint8_t tx[14] = { MCP_CMD_READ_RX0 };
        uint8_t rx[14];
        spi_transfer(ctx, tx, rx, 14);
        
        /* Parse ID (standard 11-bit) */
        frame->id = ((uint16_t)rx[1] << 3) | (rx[2] >> 5);
        frame->dlc = rx[5] & 0x0F;
        if (frame->dlc > 8) frame->dlc = 8;
        memcpy(frame->data, &rx[6], frame->dlc);
        
        /* Clear interrupt flag */
        agsys_can_bit_modify(ctx, MCP_REG_CANINTF, MCP_INT_RX0IF, 0x00);
        return true;
    }
    
    return false;
}

bool agsys_can_send(agsys_can_ctx_t *ctx, const agsys_can_frame_t *frame)
{
    if (ctx == NULL || frame == NULL) {
        return false;
    }
    
    /* Wait for TX buffer to be free */
    for (int i = 0; i < 10; i++) {
        if ((agsys_can_read_reg(ctx, MCP_REG_TXB0CTRL) & 0x08) == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    /* Load TX buffer */
    uint8_t sidh = (frame->id >> 3) & 0xFF;
    uint8_t sidl = (frame->id << 5) & 0xE0;
    
    agsys_can_write_reg(ctx, MCP_REG_TXB0SIDH, sidh);
    agsys_can_write_reg(ctx, MCP_REG_TXB0SIDH + 1, sidl);
    agsys_can_write_reg(ctx, MCP_REG_TXB0SIDH + 2, 0);  /* EID8 */
    agsys_can_write_reg(ctx, MCP_REG_TXB0SIDH + 3, 0);  /* EID0 */
    agsys_can_write_reg(ctx, MCP_REG_TXB0SIDH + 4, frame->dlc);
    
    for (int i = 0; i < frame->dlc; i++) {
        agsys_can_write_reg(ctx, MCP_REG_TXB0D0 + i, frame->data[i]);
    }
    
    /* Request to send */
    uint8_t cmd = MCP_CMD_RTS_TX0;
    spi_transfer(ctx, &cmd, NULL, 1);
    
    return true;
}
