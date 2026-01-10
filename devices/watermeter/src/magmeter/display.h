/**
 * @file display.h
 * @brief Display interface for Mag Meter using LVGL
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <lvgl.h>
#include "ui_types.h"

// Initialize display hardware and LVGL
void display_init(void);

// Show splash screen during startup
void display_showSplash(void);

// Show main flow display
void display_showMain(void);

// Update flow values on main screen
void display_updateMain(float flowRate_LPM, float totalVolume_L, 
                        float trendVolume_L, float avgVolume_L,
                        bool reverseFlow);

// Show menu screen
void display_showMenu(void);

// Show submenu screens
void display_showDisplaySettings(void);
void display_showFlowSettings(void);
void display_showAlarmSettings(void);

// Show settings screens
void display_showSettingsUnits(void);
void display_showSettingsTrend(void);
void display_showSettingsAvg(void);
void display_showSettingsMaxFlow(void);

// Alarm settings screens
void display_showAlarmLeakThreshold(void);
void display_showAlarmLeakDuration(void);
void display_showAlarmHighFlow(void);

// Show error message
void display_showError(const char* message);

// Show about screen
void display_showAbout(void);

// Show calibration screens
void display_showCalibration(void);
void display_showCalZero(void);
void display_showCalSpan(void);

// Show alarm overlay (replaces total volume section on main screen)
void display_showAlarm(AlarmType_t alarmType, uint32_t durationSec,
                       float flowRateLPM, float volumeLiters);
void display_acknowledgeAlarm(void);
void display_dismissAlarm(void);
bool display_isAlarmActive(void);

// Menu lock functions (uses menuPin/menuLockEnabled from UserSettings_t)
void display_showMenuLocked(void);           // Show PIN entry screen (standalone)
void display_showPinOverlay(void);           // Show PIN overlay on main screen
void display_hidePinOverlay(void);           // Hide PIN overlay
void display_unlockMenuRemote(void);         // Remote unlock (from LoRa command)
void display_lockMenu(void);                 // Force lock menu
bool display_isMenuLocked(void);             // Check if menu is locked

// Display power management
void display_updatePowerState(void);         // Call periodically to update dim/sleep
void display_wake(void);                     // Wake display from sleep/dim
void display_resetActivityTimer(void);       // Reset idle timer on user input
DisplayPowerState_t display_getPowerState(void);  // Get current power state

// Update status bar on main screen
void display_updateStatusBar(bool loraConnected, bool hasAlarm,
                             AlarmType_t alarmType, uint32_t lastReportSec);

// LoRa config screens
void display_showLoRaConfig(void);
void display_showLoRaReportInterval(uint16_t currentValue);
void display_showLoRaSpreadFactor(void);
void display_showLoRaPing(void);
void display_showLoRaPingResult(bool success);
void display_showLoRaSetSecret(void);

// Totalizer screens
void display_showTotalizer(float totalLiters);
void display_showTotalizerReset(float currentTotal);
void display_showTotalizerSet(float currentTotal);
void display_updateResetProgress(uint8_t percent);

// Diagnostics screens
void display_showDiagnostics(void);
void display_showDiagLoRa(LoRaStats_t* stats);
void display_showDiagADC(ADCValues_t* values);

// Handle button input
void display_handleButton(ButtonEvent_t event);

// Get/set current screen
ScreenId_t display_getCurrentScreen(void);

// Set user settings reference
void display_setSettings(UserSettings_t* settings);

// LVGL display flush callback
void display_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

#endif // DISPLAY_H
