/**
 * @file ui_common.c
 * @brief Common UI widget factory and helpers implementation
 * 
 * Provides reusable UI components for all devices with displays.
 */

#include "ui/ui_common.h"
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

void ui_common_init(void)
{
    /* Future: Initialize custom themes, fonts, etc. */
}

/* ==========================================================================
 * SCREEN FACTORY
 * ========================================================================== */

lv_obj_t *ui_create_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

lv_obj_t *ui_create_screen_with_header(const char *title, lv_obj_t **content_area)
{
    lv_obj_t *screen = ui_create_screen();
    
    /* Header bar */
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, LV_PCT(100), UI_HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Title label */
    lv_obj_t *title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);
    
    /* Content area below header */
    if (content_area != NULL) {
        lv_obj_t *content = lv_obj_create(screen);
        lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
        lv_obj_align(content, LV_ALIGN_TOP_MID, 0, UI_HEADER_HEIGHT);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_pad_all(content, UI_PADDING, 0);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        *content_area = content;
    }
    
    return screen;
}

/* ==========================================================================
 * WIDGET FACTORY
 * ========================================================================== */

lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text, 
                          const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t *ui_create_label_centered(lv_obj_t *parent, const char *text,
                                    const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = ui_create_label(parent, text, font, color);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

lv_obj_t *ui_create_panel(lv_obj_t *parent, int32_t width, int32_t height)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, UI_COLOR_PANEL_BG, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, UI_BORDER_RADIUS, 0);
    lv_obj_set_style_pad_all(panel, UI_PADDING, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t *ui_create_button(lv_obj_t *parent, const char *text,
                           int32_t width, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, width, UI_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_radius(btn, UI_BORDER_RADIUS, 0);
    
    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    
    return btn;
}

lv_obj_t *ui_create_progress_bar(lv_obj_t *parent, int32_t width)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, width, 20);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, UI_COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, UI_BORDER_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, UI_BORDER_RADIUS, LV_PART_INDICATOR);
    return bar;
}

lv_obj_t *ui_create_divider(lv_obj_t *parent, int32_t width)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, width, 1);
    lv_obj_set_style_bg_color(line, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    return line;
}

/* ==========================================================================
 * MENU WIDGETS
 * ========================================================================== */

lv_obj_t *ui_create_menu_list(lv_obj_t *parent)
{
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 2, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    return list;
}

lv_obj_t *ui_add_menu_item(lv_obj_t *list, const char *text, 
                           int8_t index, int8_t selected)
{
    lv_obj_t *item = lv_obj_create(list);
    lv_obj_set_size(item, LV_PCT(100), UI_MENU_ITEM_HEIGHT);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_radius(item, UI_BORDER_RADIUS, 0);
    lv_obj_set_style_pad_left(item, UI_PADDING, 0);
    lv_obj_set_style_pad_right(item, UI_PADDING, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Highlight if selected */
    if (index == selected) {
        lv_obj_set_style_bg_color(item, UI_COLOR_ACCENT_LIGHT, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_color(item, UI_COLOR_PANEL_BG, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    }
    
    /* Store index in user data for event handling */
    lv_obj_set_user_data(item, (void *)(intptr_t)index);
    
    /* Item label */
    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    
    /* Selection indicator (arrow) */
    if (index == selected) {
        lv_obj_t *arrow = lv_label_create(item);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arrow, UI_COLOR_ACCENT, 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    
    return item;
}

void ui_menu_update_selection(lv_obj_t *list, int8_t old_index, int8_t new_index)
{
    uint32_t child_count = lv_obj_get_child_count(list);
    
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *item = lv_obj_get_child(list, i);
        int8_t item_index = (int8_t)(intptr_t)lv_obj_get_user_data(item);
        
        if (item_index == old_index) {
            /* Deselect old item */
            lv_obj_set_style_bg_color(item, UI_COLOR_PANEL_BG, 0);
            /* Remove arrow if present */
            if (lv_obj_get_child_count(item) > 1) {
                lv_obj_t *arrow = lv_obj_get_child(item, 1);
                lv_obj_delete(arrow);
            }
        } else if (item_index == new_index) {
            /* Select new item */
            lv_obj_set_style_bg_color(item, UI_COLOR_ACCENT_LIGHT, 0);
            /* Add arrow */
            lv_obj_t *arrow = lv_label_create(item);
            lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(arrow, UI_COLOR_ACCENT, 0);
            lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }
}

/* ==========================================================================
 * FORMATTING HELPERS
 * ========================================================================== */

void ui_format_number(char *buf, size_t buf_size, float value, int decimals)
{
    if (decimals <= 0) {
        snprintf(buf, buf_size, "%d", (int)value);
    } else if (decimals == 1) {
        snprintf(buf, buf_size, "%.1f", (double)value);
    } else if (decimals == 2) {
        snprintf(buf, buf_size, "%.2f", (double)value);
    } else {
        snprintf(buf, buf_size, "%.3f", (double)value);
    }
}

void ui_format_duration(char *buf, size_t buf_size, uint32_t seconds)
{
    uint32_t hours = seconds / 3600;
    uint32_t mins = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;
    
    if (hours > 0) {
        snprintf(buf, buf_size, "%lu:%02lu:%02lu", 
                 (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
    } else {
        snprintf(buf, buf_size, "%lu:%02lu", 
                 (unsigned long)mins, (unsigned long)secs);
    }
}

void ui_format_percent(char *buf, size_t buf_size, uint8_t percent)
{
    snprintf(buf, buf_size, "%u%%", percent);
}
