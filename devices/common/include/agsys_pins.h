/**
 * @file agsys_pins.h
 * @brief Standardized Pin Definitions for AgSys Devices
 * 
 * This header defines the STANDARD pin assignments for external memory
 * (FRAM and Flash) that are common across ALL AgSys devices.
 * 
 * DESIGN RATIONALE:
 * - Using the same pins for FRAM/Flash across all devices simplifies:
 *   - PCB design (same memory subcircuit)
 *   - Firmware (single driver configuration)
 *   - Manufacturing (same assembly process)
 *   - Testing (same test fixtures)
 * 
 * PIN SELECTION CRITERIA:
 * - Must be available on both nRF52832 (32 GPIO) and nRF52840 (48 GPIO)
 * - Must be on Port 0 (P0.00-P0.31) for compatibility
 * - Avoid NFC pins (P0.09, P0.10) - may be used for NFC antenna
 * - Avoid 32kHz crystal pins (P0.00, P0.01) if external crystal used
 * - Grouped together for clean PCB routing
 * 
 * STANDARD EXTERNAL MEMORY BUS:
 * All devices use a dedicated SPI bus for FRAM and Flash.
 * This bus is separate from other SPI peripherals (LoRa, CAN, Display, etc.)
 * 
 * +--------+-------+------------------------------------------+
 * | Signal | Pin   | Notes                                    |
 * +--------+-------+------------------------------------------+
 * | SCK    | P0.26 | SPI Clock                                |
 * | MOSI   | P0.25 | Master Out, Slave In                     |
 * | MISO   | P0.24 | Master In, Slave Out                     |
 * | FRAM_CS| P0.23 | MB85RS1MT Chip Select (active low)       |
 * | FLASH_CS| P0.22| W25Q16 Chip Select (active low)          |
 * +--------+-------+------------------------------------------+
 */

#ifndef AGSYS_PINS_H
#define AGSYS_PINS_H

#include "nrf_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * STANDARD EXTERNAL MEMORY BUS (FRAM + Flash)
 * 
 * These pins MUST be used by all devices for external memory.
 * Do NOT override these in device-specific board_config.h files.
 * ========================================================================== */

#define AGSYS_MEM_SPI_SCK       NRF_GPIO_PIN_MAP(0, 26)  /* P0.26 */
#define AGSYS_MEM_SPI_MOSI      NRF_GPIO_PIN_MAP(0, 25)  /* P0.25 */
#define AGSYS_MEM_SPI_MISO      NRF_GPIO_PIN_MAP(0, 24)  /* P0.24 */
#define AGSYS_MEM_FRAM_CS       NRF_GPIO_PIN_MAP(0, 23)  /* P0.23 - MB85RS1MT */
#define AGSYS_MEM_FLASH_CS      NRF_GPIO_PIN_MAP(0, 22)  /* P0.22 - W25Q16 */

/* Convenience aliases for backward compatibility */
#define SPI_CS_FRAM_PIN         AGSYS_MEM_FRAM_CS
#define SPI_CS_FLASH_PIN        AGSYS_MEM_FLASH_CS

/* ==========================================================================
 * MEMORY DEVICE SPECIFICATIONS
 * ========================================================================== */

/* MB85RS1MT FRAM - 128KB (1Mbit) */
#define AGSYS_FRAM_SIZE_BYTES   131072
#define AGSYS_FRAM_SIZE_KBYTES  128

/* W25Q16 Flash - 2MB (16Mbit) */
#define AGSYS_FLASH_SIZE_BYTES  2097152
#define AGSYS_FLASH_SIZE_KBYTES 2048

/* ==========================================================================
 * SPI INSTANCE FOR MEMORY BUS
 * 
 * Each device should configure one SPI instance for the memory bus.
 * The instance number may vary by device depending on other SPI usage.
 * ========================================================================== */

/* Default SPI instance for memory bus (can be overridden per device) */
#ifndef AGSYS_MEM_SPI_INSTANCE
#define AGSYS_MEM_SPI_INSTANCE  2
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_PINS_H */
