/**
 * @file agsys_config.h
 * @brief Configuration for Water Meter FreeRTOS (nRF52840)
 * 
 * Mag Meter with LVGL display, 5-button navigation, LoRa reporting
 */

#ifndef AGSYS_CONFIG_H
#define AGSYS_CONFIG_H

/* ==========================================================================
 * DEVICE CONFIGURATION
 * ========================================================================== */

#define AGSYS_DEVICE_TYPE           0x04  /* Water Meter */

/* Firmware version */
#define AGSYS_FW_VERSION_MAJOR      1
#define AGSYS_FW_VERSION_MINOR      0
#define AGSYS_FW_VERSION_PATCH      0

/* ==========================================================================
 * DEBUG CONFIGURATION
 * ========================================================================== */

#define AGSYS_DEBUG_ENABLED         1
#define AGSYS_USE_NRF_LOG           0
#define AGSYS_USE_RTT               1

/* ==========================================================================
 * SPI CONFIGURATION (shared bus: ADC, Display, LoRa, FRAM)
 * ========================================================================== */

#define AGSYS_SPI_INSTANCE          0
#define AGSYS_SPI_SCK_PIN           13
#define AGSYS_SPI_MOSI_PIN          14
#define AGSYS_SPI_MISO_PIN          15
#define AGSYS_SPI_DEFAULT_FREQ      NRF_SPIM_FREQ_8M

/* ==========================================================================
 * ADC CONFIGURATION (ADS131M02)
 * ========================================================================== */

#define AGSYS_ADC_CS_PIN            16
#define AGSYS_ADC_DRDY_PIN          17
#define AGSYS_ADC_SYNC_PIN          18  /* SYNC/RESET pin */

/* ==========================================================================
 * DISPLAY CONFIGURATION (ST7789 2.8" TFT)
 * ========================================================================== */

#define AGSYS_DISPLAY_CS_PIN        19
#define AGSYS_DISPLAY_DC_PIN        20
#define AGSYS_DISPLAY_RST_PIN       21
#define AGSYS_DISPLAY_BL_PIN        22
#define AGSYS_DISPLAY_WIDTH         320
#define AGSYS_DISPLAY_HEIGHT        240

/* ==========================================================================
 * FRAM CONFIGURATION
 * ========================================================================== */

#define AGSYS_FRAM_CS_PIN           23

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define AGSYS_LORA_CS_PIN           24
#define AGSYS_LORA_RST_PIN          25
#define AGSYS_LORA_DIO0_PIN         26
#define AGSYS_LORA_FREQUENCY        915000000
#define AGSYS_LORA_SPREADING_FACTOR 7
#define AGSYS_LORA_BANDWIDTH        125000
#define AGSYS_LORA_TX_POWER         17

/* ==========================================================================
 * BUTTON CONFIGURATION (5 buttons)
 * ========================================================================== */

#define AGSYS_BTN_UP_PIN            27
#define AGSYS_BTN_DOWN_PIN          28
#define AGSYS_BTN_LEFT_PIN          29
#define AGSYS_BTN_RIGHT_PIN         30
#define AGSYS_BTN_SELECT_PIN        31

/* Button timing */
#define AGSYS_BTN_DEBOUNCE_MS       50
#define AGSYS_BTN_LONG_PRESS_MS     1000
#define AGSYS_BTN_REPEAT_MS         200

/* ==========================================================================
 * COIL DRIVER CONFIGURATION
 * ========================================================================== */

#define AGSYS_COIL_GATE_PIN         NRF_GPIO_PIN_MAP(1, 0)  /* P1.00 - MOSFET gate */
#define AGSYS_TIER_ID_PIN           NRF_GPIO_PIN_MAP(1, 1)  /* P1.01 - Tier ID ADC */

/* ==========================================================================
 * TEMPERATURE SENSOR CONFIGURATION
 * ========================================================================== */

/* Board temperature - NTC thermistor on ADC */
#define AGSYS_TEMP_BOARD_PIN        NRF_GPIO_PIN_MAP(0, 29) /* P0.29/AIN5 - NTC divider */
#define AGSYS_TEMP_NTC_B_VALUE      3380                    /* NTC B-constant */
#define AGSYS_TEMP_NTC_R25          10000                   /* NTC resistance at 25°C */
#define AGSYS_TEMP_REF_R            10000                   /* Reference resistor */

/* Pipe/coil temperature - TMP102 on I2C */
#define AGSYS_TEMP_I2C_SDA_PIN      NRF_GPIO_PIN_MAP(0, 6)  /* P0.06 - I2C SDA */
#define AGSYS_TEMP_I2C_SCL_PIN      NRF_GPIO_PIN_MAP(0, 7)  /* P0.07 - I2C SCL */
#define AGSYS_TEMP_TMP102_ADDR      0x48                    /* TMP102 I2C address (ADD0=GND) */

/* ==========================================================================
 * BLE CONFIGURATION
 * ========================================================================== */

#define AGSYS_BLE_NAME_PREFIX       "AgMeter-"
#define AGSYS_BLE_ADV_INTERVAL_MS   1000

/* ==========================================================================
 * FREERTOS TASK CONFIGURATION
 * ========================================================================== */

#define AGSYS_TASK_STACK_DEFAULT    256
#define AGSYS_TASK_STACK_BLE        256
#define AGSYS_TASK_STACK_LORA       512
#define AGSYS_TASK_STACK_DISPLAY    1024    /* LVGL needs more */
#define AGSYS_TASK_STACK_ADC        512
#define AGSYS_TASK_STACK_BUTTON     256

#define AGSYS_TASK_PRIORITY_IDLE    1
#define AGSYS_TASK_PRIORITY_LOW     2
#define AGSYS_TASK_PRIORITY_NORMAL  3
#define AGSYS_TASK_PRIORITY_HIGH    4
#define AGSYS_TASK_PRIORITY_REALTIME 5

/* ==========================================================================
 * DISPLAY POWER MANAGEMENT
 * ========================================================================== */

#define AGSYS_DISPLAY_DIM_TIMEOUT_SEC       60
#define AGSYS_DISPLAY_SLEEP_TIMEOUT_SEC     30
#define AGSYS_DISPLAY_MENU_TIMEOUT_SEC      60

/* ==========================================================================
 * DISPLAY AND LORA REPORTING INTERVALS
 * ========================================================================== */

/* Display update interval (seconds) - how often flow values refresh on screen */
#define AGSYS_DISPLAY_UPDATE_SEC_DEFAULT    15

/* LoRa report multiplier - report sent every (display_update * multiplier) seconds
 * Default: 4 → report every 60 seconds (15s × 4)
 * Range: 1-10
 */
#define AGSYS_LORA_REPORT_MULT_DEFAULT      4

/* ==========================================================================
 * METER TIERS (pipe sizes)
 * ========================================================================== */

#define TIER_MM_S   0   /* 1.5" - 2" pipe */
#define TIER_MM_M   1   /* 2.5" - 3" pipe */
#define TIER_MM_L   2   /* 4" pipe */

#endif /* AGSYS_CONFIG_H */
