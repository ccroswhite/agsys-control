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
 * H-BRIDGE MOTOR CONTROL
 * ========================================================================== */

#define HBRIDGE_A_PIN           3   /* Motor direction A */
#define HBRIDGE_B_PIN           4   /* Motor direction B */
#define HBRIDGE_EN_A_PIN        5   /* Enable A (PWM capable) */
#define HBRIDGE_EN_B_PIN        6   /* Enable B (PWM capable) */

/* ==========================================================================
 * CURRENT SENSE (ADC)
 * ========================================================================== */

#define CURRENT_SENSE_PIN       2
#define CURRENT_SENSE_AIN       NRF_SAADC_INPUT_AIN0

/* ==========================================================================
 * LIMIT SWITCHES
 * ========================================================================== */

#define LIMIT_OPEN_PIN          9
#define LIMIT_CLOSED_PIN        10

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
 * CAN CONFIGURATION
 * ========================================================================== */

#define CAN_BASE_ID             0x100
#define CAN_CMD_OPEN            0x01
#define CAN_CMD_CLOSE           0x02
#define CAN_CMD_STOP            0x03
#define CAN_CMD_EMERGENCY       0x04
#define CAN_CMD_STATUS          0x10
#define CAN_CMD_DISCOVER        0x20

/* Special CAN IDs */
#define CAN_ID_DISCOVER         0x1F0   /* Broadcast discovery - all actuators respond */
#define CAN_ID_EMERGENCY        0x1FF   /* Emergency close all */

/* Discovery response timing */
#define CAN_DISCOVERY_DELAY_MS  5       /* Delay per address to stagger responses */

/* ==========================================================================
 * VALVE CONFIGURATION
 * ========================================================================== */

#define VALVE_TIMEOUT_MS        30000   /* Max time for valve operation */
#define VALVE_OVERCURRENT_MA    2000    /* Overcurrent threshold */
#define VALVE_STALL_CURRENT_MA  500     /* Stall detection threshold */

#endif /* BOARD_CONFIG_H */
