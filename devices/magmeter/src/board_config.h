/**
 * @file board_config.h
 * @brief Hardware pin definitions for Water Meter (nRF52840)
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "agsys_pins.h"  /* Standard memory bus pins */

/* ==========================================================================
 * SPI BUS 0 - ADC (ADS131M02) - Dedicated for high-speed sampling
 * Moved to avoid conflict with standard memory bus (P0.22-P0.26)
 * ========================================================================== */

#define SPI0_SCK_PIN            NRF_GPIO_PIN_MAP(0, 5)   /* P0.05 */
#define SPI0_MOSI_PIN           NRF_GPIO_PIN_MAP(0, 4)   /* P0.04 */
#define SPI0_MISO_PIN           NRF_GPIO_PIN_MAP(0, 3)   /* P0.03 */
#define SPI_CS_ADC_PIN          NRF_GPIO_PIN_MAP(0, 2)   /* P0.02 */

/* ==========================================================================
 * SPI BUS 1 - Display (ST7789)
 * ========================================================================== */

#define SPI1_SCK_PIN            NRF_GPIO_PIN_MAP(0, 19)  /* P0.19 */
#define SPI1_MOSI_PIN           NRF_GPIO_PIN_MAP(0, 18)  /* P0.18 */
#define SPI1_MISO_PIN           NRF_GPIO_PIN_MAP(0, 31)  /* Not used, placeholder */
#define SPI_CS_DISPLAY_PIN      NRF_GPIO_PIN_MAP(0, 17)  /* P0.17 */

/* ==========================================================================
 * SPI BUS 2 - LoRa (dedicated)
 * ========================================================================== */

#define SPI2_SCK_PIN            NRF_GPIO_PIN_MAP(0, 13)  /* P0.13 */
#define SPI2_MOSI_PIN           NRF_GPIO_PIN_MAP(0, 12)  /* P0.12 */
#define SPI2_MISO_PIN           NRF_GPIO_PIN_MAP(0, 11)  /* P0.11 */
#define SPI_CS_LORA_PIN         NRF_GPIO_PIN_MAP(0, 10)  /* P0.10 */

/* ==========================================================================
 * SPI BUS 3 - External Memory (FRAM + Flash) - STANDARD PINS
 * Uses standard pins from agsys_pins.h:
 *   SCK=P0.26, MOSI=P0.25, MISO=P0.24, FRAM_CS=P0.23, FLASH_CS=P0.22
 * ========================================================================== */
/* FRAM and Flash CS pins defined in agsys_pins.h */
#define SPI3_SCK_PIN            AGSYS_MEM_SPI_SCK
#define SPI3_MOSI_PIN           AGSYS_MEM_SPI_MOSI
#define SPI3_MISO_PIN           AGSYS_MEM_SPI_MISO

/* ==========================================================================
 * ADC (ADS131M02)
 * Note: P0.21 not available on 48-pin QFAA, using P0.31 for DRDY
 * ========================================================================== */

#define ADC_DRDY_PIN            NRF_GPIO_PIN_MAP(0, 31)  /* P0.31 - Data ready interrupt */
#define ADC_SYNC_PIN            NRF_GPIO_PIN_MAP(0, 20)  /* P0.20 - Sync/reset */

/* ==========================================================================
 * LORA (RFM95C)
 * ========================================================================== */

#define LORA_DIO0_PIN           NRF_GPIO_PIN_MAP(0, 8)   /* P0.08 - TX/RX done interrupt */
#define LORA_RESET_PIN          NRF_GPIO_PIN_MAP(0, 9)   /* P0.09 */

/* ==========================================================================
 * DISPLAY (ST7789 2.8" TFT)
 * Note: P0.16 not available on 48-pin QFAA, using P0.30 for DC
 * ========================================================================== */

#define DISPLAY_DC_PIN          NRF_GPIO_PIN_MAP(0, 30)  /* P0.30 - Data/Command */
#define DISPLAY_RESET_PIN       NRF_GPIO_PIN_MAP(0, 15)  /* P0.15 */
#define DISPLAY_BACKLIGHT_PIN   NRF_GPIO_PIN_MAP(0, 14)  /* P0.14 */

/* ==========================================================================
 * COIL DRIVER (PWM to power board MOSFET)
 * ========================================================================== */

#define COIL_GATE_PIN           NRF_GPIO_PIN_MAP(1, 0)   /* P1.00 - PWM to MOSFET */

/* ==========================================================================
 * BUTTONS (Active LOW with internal pullup)
 * ========================================================================== */

#define BUTTON_UP_PIN           NRF_GPIO_PIN_MAP(1, 2)   /* P1.02 */
#define BUTTON_DOWN_PIN         NRF_GPIO_PIN_MAP(1, 3)   /* P1.03 */
#define BUTTON_LEFT_PIN         NRF_GPIO_PIN_MAP(1, 4)   /* P1.04 */
#define BUTTON_RIGHT_PIN        NRF_GPIO_PIN_MAP(1, 5)   /* P1.05 */
#define BUTTON_SELECT_PIN       NRF_GPIO_PIN_MAP(1, 6)   /* P1.06 */

/* ==========================================================================
 * TIER ID (Analog input for power board tier detection)
 * ========================================================================== */

#define TIER_ID_PIN             NRF_GPIO_PIN_MAP(1, 1)   /* P1.01 - ADC input */

/* ==========================================================================
 * STATUS LEDS (Optional - DNP for production)
 * ========================================================================== */

#define LED_BLE_PIN             NRF_GPIO_PIN_MAP(1, 7)   /* P1.07 - BLE status (green) */
#define LED_LORA_PIN            NRF_GPIO_PIN_MAP(1, 8)   /* P1.08 - LoRa status (blue) */

/* ==========================================================================
 * TASK CONFIGURATION
 * ========================================================================== */

/* Stack sizes (in words, 4 bytes each) */
#define TASK_STACK_ADC          256
#define TASK_STACK_SIGNAL       512
#define TASK_STACK_LORA         512
#define TASK_STACK_DISPLAY      1024  /* LVGL needs more */
#define TASK_STACK_BLE          256
#define TASK_STACK_UI           256

/* Priorities (higher = more important) */
#define TASK_PRIORITY_ADC       6     /* Highest - time critical */
#define TASK_PRIORITY_SIGNAL    5
#define TASK_PRIORITY_LORA      4
#define TASK_PRIORITY_DISPLAY   3
#define TASK_PRIORITY_BLE       2
#define TASK_PRIORITY_UI        1     /* Lowest */

/* ==========================================================================
 * ADC CONFIGURATION
 * ========================================================================== */

#define ADC_SAMPLE_RATE_HZ      4000  /* 4 kSPS - 2x margin for 2 kHz coil */
#define ADC_QUEUE_SIZE          256   /* Sample queue depth */

/* ==========================================================================
 * COIL EXCITATION CONFIGURATION
 * ========================================================================== */

#define COIL_FREQ_SMALL_HZ      1000  /* 1 kHz for 1.5" - 3" pipes */
#define COIL_FREQ_LARGE_HZ      2000  /* 2 kHz for 4" - 6" pipes */

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define LORA_FREQUENCY          915000000  /* 915 MHz (US) */
#define LORA_TX_POWER           20         /* dBm */
#define LORA_SPREADING_FACTOR   7
#define LORA_BANDWIDTH          125000     /* 125 kHz */
#define LORA_REPORT_INTERVAL_S  60         /* Report every 60 seconds */

/* ==========================================================================
 * DISPLAY CONFIGURATION
 * ========================================================================== */

#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          320
#define DISPLAY_FPS             30

/* ==========================================================================
 * FLOW METER CONFIGURATION
 * ========================================================================== */

#define FLOW_CALC_INTERVAL_MS   1000  /* Calculate flow every 1 second */
#define FLOW_REPORT_INTERVAL_S  60    /* Report to LoRa every 60 seconds */

#endif /* BOARD_CONFIG_H */
