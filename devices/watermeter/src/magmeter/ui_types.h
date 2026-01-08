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
    uint8_t reserved[16];        // Future use
    uint32_t checksum;
} UserSettings_t;

// Default settings
#define DEFAULT_UNIT_SYSTEM         UNIT_SYSTEM_METRIC
#define DEFAULT_TREND_PERIOD_MIN    1
#define DEFAULT_AVG_PERIOD_MIN      30
#define DEFAULT_BACKLIGHT_ON        1

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
    SCREEN_MENU,
    SCREEN_SETTINGS_UNITS,
    SCREEN_SETTINGS_TREND,
    SCREEN_SETTINGS_AVG,
    SCREEN_SETTINGS_MAX_FLOW,
    SCREEN_CALIBRATION,
    SCREEN_CAL_ZERO,
    SCREEN_CAL_SPAN,
    SCREEN_ABOUT
} ScreenId_t;

// Calibration data (stored in FRAM)
typedef struct {
    int32_t zeroOffset;         // ADC zero offset
    float spanFactor;           // Span calibration factor
    float kFactor;              // Flow constant (from factory or field cal)
    uint32_t calDate;           // Unix timestamp of last calibration
    uint32_t checksum;
} CalibrationData_t;

#endif // UI_TYPES_H
