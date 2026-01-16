/**
 * @file ui_common.h
 * @brief Common UI definitions, colors, fonts, and widget factory
 * 
 * Shared across all UI screen modules for consistent styling.
 * Part of the shared freertos-common library.
 */

#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * COLOR PALETTE (Light theme for transflective display)
 * ========================================================================== */

#define UI_COLOR_BG             lv_color_hex(0xE0E0E0)
#define UI_COLOR_TEXT           lv_color_hex(0x202020)
#define UI_COLOR_TEXT_LABEL     lv_color_hex(0x606060)
#define UI_COLOR_TEXT_MUTED     lv_color_hex(0x909090)
#define UI_COLOR_DIVIDER        lv_color_hex(0x808080)
#define UI_COLOR_BAR_BG         lv_color_hex(0xC0C0C0)
#define UI_COLOR_PANEL_BG       lv_color_hex(0xF0F0F0)
#define UI_COLOR_ACCENT         lv_color_hex(0x0066CC)
#define UI_COLOR_ACCENT_LIGHT   lv_color_hex(0xD0D0FF)
#define UI_COLOR_SUCCESS        lv_color_hex(0x00AA00)
#define UI_COLOR_WARNING        lv_color_hex(0xCC6600)
#define UI_COLOR_ERROR          lv_color_hex(0xCC0000)
#define UI_COLOR_BLE            lv_color_hex(0x0082FC)

/* ==========================================================================
 * FONTS
 * ========================================================================== */

#define UI_FONT_SMALL       (&lv_font_montserrat_14)
#define UI_FONT_NORMAL      (&lv_font_montserrat_16)
#define UI_FONT_LARGE       (&lv_font_montserrat_20)
#define UI_FONT_XLARGE      (&lv_font_montserrat_28)
#define UI_FONT_HERO        (&lv_font_montserrat_28)  /* Use 28 as largest available */

/* ==========================================================================
 * COMMON DIMENSIONS
 * ========================================================================== */

#define UI_PADDING              8
#define UI_MARGIN               4
#define UI_HEADER_HEIGHT        40
#define UI_STATUS_BAR_HEIGHT    24
#define UI_MENU_ITEM_HEIGHT     44
#define UI_BUTTON_HEIGHT        40
#define UI_BORDER_RADIUS        4

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize common UI resources
 * 
 * Call after LVGL is initialized but before creating screens.
 */
void ui_common_init(void);

/* ==========================================================================
 * SCREEN FACTORY
 * ========================================================================== */

/**
 * @brief Create a new screen with standard background
 * @return New screen object
 */
lv_obj_t *ui_create_screen(void);

/**
 * @brief Create a screen with header bar
 * @param title Header title text
 * @param content_area Output: pointer to content area below header
 * @return Screen object
 */
lv_obj_t *ui_create_screen_with_header(const char *title, lv_obj_t **content_area);

/* ==========================================================================
 * WIDGET FACTORY
 * ========================================================================== */

/**
 * @brief Create a label with specified font and color
 * @param parent Parent object
 * @param text Initial text
 * @param font Font to use (NULL for default)
 * @param color Text color
 * @return Label object
 */
lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text, 
                          const lv_font_t *font, lv_color_t color);

/**
 * @brief Create a centered label
 * @param parent Parent object
 * @param text Initial text
 * @param font Font to use (NULL for default)
 * @param color Text color
 * @return Label object
 */
lv_obj_t *ui_create_label_centered(lv_obj_t *parent, const char *text,
                                    const lv_font_t *font, lv_color_t color);

/**
 * @brief Create a panel with background color
 * @param parent Parent object
 * @param width Panel width (LV_PCT(100) for full width)
 * @param height Panel height (LV_SIZE_CONTENT for auto)
 * @return Panel object
 */
lv_obj_t *ui_create_panel(lv_obj_t *parent, int32_t width, int32_t height);

/**
 * @brief Create a button with text
 * @param parent Parent object
 * @param text Button label
 * @param width Button width
 * @param cb Click callback (can be NULL)
 * @param user_data User data for callback
 * @return Button object
 */
lv_obj_t *ui_create_button(lv_obj_t *parent, const char *text,
                           int32_t width, lv_event_cb_t cb, void *user_data);

/**
 * @brief Create a progress bar
 * @param parent Parent object
 * @param width Bar width
 * @return Progress bar object (use lv_bar_set_value to update)
 */
lv_obj_t *ui_create_progress_bar(lv_obj_t *parent, int32_t width);

/**
 * @brief Create horizontal divider line
 * @param parent Parent object
 * @param width Line width (LV_PCT(100) for full width)
 * @return Line object
 */
lv_obj_t *ui_create_divider(lv_obj_t *parent, int32_t width);

/* ==========================================================================
 * MENU WIDGETS
 * ========================================================================== */

/**
 * @brief Create a scrollable menu list container
 * @param parent Parent object
 * @return List container object
 */
lv_obj_t *ui_create_menu_list(lv_obj_t *parent);

/**
 * @brief Add item to menu list
 * @param list Menu list object
 * @param text Item text
 * @param index Item index (for selection tracking)
 * @param selected Currently selected index (-1 for none)
 * @return Menu item object
 */
lv_obj_t *ui_add_menu_item(lv_obj_t *list, const char *text, 
                           int8_t index, int8_t selected);

/**
 * @brief Update menu selection highlight
 * @param list Menu list object
 * @param old_index Previous selection (-1 for none)
 * @param new_index New selection
 */
void ui_menu_update_selection(lv_obj_t *list, int8_t old_index, int8_t new_index);

/* ==========================================================================
 * FORMATTING HELPERS
 * ========================================================================== */

/**
 * @brief Format a numeric value with appropriate precision
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param value Value to format
 * @param decimals Number of decimal places
 */
void ui_format_number(char *buf, size_t buf_size, float value, int decimals);

/**
 * @brief Format duration as HH:MM:SS or MM:SS
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param seconds Duration in seconds
 */
void ui_format_duration(char *buf, size_t buf_size, uint32_t seconds);

/**
 * @brief Format percentage
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @param percent Percentage value (0-100)
 */
void ui_format_percent(char *buf, size_t buf_size, uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* UI_COMMON_H */
