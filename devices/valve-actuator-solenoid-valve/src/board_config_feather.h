/**
 * @file board_config_feather.h
 * @brief Hardware pin definitions for Adafruit Feather nRF52832 development board
 * 
 * This configuration is for testing the Valve Actuator (Solenoid) firmware on
 * an Adafruit Feather nRF52832 with external breakout boards for:
 * - MCP2515 CAN controller
 * - MB85RS1MT FRAM (128KB)
 * - W25Q16 SPI Flash (2MB)
 * 
 * FEATHER PIN RESTRICTIONS:
 * - P0.20: DFU pin - must be HIGH at boot
 * - P0.22: Factory Reset (FRST) - must be HIGH at boot
 * - P0.31/A7: Hardwired to battery voltage divider
 * 
 * TESTING NOTES:
 * - No actual solenoid/TRIAC - use LED to simulate valve state
 * - No zero-cross detection - simulated as always ready
 * - DIP switches simulated via fixed address (0x01)
 * - Single LED for status, second LED for valve state
 */

#ifndef BOARD_CONFIG_FEATHER_H
#define BOARD_CONFIG_FEATHER_H

#include "nrf_gpio.h"

/* Prevent agsys_pins.h from being included - we define our own pins */
#define AGSYS_PINS_H

/* ==========================================================================
 * LED PINS
 * Using Feather onboard LED + external LED for valve state
 * ========================================================================== */

#define LED_POWER_PIN           17      /* Feather onboard LED - Power indicator */
#define LED_24V_PIN             17      /* Same as power (no separate LED) */
#define LED_STATUS_PIN          17      /* Feather onboard LED */
#define LED_VALVE_OPEN_PIN      16      /* P0.16/D16 - External LED for valve state */

/* ==========================================================================
 * SPI BUS 0 - CAN (MCP2515 breakout)
 * Using Feather's hardware SPI pins
 * ========================================================================== */

#define SPI_CAN_SCK_PIN         14      /* Feather SCK */
#define SPI_CAN_MOSI_PIN        13      /* Feather MOSI */
#define SPI_CAN_MISO_PIN        12      /* Feather MISO */
#define SPI_CS_CAN_PIN          11      /* D11 - CAN CS */

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
 * TRIAC AC SWITCH CONTROL (Simulated for Feather)
 * For testing, SOLENOID_CTRL_PIN drives an LED instead of TRIAC
 * Zero-cross is simulated as always ready
 * ========================================================================== */

#define SOLENOID_CTRL_PIN       3       /* P0.03/A0 - LED simulates solenoid (active HIGH) */
#define ZERO_CROSS_PIN          4       /* P0.04/A1 - Tie to 3V3 or leave floating */

/* ==========================================================================
 * NO/NC CONFIGURATION (Simulated - fixed to NC for testing)
 * ========================================================================== */

#define DIP_NONC_PIN            28      /* P0.28 - Tie to GND for NC, 3V3 for NO */

/* ==========================================================================
 * DIP SWITCHES (Simulated - fixed address for testing)
 * For Feather testing, tie all to GND for address 0x00, or use jumpers
 * ========================================================================== */

#define DIP_1_PIN               29      /* P0.29 - Address bit 0 */
#define DIP_2_PIN               30      /* P0.30 - Address bit 1 */
#define DIP_3_PIN               5       /* P0.05/A2 - Address bit 2 */
#define DIP_4_PIN               6       /* P0.06 - Address bit 3 (if available) */
#define DIP_5_PIN               6       /* Shared - not enough pins */
#define DIP_6_PIN               6       /* Shared - not enough pins */
#define DIP_TERM_PIN            2       /* P0.02 - CAN termination (tie to 3V3 to enable) */

/* ==========================================================================
 * BUTTON
 * ========================================================================== */

#define PAIRING_BUTTON_PIN      7       /* P0.07/D7 - Pairing button */
#define PAIRING_BUTTON_HOLD_MS  3000
#define BLE_PAIRING_TIMEOUT_MS  120000

/* ==========================================================================
 * TASK CONFIGURATION
 * ========================================================================== */

#define TASK_STACK_CAN          256
#define TASK_STACK_VALVE        256
#define TASK_STACK_LED          128

#define TASK_PRIORITY_CAN       4
#define TASK_PRIORITY_VALVE     3
#define TASK_PRIORITY_LED       1

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
 * Solenoid Simulation (LED):
 *   Feather A0   (P0.03) -> LED anode (with 330R to GND)
 * 
 * Valve State LED:
 *   Feather D16  (P0.16) -> LED anode (with 330R to GND)
 * 
 * Button:
 *   Feather D7   (P0.07) -> Momentary button to GND
 * 
 * Address (DIP switches or jumpers):
 *   Feather D29  (P0.29) -> Jumper to GND (bit 0)
 *   Feather A6   (P0.30) -> Jumper to GND (bit 1)
 *   Feather A2   (P0.05) -> Jumper to GND (bit 2)
 *   (Higher bits not available - limited to 8 addresses)
 * 
 * NO/NC Selection:
 *   Feather D28  (P0.28) -> GND for NC, 3V3 for NO
 * 
 * CAN Termination:
 *   Feather D2   (P0.02) -> 3V3 to enable 120R termination
 * 
 * ========================================================================== */

#endif /* BOARD_CONFIG_FEATHER_H */
