/**
 * @file config.h
 * @brief Configuration for Water Meter
 * 
 * Hardware: Nordic nRF52832 + RFM95C LoRa
 * 
 * This device monitors water flow using a pulse-based flow sensor
 * and reports readings to the property controller via LoRa.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ==========================================================================
 * DEVICE IDENTIFICATION
 * ========================================================================== */
#define DEVICE_TYPE                 0x03        // Water Meter
#define FIRMWARE_VERSION_MAJOR      1
#define FIRMWARE_VERSION_MINOR      0
#define FIRMWARE_VERSION_PATCH      0

/* ==========================================================================
 * PIN ASSIGNMENTS - nRF52832
 * ========================================================================== */

// SPI Bus (shared by LoRa, FRAM, Flash)
#define PIN_SPI_SCK                 (14)        // P0.14
#define PIN_SPI_MISO                (13)        // P0.13
#define PIN_SPI_MOSI                (12)        // P0.12

// LoRa Module (RFM95C)
#define PIN_LORA_CS                 (27)        // P0.27
#define PIN_LORA_RST                (30)        // P0.30
#define PIN_LORA_DIO0               (31)        // P0.31 - Interrupt

// FRAM (FM25V02)
#define PIN_FRAM_CS                 (11)        // P0.11

// Flash (W25Q16)
#define PIN_FLASH_CS                (16)        // P0.16

// Flow Sensor Pulse Input
#define PIN_FLOW_PULSE              (7)         // P0.07 - Interrupt capable
#define FLOW_PULSE_ACTIVE_LOW       1           // Pulse is active LOW

// Battery Voltage Monitoring
#define PIN_BATTERY_ANALOG          (A6)        // VBAT/2 on Feather nRF52

// Status LED
#define PIN_LED_STATUS              (17)        // P0.17 - Green LED

// Pairing Button
#define PIN_PAIRING_BUTTON          (6)         // P0.06 - Active LOW

/* ==========================================================================
 * FLOW SENSOR CONFIGURATION
 * ========================================================================== */

// Pulses per liter (calibration value - adjust for your flow sensor)
// Common values: 450 pulses/L for 1/2" sensors, 5880 pulses/L for 3/4"
#define FLOW_PULSES_PER_LITER       450

// Minimum flow rate to consider as "flowing" (liters per minute * 10)
#define FLOW_MIN_RATE_LPM10         5           // 0.5 L/min

// Leak detection: continuous flow for this duration triggers alert
#define LEAK_DETECTION_MINUTES      60

// Reverse flow detection (if sensor supports it)
#define REVERSE_FLOW_DETECTION      0           // 0 = disabled

/* ==========================================================================
 * TIMING CONFIGURATION
 * ========================================================================== */

// Report interval when no flow
#define REPORT_INTERVAL_IDLE_MS     (5UL * 60 * 1000)   // 5 minutes

// Report interval during active flow
#define REPORT_INTERVAL_FLOW_MS     (1UL * 60 * 1000)   // 1 minute

// Sleep interval between pulse checks (low power mode)
#define SLEEP_INTERVAL_MS           1000        // 1 second

// Debounce time for flow pulses
#define PULSE_DEBOUNCE_MS           5

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */
#ifndef LORA_FREQUENCY
#define LORA_FREQUENCY              915E6       // US915 band
#endif
#define LORA_BANDWIDTH              125E3       // 125 kHz
#define LORA_SPREADING_FACTOR       10          // SF10 for range
#define LORA_CODING_RATE            5           // 4/5
#define LORA_TX_POWER               20          // dBm (max for RFM95)
#define LORA_SYNC_WORD              0x34        // Private network

/* ==========================================================================
 * BATTERY CONFIGURATION
 * ========================================================================== */
#define BATTERY_DIVIDER_RATIO       2           // Voltage divider ratio
#define BATTERY_LOW_THRESHOLD_MV    3400        // Low battery warning
#define BATTERY_CRITICAL_MV         3200        // Critical - reduce TX

/* ==========================================================================
 * FRAM MEMORY MAP
 * ========================================================================== */
// FM25V02: 256Kbit = 32KB
#define FRAM_ADDR_CONFIG            0x0000      // Device configuration (256 bytes)
#define FRAM_ADDR_COUNTERS          0x0100      // Pulse/liter counters (64 bytes)
#define FRAM_ADDR_NONCE             0x0140      // Crypto nonce (4 bytes)
#define FRAM_ADDR_LOG               0x0200      // Event log (30KB)
#define FRAM_ADDR_END               0x8000      // End of FRAM

/* ==========================================================================
 * BLE CONFIGURATION
 * ========================================================================== */
#define BLE_DEVICE_NAME             "AgSys-Meter"
#define BLE_PAIRING_TIMEOUT_MS      300000      // 5 minutes
#define PAIRING_BUTTON_HOLD_MS      2000        // 2 second hold

/* ==========================================================================
 * DEBUG CONFIGURATION
 * ========================================================================== */
#if defined(RELEASE_BUILD)
    #define DEBUG_MODE              0
#else
    #define DEBUG_MODE              1
#endif

#if DEBUG_MODE
    #define DEBUG_PRINT(x)          Serial.print(x)
    #define DEBUG_PRINTLN(x)        Serial.println(x)
    #define DEBUG_PRINTF(...)       Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H
