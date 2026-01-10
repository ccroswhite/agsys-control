/**
 * @file settings.cpp
 * @brief Settings manager implementation - FRAM persistence
 */

#include "settings.h"
#include "magmeter_config.h"
#include <Adafruit_FRAM_SPI.h>

// External FRAM instance (defined in main.cpp)
extern Adafruit_FRAM_SPI fram;

// Current settings
static UserSettings_t currentSettings;

// Magic number to validate stored settings
#define SETTINGS_MAGIC 0xAG5E

// FRAM address for settings
#define FRAM_SETTINGS_ADDR FRAM_ADDR_CONFIG

// Calculate checksum
static uint32_t calculateChecksum(UserSettings_t* settings) {
    uint32_t sum = 0;
    uint8_t* data = (uint8_t*)settings;
    // Sum all bytes except the checksum field itself
    for (size_t i = 0; i < sizeof(UserSettings_t) - sizeof(uint32_t); i++) {
        sum += data[i];
    }
    return sum;
}

// Get default max flow based on tier
float settings_getDefaultMaxFlow(uint8_t tier) {
    switch (tier) {
        case TIER_MM_S: return DEFAULT_MAX_FLOW_MM_S;
        case TIER_MM_M: return DEFAULT_MAX_FLOW_MM_M;
        case TIER_MM_L: return DEFAULT_MAX_FLOW_MM_L;
        default: return DEFAULT_MAX_FLOW_MM_S;
    }
}

void settings_init(void) {
    // Try to load from FRAM
    fram.read(FRAM_SETTINGS_ADDR, (uint8_t*)&currentSettings, sizeof(UserSettings_t));
    
    // Validate checksum
    uint32_t expectedChecksum = calculateChecksum(&currentSettings);
    
    if (currentSettings.checksum != expectedChecksum) {
        // Invalid or uninitialized - use defaults
        DEBUG_PRINTLN("Settings checksum invalid, using defaults");
        settings_reset();
    } else {
        DEBUG_PRINTLN("Settings loaded from FRAM");
        DEBUG_PRINTF("  Units: %d\n", currentSettings.unitSystem);
        DEBUG_PRINTF("  Trend: %d min\n", currentSettings.trendPeriodMin);
        DEBUG_PRINTF("  Avg: %d min\n", currentSettings.avgPeriodMin);
        DEBUG_PRINTF("  Max Flow: %.0f L/min\n", currentSettings.maxFlowLPM);
    }
}

UserSettings_t* settings_get(void) {
    return &currentSettings;
}

void settings_save(void) {
    // Update checksum
    currentSettings.checksum = calculateChecksum(&currentSettings);
    
    // Write to FRAM
    fram.write(FRAM_SETTINGS_ADDR, (uint8_t*)&currentSettings, sizeof(UserSettings_t));
    
    DEBUG_PRINTLN("Settings saved to FRAM");
}

void settings_reset(void) {
    currentSettings.unitSystem = DEFAULT_UNIT_SYSTEM;
    currentSettings.trendPeriodMin = DEFAULT_TREND_PERIOD_MIN;
    currentSettings.avgPeriodMin = DEFAULT_AVG_PERIOD_MIN;
    currentSettings.maxFlowLPM = DEFAULT_MAX_FLOW_MM_S;  // Will be updated based on tier
    currentSettings.backlightOn = DEFAULT_BACKLIGHT_ON;
    
    // Menu lock defaults
    currentSettings.menuPin = DEFAULT_MENU_PIN;
    currentSettings.menuLockEnabled = DEFAULT_MENU_LOCK_ENABLED;
    currentSettings.menuAutoLockMin = DEFAULT_MENU_AUTO_LOCK_MIN;
    
    // LoRa defaults
    currentSettings.loraReportIntervalSec = DEFAULT_LORA_REPORT_SEC;
    currentSettings.loraSpreadingFactor = DEFAULT_LORA_SF;
    
    // Alarm defaults
    currentSettings.alarmLeakThresholdLPM10 = DEFAULT_ALARM_LEAK_THRESH;
    currentSettings.alarmLeakDurationMin = DEFAULT_ALARM_LEAK_DURATION;
    currentSettings.alarmHighFlowLPM = DEFAULT_ALARM_HIGH_FLOW;
    
    // Clear reserved bytes
    memset(currentSettings.reserved, 0, sizeof(currentSettings.reserved));
    
    // Calculate and store checksum
    currentSettings.checksum = calculateChecksum(&currentSettings);
    
    DEBUG_PRINTLN("Settings reset to defaults");
}
