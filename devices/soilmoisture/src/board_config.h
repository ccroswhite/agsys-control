/**
 * @file board_config.h
 * @brief Hardware pin definitions for Soil Moisture Sensor (nRF52832)
 * 
 * Battery-powered sensor with:
 * - 4 capacitive moisture probes (oscillator frequency measurement)
 * - RFM95C LoRa module
 * - MB85RS1MT FRAM (128KB) for logging/calibration
 * - W25Q16 SPI Flash for firmware backup
 * - BLE for pairing/calibration mode
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* Use Feather-specific config if building for Adafruit Feather nRF52832 */
#ifdef USE_FEATHER_BOARD
#include "board_config_feather.h"
#else

#include "agsys_pins.h"  /* Standard memory bus pins */

/* ==========================================================================
 * LED PIN
 * ========================================================================== */

#define LED_STATUS_PIN              17      /* Green status LED (active LOW) */

/* ==========================================================================
 * SPI BUS 0 - LoRa (RFM95C)
 * ========================================================================== */

#define SPI_LORA_SCK_PIN            14
#define SPI_LORA_MOSI_PIN           13
#define SPI_LORA_MISO_PIN           12
#define SPI_CS_LORA_PIN             11

/* ==========================================================================
 * SPI BUS 1 - External Memory (FRAM + Flash) - STANDARD PINS
 * Uses standard pins from agsys_pins.h:
 *   SCK=P0.26, MOSI=P0.25, MISO=P0.24, FRAM_CS=P0.23, FLASH_CS=P0.22
 * ========================================================================== */
/* FRAM and Flash CS pins defined in agsys_pins.h */

/* ==========================================================================
 * LORA (RFM95C)
 * ========================================================================== */

#define LORA_RESET_PIN              30
#define LORA_DIO0_PIN               31      /* RX/TX done interrupt */

/* ==========================================================================
 * MOISTURE PROBES
 * Oscillator frequency measurement - each probe has a relaxation oscillator
 * Frequency varies with soil capacitance: dry = high freq, wet = low freq
 * ========================================================================== */

#define PROBE_POWER_PIN             16      /* P-FET gate (active LOW) */
#define PROBE_1_FREQ_PIN            3       /* Probe 1 frequency input (1 ft) */
#define PROBE_2_FREQ_PIN            4       /* Probe 2 frequency input (3 ft) */
#define PROBE_3_FREQ_PIN            5       /* Probe 3 frequency input (5 ft) */
#define PROBE_4_FREQ_PIN            28      /* Probe 4 frequency input (7 ft) */

#define NUM_MOISTURE_PROBES         4
#define MAX_PROBES                  4

/* Probe measurement timing */
#define PROBE_STABILIZE_MS          10      /* Oscillator stabilization time */
#define PROBE_MEASUREMENT_MS        100     /* Frequency measurement window */

/* ==========================================================================
 * BATTERY MONITORING
 * ========================================================================== */

#define BATTERY_ADC_PIN             30      /* VBAT/2 via internal divider */
#define BATTERY_ADC_CHANNEL         NRF_SAADC_INPUT_AIN6

#define BATTERY_DIVIDER_RATIO       2
#define BATTERY_LOW_MV              3400    /* Low battery warning */
#define BATTERY_CRITICAL_MV         3200    /* Critical - extend sleep */

/* ==========================================================================
 * BUTTON
 * ========================================================================== */

#define PAIRING_BUTTON_PIN          7       /* Enter BLE pairing mode */
#define PAIRING_BUTTON_HOLD_MS      2000    /* Hold time to enter pairing */

/* ==========================================================================
 * TASK CONFIGURATION
 * Battery-powered device - minimal tasks, mostly sleeping
 * ========================================================================== */

#define TASK_STACK_SENSOR           256     /* Sensor reading task */
#define TASK_STACK_LORA             512     /* LoRa communication */
#define TASK_STACK_LED              128     /* LED status */

#define TASK_PRIORITY_SENSOR        3       /* Highest - quick measurement */
#define TASK_PRIORITY_LORA          2       /* Communication */
#define TASK_PRIORITY_LED           1       /* Lowest */

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
 * ========================================================================== */

#define SLEEP_INTERVAL_HOURS        2
#define SLEEP_INTERVAL_MS           (SLEEP_INTERVAL_HOURS * 3600UL * 1000UL)

#define LORA_TX_TIMEOUT_MS          5000
#define LORA_RX_TIMEOUT_MS          3000
#define LORA_ACK_TIMEOUT_MS         500
#define LORA_MAX_RETRIES            3

#define BLE_PAIRING_TIMEOUT_MS      300000  /* 5 minutes */

/* ==========================================================================
 * CALIBRATION
 * ========================================================================== */

/* Expected frequency ranges (sanity check) */
#define FREQ_MIN_VALID_HZ           50000
#define FREQ_MAX_VALID_HZ           5000000

/* Auto-calibration parameters */
#define CAL_MIN_DURATION_MS         30000   /* 30 seconds minimum */
#define CAL_STABILITY_THRESHOLD     0.001f  /* 0.1% relative std dev */

/* ==========================================================================
 * FRAM ADDRESSES
 * ========================================================================== */

#define FRAM_CAL_ADDR               0x0000  /* Calibration data */
#define FRAM_CAL_SIZE               256
#define FRAM_LOG_ADDR               0x0100  /* Data log */
#define FRAM_LOG_SIZE               7936    /* ~7.75 KB for logs */

/* ==========================================================================
 * DEVICE IDENTIFICATION
 * ========================================================================== */

#define DEVICE_TYPE_SOIL_MOISTURE   0x01

#endif /* USE_FEATHER_BOARD */
#endif /* BOARD_CONFIG_H */
