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

// Show settings screens
void display_showSettingsUnits(void);
void display_showSettingsTrend(void);
void display_showSettingsAvg(void);
void display_showSettingsMaxFlow(void);

// Show error message
void display_showError(const char* message);

// Show about screen
void display_showAbout(void);

// Show calibration screens
void display_showCalibration(void);
void display_showCalZero(void);
void display_showCalSpan(void);

// Handle button input
void display_handleButton(ButtonEvent_t event);

// Get/set current screen
ScreenId_t display_getCurrentScreen(void);

// Set user settings reference
void display_setSettings(UserSettings_t* settings);

// LVGL display flush callback
void display_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

#endif // DISPLAY_H
