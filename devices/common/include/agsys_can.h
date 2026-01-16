/**
 * @file agsys_can.h
 * @brief Shared MCP2515 CAN Controller Driver
 * 
 * Low-level driver for MCP2515 CAN controller used by both
 * valve controller and valve actuator devices.
 */

#ifndef AGSYS_CAN_H
#define AGSYS_CAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "agsys_spi.h"

/* ==========================================================================
 * CAN PROTOCOL DEFINITIONS
 * ========================================================================== */

/* CAN IDs for valve system communication */
#define AGSYS_CAN_ID_CMD_BASE           0x100   /* Command base: 0x100 + cmd */
#define AGSYS_CAN_ID_STATUS_BASE        0x180   /* Status response: 0x180 + addr */
#define AGSYS_CAN_ID_UID_RESP_BASE      0x190   /* UID response: 0x190 + addr */
#define AGSYS_CAN_ID_DISCOVER           0x1F0   /* Discovery broadcast */
#define AGSYS_CAN_ID_DISCOVER_RESP      0x1F1   /* Discovery response */
#define AGSYS_CAN_ID_EMERGENCY          0x1FF   /* Emergency close broadcast */

/* Wire-level command codes (added to CMD_BASE) */
#define AGSYS_CAN_WIRE_CMD_OPEN         0x00
#define AGSYS_CAN_WIRE_CMD_CLOSE        0x01
#define AGSYS_CAN_WIRE_CMD_STOP         0x02
#define AGSYS_CAN_WIRE_CMD_STATUS       0x03
#define AGSYS_CAN_WIRE_CMD_EMERGENCY    0x04

/* Actuator address range */
#define AGSYS_CAN_ADDR_MIN              1
#define AGSYS_CAN_ADDR_MAX              64

/* Discovery response delay per address (ms) to avoid collisions */
#define AGSYS_CAN_DISCOVERY_DELAY_MS    5

/* Heartbeat/discovery interval (ms) */
#define AGSYS_CAN_HEARTBEAT_INTERVAL_MS 30000

/* ==========================================================================
 * DATA STRUCTURES
 * ========================================================================== */

/**
 * @brief CAN frame structure (standard 11-bit ID)
 */
typedef struct {
    uint16_t id;        /**< 11-bit CAN identifier */
    uint8_t dlc;        /**< Data length code (0-8) */
    uint8_t data[8];    /**< Data bytes */
} agsys_can_frame_t;

/**
 * @brief MCP2515 context structure
 */
typedef struct {
    agsys_spi_handle_t spi_handle;  /**< SPI handle for communication */
    uint8_t cs_pin;                  /**< Chip select pin (for legacy API) */
    bool initialized;                /**< Initialization flag */
} agsys_can_ctx_t;

/**
 * @brief MCP2515 operating modes
 */
typedef enum {
    AGSYS_CAN_MODE_NORMAL   = 0x00,
    AGSYS_CAN_MODE_SLEEP    = 0x20,
    AGSYS_CAN_MODE_LOOPBACK = 0x40,
    AGSYS_CAN_MODE_LISTEN   = 0x60,
    AGSYS_CAN_MODE_CONFIG   = 0x80,
} agsys_can_mode_t;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize MCP2515 CAN controller
 * 
 * @param ctx       CAN context to initialize
 * @param spi_handle SPI handle from agsys_spi_register()
 * @return true on success
 */
bool agsys_can_init(agsys_can_ctx_t *ctx, agsys_spi_handle_t spi_handle);

/**
 * @brief Reset MCP2515
 * 
 * @param ctx CAN context
 */
void agsys_can_reset(agsys_can_ctx_t *ctx);

/**
 * @brief Set MCP2515 operating mode
 * 
 * @param ctx  CAN context
 * @param mode Operating mode
 * @return true if mode change successful
 */
bool agsys_can_set_mode(agsys_can_ctx_t *ctx, agsys_can_mode_t mode);

/* ==========================================================================
 * MESSAGE OPERATIONS
 * ========================================================================== */

/**
 * @brief Read a CAN message from RX buffer
 * 
 * @param ctx   CAN context
 * @param frame Frame to populate
 * @return true if message was available
 */
bool agsys_can_read(agsys_can_ctx_t *ctx, agsys_can_frame_t *frame);

/**
 * @brief Send a CAN message
 * 
 * @param ctx   CAN context
 * @param frame Frame to send
 * @return true on success
 */
bool agsys_can_send(agsys_can_ctx_t *ctx, const agsys_can_frame_t *frame);

/**
 * @brief Check if messages are pending in RX buffer
 * 
 * @param ctx CAN context
 * @return true if messages available
 */
bool agsys_can_available(agsys_can_ctx_t *ctx);

/* ==========================================================================
 * LOW-LEVEL REGISTER ACCESS
 * ========================================================================== */

/**
 * @brief Write MCP2515 register
 * 
 * @param ctx   CAN context
 * @param reg   Register address
 * @param value Value to write
 */
void agsys_can_write_reg(agsys_can_ctx_t *ctx, uint8_t reg, uint8_t value);

/**
 * @brief Read MCP2515 register
 * 
 * @param ctx CAN context
 * @param reg Register address
 * @return Register value
 */
uint8_t agsys_can_read_reg(agsys_can_ctx_t *ctx, uint8_t reg);

/**
 * @brief Modify bits in MCP2515 register
 * 
 * @param ctx   CAN context
 * @param reg   Register address
 * @param mask  Bit mask
 * @param value New values for masked bits
 */
void agsys_can_bit_modify(agsys_can_ctx_t *ctx, uint8_t reg, uint8_t mask, uint8_t value);

/**
 * @brief Get interrupt flags
 * 
 * @param ctx CAN context
 * @return CANINTF register value
 */
uint8_t agsys_can_get_interrupts(agsys_can_ctx_t *ctx);

/**
 * @brief Clear interrupt flags
 * 
 * @param ctx   CAN context
 * @param flags Flags to clear
 */
void agsys_can_clear_interrupts(agsys_can_ctx_t *ctx, uint8_t flags);

#endif /* AGSYS_CAN_H */
