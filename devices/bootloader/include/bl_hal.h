/**
 * @file bl_hal.h
 * @brief Bootloader Hardware Abstraction Layer
 *
 * Bare-metal HAL for bootloader - no FreeRTOS, no Nordic SDK.
 * Provides minimal SPI, GPIO, and Flash operations.
 */

#ifndef BL_HAL_H
#define BL_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Pin Configuration
 * 
 * Default pins match AgSys custom boards.
 * Override with -DBL_PIN_xxx=n for different hardware (e.g., Adafruit Feather)
 ******************************************************************************/

/* SPI pins (shared bus) */
#ifndef BL_PIN_SPI_SCK
#define BL_PIN_SPI_SCK      25  /* P0.25 */
#endif
#ifndef BL_PIN_SPI_MOSI
#define BL_PIN_SPI_MOSI     24  /* P0.24 */
#endif
#ifndef BL_PIN_SPI_MISO
#define BL_PIN_SPI_MISO     23  /* P0.23 */
#endif

/* FRAM chip select */
#ifndef BL_PIN_FRAM_CS
#define BL_PIN_FRAM_CS      11  /* P0.11 */
#endif

/* External flash chip select */
#ifndef BL_PIN_FLASH_CS
#define BL_PIN_FLASH_CS     12  /* P0.12 */
#endif

/* LED for status indication */
#ifndef BL_PIN_LED
#define BL_PIN_LED          17  /* P0.17 */
#endif

/*******************************************************************************
 * Memory Map Constants
 ******************************************************************************/

/* Internal Flash Layout (nRF52832 with S132) */
#define BL_FLASH_APP_ADDR           0x00026000
#define BL_FLASH_APP_SIZE           0x0004A000  /* 296KB */
#define BL_FLASH_APP_END            0x00070000
#define BL_FLASH_PAGE_SIZE          0x1000      /* 4KB */

/* Bootloader location */
#define BL_FLASH_BL_ADDR            0x00072000
#define BL_FLASH_BL_SIZE            0x00004000  /* 16KB */

/*******************************************************************************
 * Initialization
 ******************************************************************************/

/**
 * @brief Initialize bootloader hardware
 * 
 * Sets up GPIO, SPI for FRAM/Flash access.
 */
void bl_hal_init(void);

/*******************************************************************************
 * GPIO Functions
 ******************************************************************************/

/**
 * @brief Set LED state
 * @param on true = LED on, false = LED off
 */
void bl_led_set(bool on);

/**
 * @brief Toggle LED
 */
void bl_led_toggle(void);

/*******************************************************************************
 * Delay Functions
 ******************************************************************************/

/**
 * @brief Busy-wait delay in milliseconds
 * @param ms Milliseconds to wait
 */
void bl_delay_ms(uint32_t ms);

/*******************************************************************************
 * SPI Functions
 ******************************************************************************/

/**
 * @brief Select FRAM (assert CS)
 */
void bl_fram_select(void);

/**
 * @brief Deselect FRAM (deassert CS)
 */
void bl_fram_deselect(void);

/**
 * @brief Select external flash (assert CS)
 */
void bl_flash_select(void);

/**
 * @brief Deselect external flash (deassert CS)
 */
void bl_flash_deselect(void);

/**
 * @brief SPI transfer (simultaneous TX/RX)
 * @param tx_buf Transmit buffer (NULL to send 0xFF)
 * @param rx_buf Receive buffer (NULL to discard)
 * @param len Number of bytes
 */
void bl_spi_transfer(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len);

/*******************************************************************************
 * Internal Flash (NVMC) Functions
 ******************************************************************************/

/**
 * @brief Erase a flash page
 * @param page_addr Page-aligned address to erase
 */
void bl_nvmc_erase_page(uint32_t page_addr);

/**
 * @brief Write data to internal flash
 * @param addr Destination address (must be word-aligned)
 * @param data Source data
 * @param len Number of bytes (must be multiple of 4)
 */
void bl_nvmc_write(uint32_t addr, const uint8_t *data, size_t len);

/*******************************************************************************
 * CRC32 Functions
 ******************************************************************************/

/**
 * @brief Calculate CRC32 of data
 * @param data Pointer to data
 * @param len Length in bytes
 * @return CRC32 value
 */
uint32_t bl_crc32(const void *data, size_t len);

/*******************************************************************************
 * Boot Functions
 ******************************************************************************/

/**
 * @brief Jump to application
 * @note This function does not return
 */
void bl_jump_to_app(void) __attribute__((noreturn));

/**
 * @brief Enter panic mode (SOS LED pattern)
 * @note This function does not return
 */
void bl_panic(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* BL_HAL_H */
