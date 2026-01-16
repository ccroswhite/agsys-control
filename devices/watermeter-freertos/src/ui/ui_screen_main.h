/**
 * @file ui_screen_main.h
 * @brief Main flow display screen for water meter
 */

#ifndef UI_SCREEN_MAIN_H
#define UI_SCREEN_MAIN_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create main screen objects
 */
void ui_main_create(void);

/**
 * @brief Show main screen
 */
void ui_main_show(void);

/**
 * @brief Update flow values on main screen
 * @param data Flow data structure
 */
void ui_main_update(const FlowData_t *data);

/**
 * @brief Handle button input on main screen
 * @param event Button event
 * @return true if event was handled
 */
bool ui_main_handle_button(ButtonEvent_t event);

/**
 * @brief Update status bar on main screen
 * @param lora_connected LoRa connection status
 * @param has_alarm Active alarm flag
 * @param alarm_type Type of active alarm
 * @param last_report_sec Seconds since last LoRa report
 */
void ui_main_update_status_bar(bool lora_connected, bool has_alarm,
                                AlarmType_t alarm_type, uint32_t last_report_sec);

/**
 * @brief Get main screen object
 */
lv_obj_t *ui_main_get_screen(void);

/**
 * @brief Set user settings reference
 * @param settings Pointer to settings structure
 */
void ui_main_set_settings(UserSettings_t *settings);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_MAIN_H */
