/**
 * @file display.h
 * @brief Display interface for Mag Meter using LVGL
 * 
 * Ported from Arduino version for FreeRTOS/nRF52840
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_types.h"

/**
 * @brief Initialize display hardware and LVGL
 * @return true on success
 */
bool display_init(void);

/**
 * @brief LVGL tick handler - call from timer ISR or task
 */
void display_tick(void);

/**
 * @brief LVGL task handler - call periodically from display task
 */
void display_task_handler(void);

/**
 * @brief Show splash screen during startup
 */
void display_showSplash(void);

/**
 * @brief Show main flow display
 */
void display_showMain(void);

/**
 * @brief Update flow values on main screen
 * @param data Flow data structure
 */
void display_updateMain(const FlowData_t *data);

/**
 * @brief Show menu screen
 */
void display_showMenu(void);

/**
 * @brief Show PIN entry screen (menu locked)
 */
void display_showMenuLocked(void);

/**
 * @brief Show PIN overlay on main screen
 */
void display_showPinOverlay(void);

/**
 * @brief Hide PIN overlay
 */
void display_hidePinOverlay(void);

/**
 * @brief Show display settings submenu
 */
void display_showDisplaySettings(void);

/**
 * @brief Show flow settings submenu
 */
void display_showFlowSettings(void);

/**
 * @brief Show alarm settings submenu
 */
void display_showAlarmSettings(void);

/**
 * @brief Show LoRa config submenu
 */
void display_showLoRaConfig(void);

/**
 * @brief Show calibration submenu
 */
void display_showCalibration(void);

/**
 * @brief Show zero calibration screen
 */
void display_showCalZero(void);

/**
 * @brief Show span calibration screen (enter reference flow)
 */
void display_showCalSpan(void);

/**
 * @brief Show pipe size selection screen
 */
void display_showCalPipeSize(void);

/**
 * @brief Show duty cycle configuration screen
 */
void display_showCalDutyCycle(void);

/**
 * @brief Show current calibration data
 */
void display_showCalView(void);

/**
 * @brief Show totalizer screen
 * @param totalLiters Current total volume
 */
void display_showTotalizer(float totalLiters);

/**
 * @brief Show diagnostics submenu
 */
void display_showDiagnostics(void);

/**
 * @brief Show about screen
 */
void display_showAbout(void);

/**
 * @brief Show OTA progress screen
 * @param percent Progress 0-100
 * @param status Status message
 * @param version Target version string (e.g., "1.2.3") or NULL
 */
void display_showOTAProgress(uint8_t percent, const char *status, const char *version);

/**
 * @brief Update OTA progress
 * @param percent Progress 0-100
 */
void display_updateOTAProgress(uint8_t percent);

/**
 * @brief Update OTA status message
 * @param status New status message
 */
void display_updateOTAStatus(const char *status);

/**
 * @brief Show OTA error screen with OK button
 * 
 * Displays error message with acknowledge button. If not acknowledged
 * within 60 seconds, automatically returns to main screen.
 * 
 * @param error_msg Error description
 */
void display_showOTAError(const char *error_msg);

/**
 * @brief Check if OTA error screen is active
 * @return true if error screen is displayed
 */
bool display_isOTAErrorActive(void);

/**
 * @brief Tick handler for OTA error timeout
 * 
 * Call periodically. Returns true if timeout expired and screen dismissed.
 */
bool display_tickOTAError(void);

/**
 * @brief Show error message
 * @param message Error message to display
 */
void display_showError(const char *message);

/**
 * @brief Show alarm overlay on main screen
 * @param alarmType Type of alarm
 * @param durationSec Duration of alarm condition
 * @param flowRateLPM Flow rate during alarm
 * @param volumeLiters Volume during alarm
 */
void display_showAlarm(AlarmType_t alarmType, uint32_t durationSec,
                       float flowRateLPM, float volumeLiters);

/**
 * @brief Acknowledge alarm (clears visual alert)
 */
void display_acknowledgeAlarm(void);

/**
 * @brief Dismiss alarm overlay (returns to main)
 */
void display_dismissAlarm(void);

/**
 * @brief Check if alarm overlay is active
 * @return true if alarm is displayed
 */
bool display_isAlarmActive(void);

/**
 * @brief Handle button input
 * @param event Button event
 */
void display_handleButton(ButtonEvent_t event);

/**
 * @brief Get current screen ID
 * @return Current screen
 */
ScreenId_t display_getCurrentScreen(void);

/**
 * @brief Set user settings reference
 * @param settings Pointer to settings structure
 */
void display_setSettings(UserSettings_t *settings);

/**
 * @brief Update display power state (dim/sleep)
 */
void display_updatePowerState(void);

/**
 * @brief Wake display from sleep/dim
 */
void display_wake(void);

/**
 * @brief Reset activity timer on user input
 */
void display_resetActivityTimer(void);

/**
 * @brief Get current power state
 * @return Current power state
 */
DisplayPowerState_t display_getPowerState(void);

/**
 * @brief Check if menu is locked
 * @return true if PIN required
 */
bool display_isMenuLocked(void);

/**
 * @brief Lock menu (require PIN)
 */
void display_lockMenu(void);

/**
 * @brief Unlock menu via remote command
 */
void display_unlockMenuRemote(void);

/**
 * @brief Update status bar on main screen
 * @param loraConnected LoRa connection status
 * @param hasAlarm Active alarm flag
 * @param alarmType Type of active alarm
 * @param lastReportSec Seconds since last LoRa report
 */
void display_updateStatusBar(bool loraConnected, bool hasAlarm,
                             AlarmType_t alarmType, uint32_t lastReportSec);

/**
 * @brief Show LoRa diagnostics
 * @param stats LoRa statistics
 */
void display_showDiagLoRa(const LoRaStats_t *stats);

/**
 * @brief Show ADC diagnostics
 * @param values ADC values
 */
void display_showDiagADC(const ADCValues_t *values);

/**
 * @brief Update BLE status icon
 * 
 * Shows/hides the BLE icon in lower-right corner based on state.
 * Icon flashes at different rates to match LED patterns:
 * - IDLE: hidden
 * - ADVERTISING: slow blink (1Hz)
 * - CONNECTED: fast blink (2Hz)
 * - AUTHENTICATED: solid on
 * - DISCONNECTED: triple flash then hide
 * 
 * @param state Current BLE UI state
 */
void display_updateBleStatus(BleUiState_t state);

/**
 * @brief Get current BLE UI state
 * @return Current BLE state
 */
BleUiState_t display_getBleStatus(void);

/**
 * @brief Tick handler for BLE icon flashing
 * 
 * Call periodically (e.g., from display task) to update flash state.
 */
void display_tickBleIcon(void);

#endif /* DISPLAY_H */
