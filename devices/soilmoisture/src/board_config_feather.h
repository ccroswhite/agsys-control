/**
 * @file board_config_feather.h
 * @brief Hardware pin definitions for Adafruit Feather nRF52832 development board
 * 
 * This configuration is for testing the Soil Moisture Sensor firmware on
 * an Adafruit Feather nRF52832 with external breakout boards for:
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
 * - Only 1 probe input configured (use function generator to simulate)
 * - Battery ADC uses Feather's built-in voltage divider on P0.31
 */

#ifndef BOARD_CONFIG_FEATHER_H
#define BOARD_CONFIG_FEATHER_H

#include "nrf_gpio.h"
#include "nrf_saadc.h"

/* Prevent agsys_pins.h from being included - we define our own pins */
#define AGSYS_PINS_H

/* ==========================================================================
 * LED PIN (Feather onboard LED)
 * ========================================================================== */

#define LED_STATUS_PIN              17      /* Feather onboard blue LED (active LOW) */

/* ==========================================================================
 * SPI BUS 0 - LoRa (RFM95C breakout)
 * Using Feather's hardware SPI pins
 * ========================================================================== */

#define SPI_LORA_SCK_PIN            14      /* Feather SCK */
#define SPI_LORA_MOSI_PIN           13      /* Feather MOSI */
#define SPI_LORA_MISO_PIN           12      /* Feather MISO */
#define SPI_CS_LORA_PIN             11      /* D11 - LoRa CS */

/* ==========================================================================
 * SPI BUS 1 - External Memory (FRAM + Flash breakouts)
 * Remapped to avoid Feather conflicts
 * ========================================================================== */

#define AGSYS_MEM_SPI_SCK           NRF_GPIO_PIN_MAP(0, 26)  /* P0.26/SCL */
#define AGSYS_MEM_SPI_MOSI          NRF_GPIO_PIN_MAP(0, 25)  /* P0.25/SDA */
#define AGSYS_MEM_SPI_MISO          NRF_GPIO_PIN_MAP(0, 24)  /* P0.24 */
#define AGSYS_MEM_FRAM_CS           NRF_GPIO_PIN_MAP(0, 23)  /* P0.23 - FRAM CS */
#define AGSYS_MEM_FLASH_CS          NRF_GPIO_PIN_MAP(0, 15)  /* P0.15 - Flash CS (avoid P0.22/FRST!) */

/* Convenience aliases */
#define SPI_CS_FRAM_PIN             AGSYS_MEM_FRAM_CS
#define SPI_CS_FLASH_PIN            AGSYS_MEM_FLASH_CS

/* ==========================================================================
 * LORA (RFM95C breakout)
 * Remapped to avoid Feather conflicts
 * ========================================================================== */

#define LORA_RESET_PIN              29      /* P0.29 - LoRa reset (avoid P0.30 battery conflict) */
#define LORA_DIO0_PIN               27      /* P0.27 - RX/TX done interrupt (avoid P0.31 battery) */

/* ==========================================================================
 * MOISTURE PROBE (Single probe for testing with function generator)
 * ========================================================================== */

#define PROBE_POWER_PIN             16      /* P0.16 - Probe power enable (active LOW) */
#define PROBE_1_FREQ_PIN            3       /* P0.03/A0 - Probe frequency input */

/* Only 1 probe for Feather testing */
#define NUM_MOISTURE_PROBES         1
#define MAX_PROBES                  1

/* Probe measurement timing */
#define PROBE_STABILIZE_MS          10      /* Oscillator stabilization time */
#define PROBE_MEASUREMENT_MS        100     /* Frequency measurement window */

/* ==========================================================================
 * BATTERY MONITORING
 * Uses Feather's built-in voltage divider on P0.31/A7
 * VBAT -> 100K -> P0.31 -> 100K -> GND (divide by 2)
 * ========================================================================== */

#define BATTERY_ADC_PIN             31      /* P0.31/A7 - Feather battery divider */
#define BATTERY_ADC_CHANNEL         NRF_SAADC_INPUT_AIN7

#define BATTERY_DIVIDER_RATIO       2       /* Feather uses 100K/100K divider */
#define BATTERY_LOW_MV              3400    /* Low battery warning */
#define BATTERY_CRITICAL_MV         3200    /* Critical - extend sleep */

/* ==========================================================================
 * BUTTON
 * ========================================================================== */

#define PAIRING_BUTTON_PIN          7       /* P0.07/D7 - Enter BLE pairing mode */
#define PAIRING_BUTTON_HOLD_MS      2000    /* Hold time to enter pairing */

/* ==========================================================================
 * TASK CONFIGURATION
 * ========================================================================== */

#define TASK_STACK_SENSOR           256
#define TASK_STACK_LORA             512
#define TASK_STACK_LED              128

#define TASK_PRIORITY_SENSOR        3
#define TASK_PRIORITY_LORA          2
#define TASK_PRIORITY_LED           1

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define LORA_FREQUENCY              915000000   /* 915 MHz (US ISM) */
#define LORA_TX_POWER               20          /* dBm */
#define LORA_SPREADING_FACTOR       10          /* SF10 for long range */
#define LORA_BANDWIDTH              125000      /* 125 kHz */
#define LORA_SYNC_WORD              0x34        /* AgSys private network */

/* Channel hopping (US915 uplink band) */
#define LORA_BASE_FREQ              902300000   /* 902.3 MHz */
#define LORA_CHANNEL_STEP           200000      /* 200 kHz spacing */
#define LORA_NUM_CHANNELS           64

/* ==========================================================================
 * TIMING CONFIGURATION
 * Shorter intervals for testing
 * ========================================================================== */

#define SLEEP_INTERVAL_HOURS        0
#define SLEEP_INTERVAL_MS           30000       /* 30 seconds for testing (not 2 hours) */

#define LORA_TX_TIMEOUT_MS          5000
#define LORA_RX_TIMEOUT_MS          3000
#define LORA_ACK_TIMEOUT_MS         500
#define LORA_MAX_RETRIES            3

#define BLE_PAIRING_TIMEOUT_MS      300000      /* 5 minutes */

/* ==========================================================================
 * CALIBRATION
 * ========================================================================== */

#define FREQ_MIN_VALID_HZ           50000
#define FREQ_MAX_VALID_HZ           5000000

#define CAL_MIN_DURATION_MS         30000
#define CAL_STABILITY_THRESHOLD     0.001f

/* ==========================================================================
 * FRAM ADDRESSES
 * ========================================================================== */

#define FRAM_CAL_ADDR               0x0000
#define FRAM_CAL_SIZE               256
#define FRAM_LOG_ADDR               0x0100
#define FRAM_LOG_SIZE               7936

/* ==========================================================================
 * DEVICE IDENTIFICATION
 * ========================================================================== */

#define DEVICE_TYPE_SOIL_MOISTURE   0x01

/* ==========================================================================
 * FEATHER WIRING GUIDE
 * ==========================================================================
 * 
 * Adafruit Feather nRF52832 Connections:
 * 
 * LoRa (RFM95C breakout):
 *   Feather SCK  (P0.14) -> RFM95 SCK
 *   Feather MOSI (P0.13) -> RFM95 MOSI
 *   Feather MISO (P0.12) -> RFM95 MISO
 *   Feather D11  (P0.11) -> RFM95 CS
 *   Feather D29  (P0.29) -> RFM95 RST
 *   Feather D27  (P0.27) -> RFM95 DIO0 (G0)
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
 *   FRAM WP              -> 3V3 (disable write protect)
 *   FRAM HOLD            -> 3V3 (disable hold)
 * 
 * Flash (W25Q16 breakout):
 *   Feather SCL  (P0.26) -> Flash CLK
 *   Feather SDA  (P0.25) -> Flash DI
 *   Feather D24  (P0.24) -> Flash DO
 *   Feather D15  (P0.15) -> Flash CS
 *   Feather 3V3          -> Flash VCC
 *   Feather GND          -> Flash GND
 *   Flash WP             -> 3V3
 *   Flash HOLD           -> 3V3
 * 
 * Probe (Function Generator):
 *   Feather A0   (P0.03) -> Function generator output (3.3V square wave)
 *   Feather D16  (P0.16) -> Not connected (probe power control)
 *   Feather GND          -> Function generator GND
 * 
 * Button:
 *   Feather D7   (P0.07) -> Momentary button to GND
 *   (Internal pullup enabled)
 * 
 * Battery:
 *   Connect LiPo to Feather JST connector
 *   P0.31/A7 reads battery voltage automatically
 * 
 * ========================================================================== */

#endif /* BOARD_CONFIG_FEATHER_H */
