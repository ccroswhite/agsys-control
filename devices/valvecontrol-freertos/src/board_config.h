/**
 * @file board_config.h
 * @brief Hardware pin definitions for Valve Controller (nRF52832)
 * 
 * The Valve Controller manages up to 64 valve actuators via CAN bus,
 * communicates with the property controller via LoRa, and supports
 * BLE for local configuration.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ==========================================================================
 * SPI PINS (MCP2515 CAN, RFM95C LoRa, FM25V02 FRAM)
 * ========================================================================== */

#define SPI_SCK_PIN             26
#define SPI_MOSI_PIN            27
#define SPI_MISO_PIN            28

/* Chip selects */
#define SPI_CS_CAN_PIN          11
#define SPI_CS_LORA_PIN         12
#define SPI_CS_FRAM_PIN         13
#define SPI_CS_FLASH_PIN        29  /* W25Q16 SPI Flash (optional) */

/* ==========================================================================
 * CAN (MCP2515)
 * ========================================================================== */

#define CAN_INT_PIN             14

/* ==========================================================================
 * LORA (RFM95C)
 * ========================================================================== */

#define LORA_DIO0_PIN           15  /* TX/RX done interrupt */
#define LORA_RESET_PIN          16

/* ==========================================================================
 * I2C (RV-3028 RTC)
 * ========================================================================== */

#define I2C_SDA_PIN             24
#define I2C_SCL_PIN             25

/* ==========================================================================
 * POWER MANAGEMENT
 * ========================================================================== */

#define POWER_FAIL_PIN          17  /* 24V power fail detection (active low) */

/* ==========================================================================
 * STATUS LEDS
 * ========================================================================== */

#define LED_3V3_PIN             18  /* 3.3V power indicator */
#define LED_24V_PIN             19  /* 24V power indicator */
#define LED_STATUS_PIN          20  /* Status/activity LED */

/* ==========================================================================
 * BUTTON
 * ========================================================================== */

#define PAIRING_BUTTON_PIN      30

/* ==========================================================================
 * TASK CONFIGURATION
 * ========================================================================== */

/* Stack sizes (in words, 4 bytes each) */
#define TASK_STACK_CAN          256
#define TASK_STACK_LORA         512
#define TASK_STACK_SCHEDULE     256
#define TASK_STACK_BLE          256
#define TASK_STACK_LED          128

/* Priorities (higher = more important) */
#define TASK_PRIORITY_CAN       5     /* Highest - CAN bus timing */
#define TASK_PRIORITY_LORA      4
#define TASK_PRIORITY_SCHEDULE  3
#define TASK_PRIORITY_BLE       2
#define TASK_PRIORITY_LED       1     /* Lowest */

/* ==========================================================================
 * CAN CONFIGURATION
 * ========================================================================== */

#define CAN_BASE_ID             0x100
#define CAN_WIRE_CMD_OPEN       0x01
#define CAN_WIRE_CMD_CLOSE      0x02
#define CAN_WIRE_CMD_STOP       0x03
#define CAN_WIRE_CMD_EMERGENCY  0x04
#define CAN_WIRE_CMD_STATUS     0x10
#define CAN_WIRE_CMD_DISCOVER   0x20

#define ACTUATOR_ADDR_MIN       1
#define ACTUATOR_ADDR_MAX       64

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define LORA_FREQUENCY          915000000  /* 915 MHz (US) */
#define LORA_TX_POWER           20         /* dBm */
#define LORA_SPREADING_FACTOR   7
#define LORA_BANDWIDTH          125000     /* 125 kHz */
#define LORA_CODING_RATE        5          /* 4/5 */
#define LORA_SYNC_WORD          0x34       /* AgSys private sync word */

/* ==========================================================================
 * TIMING CONFIGURATION
 * ========================================================================== */

#define HEARTBEAT_INTERVAL_MS       30000   /* Query actuators every 30s */
#define STATUS_REPORT_INTERVAL_MS   60000   /* Report to property controller every 60s */
#define SCHEDULE_PULL_INTERVAL_MS   300000  /* Pull schedule updates every 5 min */
#define BLE_PAIRING_TIMEOUT_MS      120000  /* 2 minute pairing window */
#define PAIRING_BUTTON_HOLD_MS      3000    /* 3 second hold to enter pairing */
#define POWER_FAIL_DEBOUNCE_MS      50

#endif /* BOARD_CONFIG_H */
