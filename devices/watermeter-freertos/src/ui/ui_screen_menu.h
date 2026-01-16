/**
 * @file ui_screen_menu.h
 * @brief Menu navigation screens for water meter
 */

#ifndef UI_SCREEN_MENU_H
#define UI_SCREEN_MENU_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create menu screen objects
 */
void ui_menu_create(void);

/**
 * @brief Show main menu screen
 */
void ui_menu_show(void);

/**
 * @brief Show submenu screen
 * @param submenu_id Submenu identifier
 */
void ui_menu_show_submenu(uint8_t submenu_id);

/**
 * @brief Handle button input on menu screen
 * @param event Button event
 * @return Next screen to show, or SCREEN_MENU to stay
 */
ScreenId_t ui_menu_handle_button(ButtonEvent_t event);

/**
 * @brief Get current menu selection index
 */
int8_t ui_menu_get_selection(void);

/**
 * @brief Check if menu is locked (requires PIN)
 */
bool ui_menu_is_locked(void);

/**
 * @brief Lock menu
 */
void ui_menu_lock(void);

/**
 * @brief Unlock menu
 */
void ui_menu_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_MENU_H */
