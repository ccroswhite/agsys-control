/**
 * @file ui_types.h
 * @brief UI type definitions and settings structures for Mag Meter
 */

#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdint.h>

// Unit systems
typedef enum {
    UNIT_SYSTEM_METRIC = 0,      // L, kL, ML
    UNIT_SYSTEM_IMPERIAL,        // gal, kgal, Mgal
    UNIT_SYSTEM_IMPERIAL_AG      // gal, acre-in, acre-ft
} UnitSystem_t;

// Flow rate units
typedef enum {
    FLOW_UNIT_LPM = 0,           // Liters per minute
    FLOW_UNIT_GPM                // Gallons per minute
} FlowUnit_t;

// Volume units (auto-scaled based on magnitude)
typedef enum {
    VOL_UNIT_ML = 0,             // Milliliters
    VOL_UNIT_L,                  // Liters
    VOL_UNIT_KL,                 // Kiloliters
    VOL_UNIT_ML_MEGA,            // Megaliters
    VOL_UNIT_GAL,                // Gallons
    VOL_UNIT_KGAL,               // Thousand gallons
    VOL_UNIT_MGAL,               // Million gallons
    VOL_UNIT_ACRE_IN,            // Acre-inches
    VOL_UNIT_ACRE_FT             // Acre-feet
} VolumeUnit_t;

// User settings (stored in FRAM)
typedef struct {
    UnitSystem_t unitSystem;
    uint16_t trendPeriodMin;     // Trend calculation period (minutes), default 1
    uint16_t avgPeriodMin;       // Average calculation period (minutes), default 30
    float maxFlowLPM;            // Max flow for bar display (L/min)
    uint8_t backlightOn;         // Backlight enabled
    // Menu lock settings
    uint32_t menuPin;            // 6-digit PIN (000000-999999), default 000000
    uint8_t menuLockEnabled;     // If true, menu requires PIN or remote unlock
    uint8_t menuAutoLockMin;     // Auto-lock after N minutes (0=never), default 5
    // LoRa settings
    uint16_t loraReportIntervalSec; // Report interval in seconds (10-300), default 60
    uint8_t loraSpreadingFactor;    // SF7-SF12, default 7
    // Alarm settings
    uint16_t alarmLeakThresholdLPM10; // Leak threshold in 0.1 L/min units (5-100), default 20 (2.0 L/min)
    uint16_t alarmLeakDurationMin;    // Duration before alarm (5-240), default 60
    uint16_t alarmHighFlowLPM;        // High flow threshold L/min (50-500), default 150
    uint8_t reserved[4];         // Future use
    uint32_t checksum;
} UserSettings_t;

// Default settings
#define DEFAULT_UNIT_SYSTEM         UNIT_SYSTEM_METRIC
#define DEFAULT_TREND_PERIOD_MIN    1
#define DEFAULT_AVG_PERIOD_MIN      30
#define DEFAULT_BACKLIGHT_ON        1
#define DEFAULT_MENU_PIN            0       // 000000 (6-digit)
#define DEFAULT_MENU_LOCK_ENABLED   1       // Locked by default
#define DEFAULT_MENU_AUTO_LOCK_MIN  5       // Auto-lock after 5 min inactivity
#define DEFAULT_LORA_REPORT_SEC     60      // Report every 60 seconds
#define DEFAULT_LORA_SF             7       // SF7 (fastest)
#define DEFAULT_ALARM_LEAK_THRESH   20      // 2.0 L/min (in 0.1 L/min units)
#define DEFAULT_ALARM_LEAK_DURATION 60      // 60 minutes
#define DEFAULT_ALARM_HIGH_FLOW     150     // 150 L/min

// Default max flow by tier (L/min)
#define DEFAULT_MAX_FLOW_MM_S       100.0f
#define DEFAULT_MAX_FLOW_MM_M       300.0f
#define DEFAULT_MAX_FLOW_MM_L       800.0f

// Conversion factors
#define LITERS_TO_GALLONS           0.264172f
#define GALLONS_TO_LITERS           3.78541f
#define LITERS_TO_ACRE_FT           0.000000810714f
#define ACRE_FT_TO_LITERS           1233481.84f

// Button events
typedef enum {
    BTN_NONE = 0,
    BTN_UP_SHORT,
    BTN_UP_LONG,
    BTN_DOWN_SHORT,
    BTN_DOWN_LONG,
    BTN_LEFT_SHORT,
    BTN_LEFT_LONG,
    BTN_RIGHT_SHORT,
    BTN_RIGHT_LONG,
    BTN_SELECT_SHORT,
    BTN_SELECT_LONG
} ButtonEvent_t;

// Screen IDs
typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_MENU_LOCKED,      // PIN entry screen
    SCREEN_MENU,             // Main settings menu
    // Display Settings submenu
    SCREEN_DISPLAY_SETTINGS,
    SCREEN_SETTINGS_UNITS,
    SCREEN_SETTINGS_TREND,
    SCREEN_SETTINGS_AVG,
    // Flow Settings submenu
    SCREEN_FLOW_SETTINGS,
    SCREEN_SETTINGS_MAX_FLOW,
    // Alarm Settings submenu
    SCREEN_ALARM_SETTINGS,
    SCREEN_ALARM_LEAK_THRESH,
    SCREEN_ALARM_LEAK_DURATION,
    SCREEN_ALARM_HIGH_FLOW,
    // LoRa Config submenu
    SCREEN_LORA_CONFIG,
    SCREEN_LORA_REPORT_INTERVAL,
    SCREEN_LORA_SPREAD_FACTOR,
    SCREEN_LORA_PING,
    SCREEN_LORA_SET_SECRET,
    // Calibration submenu
    SCREEN_CALIBRATION,
    SCREEN_CAL_ZERO,
    // Totalizer submenu
    SCREEN_TOTALIZER,
    SCREEN_TOTALIZER_RESET,
    // Diagnostics submenu
    SCREEN_DIAGNOSTICS,
    SCREEN_DIAG_LORA,
    SCREEN_DIAG_ADC,
    // Other screens
    SCREEN_ABOUT,
    SCREEN_ALARM              // Alarm overlay (auto-shown)
} ScreenId_t;

// Menu lock state
typedef enum {
    MENU_LOCKED = 0,         // Default - menu access requires PIN or remote unlock
    MENU_UNLOCKED_PIN,       // Unlocked via local PIN entry
    MENU_UNLOCKED_REMOTE     // Unlocked via remote command (auto-locks on timeout)
} MenuLockState_t;

// Display power state
typedef enum {
    DISPLAY_ACTIVE = 0,      // Full brightness, display on
    DISPLAY_DIM,             // Reduced brightness (50%), display on
    DISPLAY_SLEEP            // Display off, backlight off
} DisplayPowerState_t;

// Display timeout defaults (in seconds)
#define DEFAULT_DIM_TIMEOUT_SEC     60      // Active → Dim after 60s
#define DEFAULT_SLEEP_TIMEOUT_SEC   30      // Dim → Sleep after 30s
#define DEFAULT_MENU_TIMEOUT_SEC    60      // Menu → Main (dimmed) after 60s

// Menu lock configuration (stored in FRAM)
typedef struct {
    uint32_t pin;            // 6-digit PIN (000000-999999), default 000000
    bool lockEnabled;        // If false, menu is always accessible
    uint16_t autoLockMinutes; // Auto-lock after N minutes of inactivity (0=never)
} MenuLockConfig_t;

// Alarm types (matches protocol)
typedef enum {
    ALARM_CLEARED = 0,
    ALARM_LEAK,
    ALARM_REVERSE_FLOW,
    ALARM_TAMPER,
    ALARM_HIGH_FLOW
} AlarmType_t;

// LoRa statistics for diagnostics
typedef struct {
    bool connected;
    uint32_t lastTxSec;
    uint32_t lastRxSec;
    uint32_t txCount;
    uint32_t rxCount;
    uint32_t errorCount;
    int16_t rssi;
    float snr;
} LoRaStats_t;

// ADC values for diagnostics
typedef struct {
    int32_t ch1Raw;        // ELEC+ raw
    int32_t ch2Raw;        // ELEC- raw
    int32_t diffRaw;       // Differential
    float temperatureC;
    int32_t zeroOffset;
    float spanFactor;
    float flowRaw;         // Before calibration
    float flowCal;         // After calibration
} ADCValues_t;

// Calibration data (stored in FRAM)
typedef struct {
    int32_t zeroOffset;         // ADC zero offset
    float spanFactor;           // Span calibration factor
    float kFactor;              // Flow constant (from factory or field cal)
    uint32_t calDate;           // Unix timestamp of last calibration
    uint32_t checksum;
} CalibrationData_t;

#endif // UI_TYPES_H
