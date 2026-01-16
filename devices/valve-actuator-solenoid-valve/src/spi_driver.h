/**
 * @file spi_driver.h
 * @brief Simple SPI driver for Valve Actuator
 * 
 * Provides basic SPI operations for MCP2515 CAN and FRAM.
 * Uses nRF5 SDK legacy SPI driver with EasyDMA.
 */

#ifndef SPI_DRIVER_H
#define SPI_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize SPI bus
 * @return true on success
 */
bool spi_init(void);

/**
 * @brief Transfer data on SPI bus
 * 
 * Asserts CS, transfers data, deasserts CS.
 * 
 * @param cs_pin Chip select pin
 * @param tx_buf Transmit buffer (can be NULL for RX only)
 * @param rx_buf Receive buffer (can be NULL for TX only)
 * @param len Transfer length
 * @return true on success
 */
bool spi_transfer(uint8_t cs_pin, const uint8_t *tx_buf, uint8_t *rx_buf, size_t len);

/**
 * @brief Transfer with CS held between multiple operations
 * 
 * Use spi_cs_assert/spi_cs_deassert with spi_transfer_raw for
 * multi-part transactions.
 */
void spi_cs_assert(uint8_t cs_pin);
void spi_cs_deassert(uint8_t cs_pin);
bool spi_transfer_raw(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len);

#endif /* SPI_DRIVER_H */
