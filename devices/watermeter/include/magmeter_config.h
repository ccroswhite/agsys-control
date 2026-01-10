/**
 * @file magmeter_config.h
 * @brief Configuration for Electromagnetic Flow Meter (Mag Meter)
 * 
 * Hardware: Nordic nRF52840 + RFM95C LoRa + ADS131M02 ADC + ST7789 Display
 * 
 * This device measures water flow using electromagnetic induction with
 * capacitively-coupled electrodes on PVC pipe.
 */

#ifndef MAGMETER_CONFIG_H
#define MAGMETER_CONFIG_H

#include <Arduino.h>

/* ==========================================================================
 * DEVICE IDENTIFICATION
 * ========================================================================== */
#define DEVICE_TYPE                 0x04        // Mag Meter
#define FIRMWARE_VERSION_MAJOR      1
#define FIRMWARE_VERSION_MINOR      0
#define FIRMWARE_VERSION_PATCH      0

/* ==========================================================================
 * PIN ASSIGNMENTS - nRF52840
 * ========================================================================== */

// ADC (ADS131M02) - SPI
#define PIN_ADC_CS                  (22)        // P0.22
#define PIN_ADC_DRDY                (21)        // P0.21 - Interrupt
#define PIN_ADC_SYNC_RST            (20)        // P0.20
#define PIN_ADC_SCLK                (25)        // P0.25
#define PIN_ADC_MOSI                (24)        // P0.24
#define PIN_ADC_MISO                (23)        // P0.23

// Display (ST7789) - SPI
#define PIN_DISP_CS                 (17)        // P0.17
#define PIN_DISP_DC                 (16)        // P0.16
#define PIN_DISP_RST                (15)        // P0.15
#define PIN_DISP_SCLK               (19)        // P0.19
#define PIN_DISP_MOSI               (18)        // P0.18
#define PIN_DISP_BL_EN              (14)        // P0.14 - Backlight enable

// LoRa Module (RFM95C) - SPI
#define PIN_LORA_CS                 (10)        // P0.10
#define PIN_LORA_RST                (9)         // P0.09
#define PIN_LORA_DIO0               (8)         // P0.08 - Interrupt
#define PIN_LORA_SCLK               (13)        // P0.13
#define PIN_LORA_MOSI               (12)        // P0.12
#define PIN_LORA_MISO               (11)        // P0.11

// FRAM (FM25V02) - SPI (shared with LoRa)
#define PIN_FRAM_CS                 (4)         // P0.04

// Coil Drive
#define PIN_COIL_GATE               (32)        // P1.00 - PWM output to power board

// Tier ID (analog input from power board voltage divider)
#define PIN_TIER_ID                 (33)        // P1.01 - ADC input

// Debug/Status
#define PIN_LED_STATUS              (6)         // P0.06

// Navigation Buttons (active LOW with internal pullup)
#define PIN_BTN_UP                  (34)        // P1.02
#define PIN_BTN_DOWN                (35)        // P1.03
#define PIN_BTN_LEFT                (36)        // P1.04
#define PIN_BTN_RIGHT               (37)        // P1.05
#define PIN_BTN_SELECT              (38)        // P1.06
#define BTN_DEBOUNCE_MS             50
#define BTN_LONG_PRESS_MS           2000

// BLE Pairing Mode: Hold UP + DOWN together for 2 seconds
// (SELECT long press enters menu/config mode, so we use a combo for BLE)
#define BLE_PAIRING_COMBO_MS        2000        // Hold time for UP+DOWN combo
#define BLE_PAIRING_TIMEOUT_MS      300000      // 5 minutes pairing window

/* ==========================================================================
 * DISPLAY CONFIGURATION
 * ========================================================================== */
#define DISP_WIDTH                  240
#define DISP_HEIGHT                 320
#define DISP_ROTATION               0           // 0, 1, 2, or 3

/* ==========================================================================
 * ADC CONFIGURATION (ADS131M02)
 * ========================================================================== */
#define ADC_SAMPLE_RATE             1000        // Samples per second
#define ADC_GAIN_ELECTRODE          32          // PGA gain for electrode signal
#define ADC_GAIN_CURRENT            1           // PGA gain for current sense

// ADC channels
#define ADC_CH_ELECTRODE            0           // Channel 0: Electrode signal
#define ADC_CH_CURRENT              1           // Channel 1: Current sense

// Current sensing
#define CURRENT_SENSE_SHUNT_OHMS    0.1f        // Shunt resistor value (100mΩ)

/* ==========================================================================
 * COIL EXCITATION CONFIGURATION
 * ========================================================================== */

// Tier-specific settings (detected via TIER_ID pin)
typedef struct {
    uint16_t voltage_mv;        // Coil voltage (from power board)
    uint16_t frequency_hz;      // Excitation frequency
    uint16_t current_ma;        // Expected coil current
    float    pipe_diameter_mm;  // Pipe inner diameter
    float    k_factor;          // Calibration factor
} MagmeterTier_t;

// Tier definitions (MM-S, MM-M, MM-L)
#define TIER_MM_S                   0           // 1.5" - 2" pipe, 24V
#define TIER_MM_M                   1           // 2.5" - 3" pipe, 48V
#define TIER_MM_L                   2           // 4" pipe, 60V

// Tier ID ADC thresholds (based on voltage divider values)
// MM-S: 3MΩ/1MΩ = 0.825V -> ADC ~1024
// MM-M: 1MΩ/1MΩ = 1.65V  -> ADC ~2048
// MM-L: 1MΩ/3MΩ = 2.475V -> ADC ~3072
#define TIER_ID_THRESHOLD_SM        1536        // Below = MM-S, above = MM-M or MM-L
#define TIER_ID_THRESHOLD_ML        2560        // Below = MM-M, above = MM-L

/* ==========================================================================
 * SIGNAL PROCESSING CONFIGURATION
 * ========================================================================== */

// Synchronous detection parameters
#define SYNC_DETECT_SAMPLES         100         // Samples per half-cycle
#define SYNC_DETECT_SETTLING        10          // Samples to discard after polarity change

// Hardware-synced coil/ADC timing
#define COIL_SETTLING_TIME_US       50          // Microseconds to wait after polarity change
#define SAMPLES_PER_HALF_CYCLE      10          // ADC samples per coil half-cycle

// Averaging
#define FLOW_AVERAGING_SAMPLES      1000        // 1 second of samples
#define FLOW_REPORT_INTERVAL_MS     60000       // Report every minute

// Flow calculation
#define FLOW_MIN_VELOCITY_MPS       0.01f       // Minimum detectable velocity
#define FLOW_MAX_VELOCITY_MPS       10.0f       // Maximum expected velocity

/* ==========================================================================
 * CALIBRATION
 * ========================================================================== */

// Default calibration (will be overwritten from FRAM)
#define CAL_OFFSET_DEFAULT          0           // ADC offset
#define CAL_GAIN_DEFAULT            0x800000    // ADC gain (1.0)
#define CAL_K_FACTOR_DEFAULT        1.0f        // Flow calibration factor

// ADC-level calibration structure (stored in FRAM)
typedef struct {
    int32_t offset_ch0;     // Channel 0 offset calibration
    int32_t offset_ch1;     // Channel 1 offset calibration
    uint32_t gain_ch0;      // Channel 0 gain calibration
    uint32_t gain_ch1;      // Channel 1 gain calibration
    float k_factor;         // Flow calibration factor
    uint32_t checksum;      // Validation checksum
} ADCCalibration_t;

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
 * FRAM MEMORY MAP
 * ========================================================================== */
// FM25V02: 256Kbit = 32KB
#define FRAM_ADDR_CONFIG            0x0000      // Device configuration (256 bytes)
#define FRAM_ADDR_CALIBRATION       0x0100      // Calibration data (256 bytes)
#define FRAM_ADDR_COUNTERS          0x0200      // Flow counters (64 bytes)
#define FRAM_ADDR_NONCE             0x0240      // Crypto nonce (4 bytes)
#define FRAM_ADDR_LOG               0x0300      // Event log (30KB)
#define FRAM_ADDR_END               0x8000      // End of FRAM

/* ==========================================================================
 * TIMING CONFIGURATION
 * ========================================================================== */
#define REPORT_INTERVAL_MS          (60UL * 1000)   // 1 minute
#define DISPLAY_UPDATE_MS           33              // ~30 FPS
#define CALIBRATION_SAVE_MS         (5UL * 60 * 1000)  // 5 minutes

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

#endif // MAGMETER_CONFIG_H
