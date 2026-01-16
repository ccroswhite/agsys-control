/**
 * @file ui_screen_settings.h
 * @brief Settings screens for water meter (display, flow, alarm, LoRa)
 */

#ifndef UI_SCREEN_SETTINGS_H
#define UI_SCREEN_SETTINGS_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create settings screen objects
 */
void ui_settings_create(void);

/**
 * @brief Show display settings screen
 */
void ui_settings_show_display(void);

/**
 * @brief Show flow settings screen
 */
void ui_settings_show_flow(void);

/**
 * @brief Show alarm settings screen
 */
void ui_settings_show_alarm(void);

/**
 * @brief Show LoRa config screen
 */
void ui_settings_show_lora(void);

/**
 * @brief Handle button input on settings screens
 * @param event Button event
 * @return Next screen to show
 */
ScreenId_t ui_settings_handle_button(ButtonEvent_t event);

/**
 * @brief Set settings reference for editing
 * @param settings Pointer to user settings
 */
void ui_settings_set_ref(UserSettings_t *settings);

/**
 * @brief Callback when settings are changed
 */
typedef void (*settings_changed_cb_t)(void);
void ui_settings_set_callback(settings_changed_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_SETTINGS_H */
