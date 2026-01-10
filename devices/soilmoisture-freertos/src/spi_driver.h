/**
 * @file spi_driver.h
 * @brief SPI driver for Soil Moisture Sensor
 * 
 * Manages shared SPI bus for RFM95C LoRa, FM25V02 FRAM, and W25Q16 Flash.
 */

#ifndef SPI_DRIVER_H
#define SPI_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialize SPI peripheral
 */
bool spi_init(void);

/**
 * @brief Transfer data with automatic CS handling
 */
bool spi_transfer(uint8_t cs_pin, const uint8_t *tx_buf, uint8_t *rx_buf, size_t len);

/**
 * @brief Assert chip select (active low)
 */
void spi_cs_assert(uint8_t cs_pin);

/**
 * @brief Deassert chip select
 */
void spi_cs_deassert(uint8_t cs_pin);

/**
 * @brief Raw transfer without CS handling
 */
bool spi_transfer_raw(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len);

/**
 * @brief Acquire SPI bus mutex
 */
bool spi_acquire(uint32_t timeout);

/**
 * @brief Release SPI bus mutex
 */
void spi_release(void);

#endif /* SPI_DRIVER_H */
