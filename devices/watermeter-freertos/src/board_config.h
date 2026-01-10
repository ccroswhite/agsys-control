/**
 * @file board_config.h
 * @brief Hardware pin definitions for Water Meter (nRF52840)
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* ==========================================================================
 * SPI PINS (ADS131M02 ADC, RFM95C LoRa, ST7789 Display, FM25V02 FRAM)
 * ========================================================================== */

#define SPI_SCK_PIN             26
#define SPI_MOSI_PIN            27
#define SPI_MISO_PIN            28

/* Chip selects */
#define SPI_CS_ADC_PIN          29  /* ADS131M02 */
#define SPI_CS_LORA_PIN         30  /* RFM95C */
#define SPI_CS_DISPLAY_PIN      31  /* ST7789 */
#define SPI_CS_FRAM_PIN         32  /* FM25V02 */
#define SPI_CS_FLASH_PIN        4   /* W25Q16 SPI Flash */

/* ==========================================================================
 * ADC (ADS131M02)
 * ========================================================================== */

#define ADC_DRDY_PIN            33  /* Data ready interrupt */
#define ADC_SYNC_PIN            34  /* Sync/reset */

/* ==========================================================================
 * LORA (RFM95C)
 * ========================================================================== */

#define LORA_DIO0_PIN           35  /* TX/RX done interrupt */
#define LORA_DIO1_PIN           36  /* CAD done */
#define LORA_RESET_PIN          37

/* ==========================================================================
 * DISPLAY (ST7789 2.8" TFT)
 * ========================================================================== */

#define DISPLAY_DC_PIN          38  /* Data/Command */
#define DISPLAY_RESET_PIN       39
#define DISPLAY_BACKLIGHT_PIN   40

/* ==========================================================================
 * COIL DRIVER (H-Bridge for magnetic field excitation)
 * ========================================================================== */

#define COIL_A_PIN              41  /* Coil driver A */
#define COIL_B_PIN              42  /* Coil driver B */
#define COIL_EN_PIN             43  /* Enable */

/* ==========================================================================
 * BUTTONS
 * ========================================================================== */

#define BUTTON_UP_PIN           11
#define BUTTON_DOWN_PIN         12
#define BUTTON_SELECT_PIN       13
#define BUTTON_BACK_PIN         14

/* ==========================================================================
 * STATUS LEDS
 * ========================================================================== */

#define LED_POWER_PIN           15
#define LED_STATUS_PIN          16
#define LED_LORA_PIN            17

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

#define ADC_SAMPLE_RATE_HZ      1000  /* 1 kHz sampling */
#define ADC_QUEUE_SIZE          1024  /* Sample queue depth */

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
