/**
 * @file board_config.h
 * @brief Hardware pin definitions for Valve Actuator (nRF52832)
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "agsys_pins.h"  /* Standard memory bus pins */

/* ==========================================================================
 * LED PINS
 * Note: Moved from P0.25-P0.28 to avoid conflict with standard memory bus
 * ========================================================================== */

#define LED_POWER_PIN           7   /* 3.3V Power indicator */
#define LED_24V_PIN             21  /* 24V Present indicator */
#define LED_STATUS_PIN          29  /* Status LED */
#define LED_VALVE_OPEN_PIN      30  /* Valve Open indicator */

/* ==========================================================================
 * SPI BUS 0 - CAN (MCP2515)
 * ========================================================================== */

#define SPI_CAN_SCK_PIN         14
#define SPI_CAN_MOSI_PIN        12
#define SPI_CAN_MISO_PIN        13
#define SPI_CS_CAN_PIN          11

/* ==========================================================================
 * SPI BUS 1 - External Memory (FRAM + Flash) - STANDARD PINS
 * Uses standard pins from agsys_pins.h:
 *   SCK=P0.26, MOSI=P0.25, MISO=P0.24, FRAM_CS=P0.23, FLASH_CS=P0.22
 * ========================================================================== */
/* FRAM and Flash CS pins defined in agsys_pins.h */

/* ==========================================================================
 * CAN (MCP2515)
 * ========================================================================== */

#define CAN_INT_PIN             8

/* ==========================================================================
 * TRIAC AC SWITCH CONTROL
 * Solenoid control via optoisolated TRIAC (MOC3021 + BTA06-600B)
 * ========================================================================== */

#define SOLENOID_CTRL_PIN       3   /* Drives optocoupler LED to trigger TRIAC */
#define ZERO_CROSS_PIN          4   /* AC zero-cross detection input */

/* ==========================================================================
 * NO/NC CONFIGURATION (DIP Switch 7)
 * ========================================================================== */

#define DIP_NONC_PIN            27  /* NO/NC valve type selection */

/* Valve behavior based on NO/NC setting:
 * NO (Normally Open):  Valve open when de-energized, closes when energized
 * NC (Normally Closed): Valve closed when de-energized, opens when energized
 *
 * For "open valve" command:
 *   NO valve: de-energize solenoid (TRIAC off)
 *   NC valve: energize solenoid (TRIAC on)
 *
 * For "close valve" command:
 *   NO valve: energize solenoid (TRIAC on)
 *   NC valve: de-energize solenoid (TRIAC off)
 */

/* ==========================================================================
 * DIP SWITCHES (Device Address)
 * ========================================================================== */

#define DIP_1_PIN               15
#define DIP_2_PIN               16
#define DIP_3_PIN               17
#define DIP_4_PIN               18
#define DIP_5_PIN               19
#define DIP_6_PIN               20
#define DIP_TERM_PIN            28  /* CAN termination switch - moved from P0.24 to avoid memory bus conflict */

/* ==========================================================================
 * BUTTON
 * ========================================================================== */

#define PAIRING_BUTTON_PIN      31
#define PAIRING_BUTTON_HOLD_MS  3000    /* 3 second hold to enter pairing */
#define BLE_PAIRING_TIMEOUT_MS  120000  /* 2 minute pairing window */

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
 * SOLENOID CONFIGURATION
 * ========================================================================== */

/* Solenoid valves don't have position feedback - state is based on energized state */
/* NO valve: energized = closed, de-energized = open */
/* NC valve: energized = open, de-energized = closed */

#endif /* BOARD_CONFIG_H */
