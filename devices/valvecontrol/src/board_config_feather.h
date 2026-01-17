/**
 * @file board_config_feather.h
 * @brief Hardware pin definitions for Adafruit Feather nRF52832 development board
 * 
 * This configuration is for testing the Valve Controller firmware on
 * an Adafruit Feather nRF52832 with external breakout boards for:
 * - MCP2515 CAN controller
 * - RFM95C LoRa module
 * - MB85RS1MT FRAM (128KB)
 * - W25Q16 SPI Flash (2MB)
 * 
 * FEATHER PIN RESTRICTIONS:
 * - P0.20: DFU pin - must be HIGH at boot
 * - P0.22: Factory Reset (FRST) - must be HIGH at boot
 * - P0.31/A7: Hardwired to battery voltage divider
 * 
 * TESTING NOTES:
 * - No RTC connected for Feather testing (uses system tick)
 * - Power fail simulated via button or always-high
 * - Single LED for status indication
 */

#ifndef BOARD_CONFIG_FEATHER_H
#define BOARD_CONFIG_FEATHER_H

#include "nrf_gpio.h"

/* Prevent agsys_pins.h from being included - we define our own pins */
#define AGSYS_PINS_H

/* ==========================================================================
 * SPI BUS 0 - CAN + LoRa (MCP2515, RFM95C)
 * Using Feather's hardware SPI pins
 * ========================================================================== */

#define SPI_PERIPH_SCK_PIN      14      /* Feather SCK */
#define SPI_PERIPH_MOSI_PIN     13      /* Feather MOSI */
#define SPI_PERIPH_MISO_PIN     12      /* Feather MISO */
#define SPI_CS_CAN_PIN          11      /* D11 - CAN CS */
#define SPI_CS_LORA_PIN         10      /* D10 - LoRa CS (avoid P0.31 battery) */

/* ==========================================================================
 * SPI BUS 1 - External Memory (FRAM + Flash breakouts)
 * Remapped to avoid Feather conflicts
 * ========================================================================== */

#define AGSYS_MEM_SPI_SCK       NRF_GPIO_PIN_MAP(0, 26)  /* P0.26/SCL */
#define AGSYS_MEM_SPI_MOSI      NRF_GPIO_PIN_MAP(0, 25)  /* P0.25/SDA */
#define AGSYS_MEM_SPI_MISO      NRF_GPIO_PIN_MAP(0, 24)  /* P0.24 */
#define AGSYS_MEM_FRAM_CS       NRF_GPIO_PIN_MAP(0, 23)  /* P0.23 - FRAM CS */
#define AGSYS_MEM_FLASH_CS      NRF_GPIO_PIN_MAP(0, 15)  /* P0.15 - Flash CS (avoid P0.22/FRST!) */

/* Convenience aliases */
#define SPI_CS_FRAM_PIN         AGSYS_MEM_FRAM_CS
#define SPI_CS_FLASH_PIN        AGSYS_MEM_FLASH_CS

/* ==========================================================================
 * CAN (MCP2515 breakout)
 * ========================================================================== */

#define CAN_INT_PIN             27      /* P0.27 - CAN interrupt */

/* ==========================================================================
 * LORA (RFM95C breakout)
 * ========================================================================== */

#define LORA_DIO0_PIN           29      /* P0.29 - TX/RX done interrupt */
#define LORA_RESET_PIN          28      /* P0.28 - LoRa reset */

/* ==========================================================================
 * I2C (RTC - not connected for Feather testing)
 * ========================================================================== */

#define I2C_SDA_PIN             2       /* P0.02 - I2C SDA (unused) */
#define I2C_SCL_PIN             3       /* P0.03 - I2C SCL (unused) */

/* ==========================================================================
 * POWER MANAGEMENT
 * For Feather testing, tie high or use button to simulate power fail
 * ========================================================================== */

#define POWER_FAIL_PIN          30      /* P0.30/A6 - Power fail (tie to 3V3 for normal) */

/* ==========================================================================
 * STATUS LEDS
 * Using Feather onboard LED only
 * ========================================================================== */

#define LED_3V3_PIN             17      /* Feather onboard LED - 3.3V indicator */
#define LED_24V_PIN             17      /* Same as 3V3 for Feather (no separate LED) */
#define LED_STATUS_PIN          17      /* Feather onboard LED */

/* ==========================================================================
 * BUTTON
 * ========================================================================== */

#define PAIRING_BUTTON_PIN      7       /* P0.07/D7 - Pairing button */

/* ==========================================================================
 * TASK CONFIGURATION
 * ========================================================================== */

#define TASK_STACK_CAN          256
#define TASK_STACK_LORA         512
#define TASK_STACK_SCHEDULE     256
#define TASK_STACK_BLE          256
#define TASK_STACK_LED          128

#define TASK_PRIORITY_CAN       5
#define TASK_PRIORITY_LORA      4
#define TASK_PRIORITY_SCHEDULE  3
#define TASK_PRIORITY_BLE       2
#define TASK_PRIORITY_LED       1

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define LORA_FREQUENCY          915000000
#define LORA_TX_POWER           20
#define LORA_SPREADING_FACTOR   7
#define LORA_BANDWIDTH          125000
#define LORA_CODING_RATE        5
#define LORA_SYNC_WORD          0x34

/* ==========================================================================
 * TIMING CONFIGURATION
 * Shorter intervals for testing
 * ========================================================================== */

#define STATUS_REPORT_INTERVAL_MS   10000   /* 10 seconds for testing */
#define SCHEDULE_PULL_INTERVAL_MS   30000   /* 30 seconds for testing */
#define BLE_PAIRING_TIMEOUT_MS      120000
#define PAIRING_BUTTON_HOLD_MS      3000
#define POWER_FAIL_DEBOUNCE_MS      50

/* ==========================================================================
 * FEATHER WIRING GUIDE
 * ==========================================================================
 * 
 * Adafruit Feather nRF52832 Connections:
 * 
 * CAN (MCP2515 breakout):
 *   Feather SCK  (P0.14) -> MCP2515 SCK
 *   Feather MOSI (P0.13) -> MCP2515 SI
 *   Feather MISO (P0.12) -> MCP2515 SO
 *   Feather D11  (P0.11) -> MCP2515 CS
 *   Feather D27  (P0.27) -> MCP2515 INT
 *   Feather 3V3          -> MCP2515 VCC
 *   Feather GND          -> MCP2515 GND
 * 
 * LoRa (RFM95C breakout):
 *   Feather SCK  (P0.14) -> RFM95 SCK (shared with CAN)
 *   Feather MOSI (P0.13) -> RFM95 MOSI (shared with CAN)
 *   Feather MISO (P0.12) -> RFM95 MISO (shared with CAN)
 *   Feather D10  (P0.10) -> RFM95 CS
 *   Feather D29  (P0.29) -> RFM95 DIO0 (G0)
 *   Feather D28  (P0.28) -> RFM95 RST
 *   Feather 3V3          -> RFM95 VIN
 *   Feather GND          -> RFM95 GND
 * 
 * FRAM (MB85RS1MT breakout):
 *   Feather SCL  (P0.26) -> FRAM SCK
 *   Feather SDA  (P0.25) -> FRAM MOSI (SI)
 *   Feather D24  (P0.24) -> FRAM MISO (SO)
 *   Feather D23  (P0.23) -> FRAM CS
 *   Feather 3V3          -> FRAM VCC
 *   Feather GND          -> FRAM GND
 *   FRAM WP              -> 3V3
 *   FRAM HOLD            -> 3V3
 * 
 * Flash (W25Q16 breakout):
 *   Feather SCL  (P0.26) -> Flash CLK
 *   Feather SDA  (P0.25) -> Flash DI
 *   Feather D24  (P0.24) -> Flash DO
 *   Feather D15  (P0.15) -> Flash CS
 *   Feather 3V3          -> Flash VCC
 *   Feather GND          -> Flash GND
 * 
 * Button:
 *   Feather D7   (P0.07) -> Momentary button to GND
 * 
 * Power Fail (optional):
 *   Feather A6   (P0.30) -> 3V3 (normal) or button to GND (simulate fail)
 * 
 * ========================================================================== */

#endif /* BOARD_CONFIG_FEATHER_H */
