/**
 * @file ui_types.h
 * @brief UI type definitions and settings structures for Mag Meter
 * 
 * Ported from Arduino version for FreeRTOS/nRF52840
 */

#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * UNIT SYSTEMS
 * ========================================================================== */

typedef enum {
    UNIT_SYSTEM_METRIC = 0,      // L, kL, ML
    UNIT_SYSTEM_IMPERIAL,        // gal, kgal, Mgal
    UNIT_SYSTEM_IMPERIAL_AG      // gal, acre-in, acre-ft
} UnitSystem_t;

typedef enum {
    FLOW_UNIT_LPM = 0,           // Liters per minute
    FLOW_UNIT_GPM                // Gallons per minute
} FlowUnit_t;

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

/* ==========================================================================
 * USER SETTINGS (stored in FRAM)
 * ========================================================================== */

typedef struct {
    UnitSystem_t unitSystem;
    uint16_t trendPeriodMin;         // Trend calculation period (minutes)
    uint16_t avgPeriodMin;           // Average calculation period (minutes)
    float maxFlowLPM;                // Max flow for bar display (L/min)
    uint8_t backlightOn;             // Backlight enabled
    uint32_t menuPin;                // 6-digit PIN (000000-999999)
    uint8_t menuLockEnabled;         // If true, menu requires PIN
    uint8_t menuAutoLockMin;         // Auto-lock after N minutes
    uint16_t loraReportIntervalSec;  // Report interval in seconds
    uint8_t loraSpreadingFactor;     // SF7-SF12
    uint16_t alarmLeakThresholdLPM10; // Leak threshold in 0.1 L/min units
    uint16_t alarmLeakDurationMin;   // Duration before alarm (minutes)
    uint16_t alarmHighFlowLPM;       // High flow threshold L/min
    uint8_t reserved[4];
    uint32_t checksum;
} UserSettings_t;

/* Default settings */
#define DEFAULT_UNIT_SYSTEM         UNIT_SYSTEM_METRIC
#define DEFAULT_TREND_PERIOD_MIN    1
#define DEFAULT_AVG_PERIOD_MIN      30
#define DEFAULT_BACKLIGHT_ON        1
#define DEFAULT_MENU_PIN            0       // 0000
#define DEFAULT_MENU_LOCK_ENABLED   1
#define DEFAULT_MENU_AUTO_LOCK_MIN  5
#define DEFAULT_LORA_REPORT_SEC     60
#define DEFAULT_LORA_SF             7
#define DEFAULT_ALARM_LEAK_THRESH   20      // 2.0 L/min
#define DEFAULT_ALARM_LEAK_DURATION 60
#define DEFAULT_ALARM_HIGH_FLOW     150

/* Default max flow by tier (L/min) */
#define DEFAULT_MAX_FLOW_MM_S       100.0f
#define DEFAULT_MAX_FLOW_MM_M       300.0f
#define DEFAULT_MAX_FLOW_MM_L       800.0f

/* Conversion factors */
#define LITERS_TO_GALLONS           0.264172f
#define GALLONS_TO_LITERS           3.78541f
#define LITERS_TO_ACRE_FT           0.000000810714f
#define ACRE_FT_TO_LITERS           1233481.84f

/* ==========================================================================
 * BUTTON EVENTS
 * ========================================================================== */

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

/* ==========================================================================
 * SCREEN IDS
 * ========================================================================== */

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_MENU_LOCKED,
    SCREEN_MENU,
    SCREEN_DISPLAY_SETTINGS,
    SCREEN_SETTINGS_UNITS,
    SCREEN_SETTINGS_TREND,
    SCREEN_SETTINGS_AVG,
    SCREEN_FLOW_SETTINGS,
    SCREEN_SETTINGS_MAX_FLOW,
    SCREEN_ALARM_SETTINGS,
    SCREEN_ALARM_LEAK_THRESH,
    SCREEN_ALARM_LEAK_DURATION,
    SCREEN_ALARM_HIGH_FLOW,
    SCREEN_LORA_CONFIG,
    SCREEN_LORA_REPORT_INTERVAL,
    SCREEN_LORA_SPREAD_FACTOR,
    SCREEN_LORA_PING,
    SCREEN_LORA_SET_SECRET,
    SCREEN_CALIBRATION,
    SCREEN_CAL_ZERO,
    SCREEN_TOTALIZER,
    SCREEN_TOTALIZER_RESET,
    SCREEN_DIAGNOSTICS,
    SCREEN_DIAG_LORA,
    SCREEN_DIAG_ADC,
    SCREEN_ABOUT,
    SCREEN_OTA_PROGRESS,
    SCREEN_ALARM
} ScreenId_t;

/* ==========================================================================
 * MENU LOCK STATE
 * ========================================================================== */

typedef enum {
    MENU_LOCKED = 0,
    MENU_UNLOCKED_PIN,
    MENU_UNLOCKED_REMOTE
} MenuLockState_t;

/* ==========================================================================
 * DISPLAY POWER STATE
 * ========================================================================== */

typedef enum {
    DISPLAY_ACTIVE = 0,
    DISPLAY_DIM,
    DISPLAY_SLEEP
} DisplayPowerState_t;

#define DEFAULT_DIM_TIMEOUT_SEC     60
#define DEFAULT_SLEEP_TIMEOUT_SEC   30
#define DEFAULT_MENU_TIMEOUT_SEC    60

/* ==========================================================================
 * ALARM TYPES
 * ========================================================================== */

typedef enum {
    ALARM_CLEARED = 0,
    ALARM_LEAK,
    ALARM_REVERSE_FLOW,
    ALARM_TAMPER,
    ALARM_HIGH_FLOW
} AlarmType_t;

/* ==========================================================================
 * DIAGNOSTIC STRUCTURES
 * ========================================================================== */

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

typedef struct {
    int32_t ch1Raw;
    int32_t ch2Raw;
    int32_t diffRaw;
    float temperatureC;
    int32_t zeroOffset;
    float spanFactor;
    float flowRaw;
    float flowCal;
} ADCValues_t;

/* ==========================================================================
 * CALIBRATION DATA (stored in FRAM)
 * ========================================================================== */

typedef struct {
    int32_t zeroOffset;
    float spanFactor;
    float kFactor;
    uint32_t calDate;
    uint32_t checksum;
} CalibrationData_t;

/* ==========================================================================
 * FLOW DATA (for display updates)
 * ========================================================================== */

typedef struct {
    float flowRateLPM;
    float totalVolumeLiters;
    float trendVolumeLiters;
    float avgVolumeLiters;
    bool reverseFlow;
} FlowData_t;

#endif /* UI_TYPES_H */
