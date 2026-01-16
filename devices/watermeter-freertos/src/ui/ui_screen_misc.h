/**
 * @file ui_screen_misc.h
 * @brief Miscellaneous screens (splash, about, totalizer, error)
 */

#ifndef UI_SCREEN_MISC_H
#define UI_SCREEN_MISC_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create miscellaneous screen objects
 */
void ui_misc_create(void);

/**
 * @brief Show splash screen
 */
void ui_misc_show_splash(void);

/**
 * @brief Show about screen
 * @param version Firmware version string
 * @param build_date Build date string
 */
void ui_misc_show_about(const char *version, const char *build_date);

/**
 * @brief Show totalizer screen
 * @param total_liters Current total volume
 */
void ui_misc_show_totalizer(float total_liters);

/**
 * @brief Update totalizer value
 * @param total_liters Current total volume
 */
void ui_misc_update_totalizer(float total_liters);

/**
 * @brief Show error screen
 * @param message Error message
 */
void ui_misc_show_error(const char *message);

/**
 * @brief Handle button input on misc screens
 * @param event Button event
 * @return Next screen to show
 */
ScreenId_t ui_misc_handle_button(ButtonEvent_t event);

/**
 * @brief Set totalizer reset callback
 */
typedef void (*totalizer_reset_cb_t)(void);
void ui_misc_set_totalizer_callback(totalizer_reset_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_MISC_H */
