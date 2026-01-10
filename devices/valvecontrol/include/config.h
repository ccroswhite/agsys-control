/**
 * @file config.h
 * @brief Configuration for Valve Controller
 * 
 * Hardware: Nordic nRF52832 + RFM95C LoRa + MCP2515 CAN
 * 
 * This controller manages up to 64 valve actuators via CAN bus,
 * communicates with the property controller via LoRa, and supports
 * BLE for local configuration.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ==========================================================================
 * DEVICE IDENTIFICATION
 * ========================================================================== */
#define DEVICE_TYPE                 0x02        // Valve Controller
#define FIRMWARE_VERSION_MAJOR      1
#define FIRMWARE_VERSION_MINOR      0
#define FIRMWARE_VERSION_PATCH      0

/* ==========================================================================
 * PIN ASSIGNMENTS - nRF52832
 * ========================================================================== */

// SPI Bus (shared by LoRa, CAN, FRAM, Flash)
// Undefine Adafruit defaults before redefining for our hardware
#ifdef PIN_SPI_SCK
#undef PIN_SPI_SCK
#endif
#ifdef PIN_SPI_MISO
#undef PIN_SPI_MISO
#endif
#ifdef PIN_SPI_MOSI
#undef PIN_SPI_MOSI
#endif
#define PIN_SPI_SCK                 (14)        // P0.14
#define PIN_SPI_MISO                (13)        // P0.13
#define PIN_SPI_MOSI                (12)        // P0.12

// LoRa Module (RFM95C)
#define PIN_LORA_CS                 (27)        // P0.27
#define PIN_LORA_RST                (30)        // P0.30
#define PIN_LORA_DIO0               (31)        // P0.31 - Interrupt

// CAN Bus (MCP2515)
#define PIN_CAN_CS                  (11)        // P0.11
#define PIN_CAN_INT                 (8)         // P0.08 - Interrupt

// FRAM (FM25V02)
#define PIN_FRAM_CS                 (15)        // P0.15

// Flash (W25Q16)
#define PIN_FLASH_CS                (16)        // P0.16

// RTC (RV-3028) - I2C
#define PIN_I2C_SDA                 (25)        // P0.25
#define PIN_I2C_SCL                 (26)        // P0.26
#define RTC_I2C_ADDR                0x52        // RV-3028 address

// Status LEDs
#define PIN_LED_3V3                 (17)        // P0.17 - Green (power)
#define PIN_LED_24V                 (19)        // P0.19 - Yellow (24V present)
#define PIN_LED_STATUS              (20)        // P0.20 - Red (error/status)

// Power Fail Detection (from PSU board)
#define PIN_POWER_FAIL              (7)         // P0.07 - Active LOW when on battery

// Pairing Button
#define PIN_PAIRING_BUTTON          (6)         // P0.06 - Active LOW

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */
#ifndef LORA_FREQUENCY
#define LORA_FREQUENCY              915E6       // US915 band
#endif
#define LORA_BANDWIDTH              125E3       // 125 kHz
#define LORA_SPREADING_FACTOR       9           // SF9
#define LORA_CODING_RATE            5           // 4/5
#define LORA_TX_POWER               20          // dBm (max for RFM95)
#define LORA_SYNC_WORD              0x34        // Private network

/* ==========================================================================
 * CAN BUS CONFIGURATION
 * ========================================================================== */
#define CAN_SPEED                   CAN_1000KBPS // 1 Mbps (short cable runs in enclosure)
#define CAN_CLOCK                   MCP_16MHZ   // MCP2515 crystal

// CAN Message IDs
#define CAN_ID_VALVE_OPEN           0x100       // Controller -> Actuator: Open valve
#define CAN_ID_VALVE_CLOSE          0x101       // Controller -> Actuator: Close valve
#define CAN_ID_VALVE_STOP           0x102       // Controller -> Actuator: Stop motor
#define CAN_ID_VALVE_QUERY          0x103       // Controller -> Actuator: Query status
#define CAN_ID_UID_QUERY            0x104       // Controller -> Actuator: Query UID
#define CAN_ID_DISCOVER_ALL         0x105       // Controller -> All: Discovery broadcast
#define CAN_ID_EMERGENCY_CLOSE      0x1FF       // Controller -> All: Emergency close
#define CAN_ID_STATUS_BASE          0x200       // Actuator -> Controller: Status (0x200 + addr)
#define CAN_ID_UID_RESPONSE_BASE    0x280       // Actuator -> Controller: UID response (0x280 + addr)

// CAN Timing
#define CAN_RESPONSE_TIMEOUT_MS     500         // Max wait for actuator response
#define CAN_RETRY_COUNT             3           // Retries before marking actuator offline

/* ==========================================================================
 * VALVE ACTUATOR LIMITS
 * ========================================================================== */
#define MAX_ACTUATORS               64          // Maximum actuators on CAN bus
#define ACTUATOR_ADDR_MIN           1           // Minimum actuator address
#define ACTUATOR_ADDR_MAX           64          // Maximum actuator address

/* ==========================================================================
 * SCHEDULE CONFIGURATION
 * ========================================================================== */
#define MAX_SCHEDULE_ENTRIES        256         // Max schedules in FRAM
#define SCHEDULE_PULL_INTERVAL_MS   (6UL * 60 * 60 * 1000)  // 6 hours
#define PROCEED_CHECK_TIMEOUT_MS    30000       // 30 seconds to wait for property controller

/* ==========================================================================
 * TIMING CONFIGURATION
 * ========================================================================== */
#define VALVE_OPERATION_TIMEOUT_MS  30000       // Max time for valve to open/close
#define STATUS_REPORT_INTERVAL_MS   60000       // Report status every 60 seconds
#define HEARTBEAT_INTERVAL_MS       10000       // CAN bus heartbeat

/* ==========================================================================
 * BLE CONFIGURATION
 * ========================================================================== */
#define BLE_DEVICE_NAME             "ValveCtrl"
#define BLE_PAIRING_TIMEOUT_MS      300000      // 5 minutes
#define PAIRING_BUTTON_HOLD_MS      2000        // 2 second hold to enter pairing

/* ==========================================================================
 * FRAM MEMORY MAP
 * ========================================================================== */
// FM25V02: 256Kbit = 32KB
#define FRAM_ADDR_CONFIG            0x0000      // Device configuration (256 bytes)
#define FRAM_ADDR_SCHEDULES         0x0100      // Schedule entries (8KB)
#define FRAM_ADDR_ACTUATOR_STATE    0x2100      // Actuator state cache (1KB)
#define FRAM_ADDR_EVENT_LOG         0x2500      // Event log (22KB)
#define FRAM_ADDR_END               0x8000      // End of FRAM

/* ==========================================================================
 * POWER MANAGEMENT
 * ========================================================================== */
#define POWER_FAIL_DEBOUNCE_MS      100         // Debounce power fail signal
#define BATTERY_MODE_POLL_MS        1000        // Poll interval when on battery

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
