/**
 * @file config.h
 * @brief Configuration settings for the Soil Moisture Sensor IoT device
 * 
 * Target: Nordic nRF52832 Microcontroller (Arduino Framework)
 * LoRa Module: HOPERF RFM95C
 * BLE: Built-in (for OTA firmware updates)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ==========================================================================
 * DEVICE IDENTIFICATION
 * ========================================================================== */
#define DEVICE_TYPE                 0x01    // Soil Moisture Sensor
#define FIRMWARE_VERSION_MAJOR      1
#define FIRMWARE_VERSION_MINOR      1
#define FIRMWARE_VERSION_PATCH      0

/* ==========================================================================
 * TIMING CONFIGURATION
 * ========================================================================== */
#define SLEEP_INTERVAL_HOURS        2       // Wake interval in hours
#define SLEEP_INTERVAL_MS           (SLEEP_INTERVAL_HOURS * 3600UL * 1000UL)

// Timeouts in milliseconds
#define LORA_TX_TIMEOUT_MS          5000
#define LORA_RX_TIMEOUT_MS          3000
#define SENSOR_STABILIZE_MS         100

// BLE OTA window
#define BLE_OTA_WINDOW_MS           300000  // 5 minutes after button press

/* ==========================================================================
 * PIN ASSIGNMENTS
 * Configured for Adafruit Feather nRF52832
 * Directly wire RFM95C LoRa module to SPI pins
 * ========================================================================== */

// LoRa Module (RFM95C) - External SPI module
#define PIN_LORA_CS                 27      // A3 on Feather nRF52
#define PIN_LORA_RST                30      // A6 on Feather nRF52
#define PIN_LORA_DIO0               31      // A7 on Feather nRF52 (interrupt)

// NVRAM (SPI FRAM) - External FM25V02 8KB
#define PIN_NVRAM_CS                11      // GPIO 11

// SPI NOR Flash - External W25Q16 2MB (for firmware backup/rollback)
#define PIN_FLASH_CS                12      // GPIO 12

// Soil Moisture Sensor - Discrete H-Bridge Capacitance Measurement
// Uses 100kHz bipolar AC signal via MOSFET H-bridge
// H-bridge: 2× SSM6P15FU (P-ch) + 2× 2SK2009 (N-ch) + BAT54S flyback diodes
// Drive: nRF52832 Timer2 + PPI + GPIOTE (hardware, zero CPU)
// Detection: Envelope detector → ADC
#define PIN_HBRIDGE_A               14      // H-bridge drive A (Timer2+PPI)
#define PIN_HBRIDGE_B               15      // H-bridge drive B (complementary)
#define PIN_MOISTURE_POWER          16      // H-bridge power enable
#define PIN_MOISTURE_ADC            2       // AIN0 - envelope detector output

// H-Bridge Configuration
#define HBRIDGE_FREQUENCY_HZ        100000  // 100 kHz AC excitation
#define MOISTURE_MEASUREMENT_MS     1000    // 1 second measurement window
#define ADC_SAMPLES_PER_MEASUREMENT 1000    // Samples to average per reading

// Battery Voltage Monitoring
// nRF52 has built-in VBAT measurement via internal divider
#define PIN_BATTERY_ANALOG          A6      // VBAT/2 on Feather nRF52 (P0.30)

// Status LEDs
#define PIN_LED_STATUS              17      // Green LED - system status (heartbeat)
#define PIN_LED_SPI                 19      // Yellow LED - SPI activity indicator
#define PIN_LED_CONN                20      // Blue LED - BLE connection indicator

// OTA Update Button
#define PIN_OTA_BUTTON              7       // User button to enable BLE OTA mode

/* ==========================================================================
 * VOLTAGE CONFIGURATION
 * ========================================================================== */
// Operating voltage - 2.5V for ultra-low power operation
// nRF52832 supports 1.7V-3.6V, runs at 64MHz across full range
// 2.5V reduces power consumption by ~25% vs 3.3V
// Note: Ensure all peripherals support 2.5V:
//   - RFM95 LoRa: 1.8V-3.7V ✓
//   - FRAM FM25V02: 2.0V-3.6V ✓
//   - H-bridge MOSFETs: SSM6P15FU/2SK2009 support 2.5V ✓
#define OPERATING_VOLTAGE_MV        2500

/* ==========================================================================
 * ADC CONFIGURATION
 * ========================================================================== */
#define ADC_RESOLUTION_BITS         12
#define ADC_MAX_VALUE               ((1 << ADC_RESOLUTION_BITS) - 1)
#define ADC_REFERENCE_MV            OPERATING_VOLTAGE_MV  // VDDANA reference

// Battery voltage divider
// Feather nRF52 has built-in 2:1 divider on VBAT pin
#define BATTERY_DIVIDER_RATIO       2       // Voltage divider ratio
#define BATTERY_LOW_THRESHOLD_MV    3400    // Low battery warning (~50%)
#define BATTERY_CRITICAL_MV         3200    // Critical - extend sleep (~20%)

// Soil moisture calibration for H-bridge capacitance sensor
// ADC reads envelope detector output (0-4095 for 12-bit)
// WARNING: These are UNCALIBRATED PLACEHOLDERS - you MUST measure actual values!
// Calibration procedure:
//   1. Enable DEBUG_MODE and connect serial monitor
//   2. Hold probe in dry air, record ADC value -> MOISTURE_DRY_VALUE
//   3. Submerge probe in water, record ADC value -> MOISTURE_WET_VALUE
#define MOISTURE_DRY_VALUE          0       // TODO: Calibrate - ADC reading when dry
#define MOISTURE_WET_VALUE          0       // TODO: Calibrate - ADC reading when wet

// Compile-time check to ensure calibration values are set
#if MOISTURE_DRY_VALUE == 0 || MOISTURE_WET_VALUE == 0
    #warning "MOISTURE_DRY_VALUE and MOISTURE_WET_VALUE must be calibrated for your hardware!"
#endif

/* ==========================================================================
 * LORA CONFIGURATION (RFM95C - 915 MHz ISM Band)
 * Optimized for: Long range + High device density
 * ========================================================================== */
// Radio parameters
#define LORA_BANDWIDTH              125E3   // 125 kHz (narrowest = longest range)
#define LORA_SPREADING_FACTOR       10      // SF10 - long range, reasonable airtime
#define LORA_CODING_RATE            5       // 4/5 (good error correction, fast)
#define LORA_PREAMBLE_LENGTH        8
#define LORA_TX_POWER_DBM           20      // +20 dBm (max power for range)
#define LORA_SYNC_WORD              0x34    // Private network sync word
#define LORA_MAX_PAYLOAD_SIZE       64

// Calculated airtime for 32-byte payload @ SF10/125kHz: ~370ms
// Max packets/hour (1% duty cycle): ~97 packets
// Estimated range: 5-10 km line-of-sight, 1-3 km with obstructions

/* --------------------------------------------------------------------------
 * Channel Hopping (US915 uplink band)
 * 64 channels, 125kHz spacing, 902.3 - 914.9 MHz
 * Channel selected via hardware TRNG for true randomness
 * -------------------------------------------------------------------------- */
#define LORA_BASE_FREQ_HZ           902300000   // 902.3 MHz (Channel 0)
#define LORA_CHANNEL_STEP_HZ        200000      // 200 kHz spacing
#define LORA_NUM_CHANNELS           64          // Channels 0-63
#define LORA_USE_CHANNEL_HOPPING    1           // 1 = enabled, 0 = fixed 915 MHz

// Calculate channel frequency: LORA_BASE_FREQ_HZ + (channel * LORA_CHANNEL_STEP_HZ)
// Channel 0  = 902.3 MHz
// Channel 32 = 908.7 MHz
// Channel 63 = 914.9 MHz

/* --------------------------------------------------------------------------
 * ALOHA Protocol with Exponential Backoff
 * Collision avoidance for high device density (100+ sensors)
 * Uses hardware TRNG for random jitter and channel selection
 * -------------------------------------------------------------------------- */
// Initial transmission jitter (spreads first TX attempts)
#define TX_INITIAL_JITTER_MAX_MS    2000        // Random 0-2000ms before first TX

// Exponential backoff on failed transmission (no ACK received)
#define BACKOFF_BASE_MS             1000        // 1 second base backoff
#define BACKOFF_MULTIPLIER          2           // Double each retry (exponential)
#define BACKOFF_MAX_MS              60000       // Cap at 60 seconds
#define BACKOFF_JITTER_PERCENT      50          // Add random 0-50% to backoff

// Retry limits
#define TX_MAX_RETRIES              5           // Give up after 5 failed attempts
#define TX_ACK_TIMEOUT_MS           500         // Wait for ACK before retry

// Backoff sequence: 1s → 2s → 4s → 8s → 16s (+ 0-50% jitter each)
// Worst case total retry time: ~46 seconds

/* ==========================================================================
 * NVRAM CONFIGURATION
 * See nvram_layout.h for detailed memory map
 * ========================================================================== */
#include "nvram_layout.h"

// Legacy compatibility defines (use nvram_layout.h for new code)
#define NVRAM_SIZE_BYTES            NVRAM_TOTAL_SIZE
#define NVRAM_CONFIG_ADDR           NVRAM_USER_CONFIG_ADDR
#define NVRAM_CONFIG_SIZE           NVRAM_USER_CONFIG_SIZE
#define NVRAM_LOG_START_ADDR        NVRAM_LOG_ENTRIES_ADDR
#define NVRAM_LOG_ENTRY_SIZE        16
#define NVRAM_LOG_MAX_ENTRIES       ((NVRAM_LOG_SIZE - NVRAM_LOG_HEADER_SIZE) / NVRAM_LOG_ENTRY_SIZE)

/* ==========================================================================
 * DEVICE IDENTITY (from nRF52832 FICR - factory-programmed)
 * ========================================================================== */
// Device ID is read from chip's FICR registers (64-bit, globally unique)
// Access: NRF_FICR->DEVICEID[0] (lower 32 bits)
//         NRF_FICR->DEVICEID[1] (upper 32 bits)
// This eliminates the need for UUID storage in FRAM.
#define DEVICE_ID_SIZE              8       // 64-bit device ID

/* ==========================================================================
 * FIRMWARE BACKUP ENCRYPTION
 * ========================================================================== */
// Firmware backups in external flash are encrypted with AES-256-CTR
// Key = SHA-256(SECRET_SALT || FICR_DEVICE_ID)
// See firmware_crypto.cpp for SECRET_SALT (CHANGE FOR PRODUCTION!)
#define FW_BACKUP_ENCRYPTED         1       // Enable encrypted backups
#define FW_VALIDATION_TIMEOUT_MS    60000   // 60 seconds to validate new firmware

/* ==========================================================================
 * PROTOCOL CONFIGURATION
 * ========================================================================== */
#define PROTOCOL_VERSION            1
#define PROTOCOL_MAGIC_BYTE1        0x41    // 'A'
#define PROTOCOL_MAGIC_BYTE2        0x47    // 'G'

// Message types
#define MSG_TYPE_SENSOR_REPORT      0x01
#define MSG_TYPE_ACK                0x02
#define MSG_TYPE_CONFIG_REQUEST     0x03
#define MSG_TYPE_CONFIG_RESPONSE    0x04
#define MSG_TYPE_LOG_BATCH          0x05
#define MSG_TYPE_TIME_SYNC          0x06

// Device types
#define DEVICE_TYPE_SOIL_MOISTURE   0x01
#define DEVICE_TYPE_VALVE_CONTROL   0x02
#define DEVICE_TYPE_WATER_METER     0x03

// Retry configuration
#define LORA_MAX_RETRIES            3
#define LORA_RETRY_DELAY_MS         500

/* ==========================================================================
 * CLOCK CONFIGURATION
 * ========================================================================== */
// nRF52832 runs at 64MHz by default
// Low power modes are handled by the SoftDevice and system-on sleep
#define CPU_FREQUENCY_HZ            64000000UL

// SPI clock speeds per device
// Each device uses its own SPISettings for optimal performance
#define SPI_CLOCK_NVRAM_HZ          8000000UL   // FM25V02 FRAM: max 40 MHz, use 8 MHz
#define SPI_CLOCK_FLASH_HZ          16000000UL  // W25Q16 Flash: max 104 MHz, use 16 MHz
#define SPI_CLOCK_LORA_HZ           8000000UL   // RFM95C LoRa: max 10 MHz, use 8 MHz

// Legacy define (for compatibility)
#define SPI_CLOCK_HZ                1000000UL

/* ==========================================================================
 * BLE CONFIGURATION
 * ========================================================================== */
// Device name for BLE advertising (max 20 chars)
#define BLE_DEVICE_NAME             "AgSys-Soil"

// BLE OTA DFU settings
#define BLE_DFU_ENABLED             1       // Enable Nordic DFU service
#define BLE_ADVERTISING_INTERVAL_MS 100     // Fast advertising during OTA window
#define BLE_CONNECTION_INTERVAL_MS  15      // Connection interval for fast transfer

/* ==========================================================================
 * POWER MANAGEMENT
 * ========================================================================== */
// Extended sleep when battery critical
#define CRITICAL_SLEEP_MULTIPLIER   4

// Debug mode - set to 0 for production
#define DEBUG_MODE                  1

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
