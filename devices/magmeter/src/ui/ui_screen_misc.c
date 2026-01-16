/**
 * @file ui_screen_misc.c
 * @brief Miscellaneous screens (splash, about, totalizer, error)
 */

#include "ui_screen_misc.h"
#include "ui/ui_common.h"
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

/* Splash screen */
static lv_obj_t *m_splash_screen = NULL;

/* About screen */
static lv_obj_t *m_about_screen = NULL;
static lv_obj_t *m_about_version_label = NULL;
static lv_obj_t *m_about_build_label = NULL;

/* Totalizer screen */
static lv_obj_t *m_totalizer_screen = NULL;
static lv_obj_t *m_totalizer_value_label = NULL;
static lv_obj_t *m_totalizer_unit_label = NULL;

/* Error screen */
static lv_obj_t *m_error_screen = NULL;
static lv_obj_t *m_error_msg_label = NULL;

static ScreenId_t m_current_misc_screen = SCREEN_MAIN;
static totalizer_reset_cb_t m_reset_callback = NULL;

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static void format_volume(char *value_buf, size_t value_size,
                          char *unit_buf, size_t unit_size,
                          float liters)
{
    float value;
    const char *unit;
    
    if (liters >= 1000000.0f) {
        value = liters / 1000000.0f;
        unit = "ML";
    } else if (liters >= 1000.0f) {
        value = liters / 1000.0f;
        unit = "kL";
    } else {
        value = liters;
        unit = "L";
    }
    
    if (value < 10.0f) {
        snprintf(value_buf, value_size, "%.2f", (double)value);
    } else if (value < 100.0f) {
        snprintf(value_buf, value_size, "%.1f", (double)value);
    } else {
        snprintf(value_buf, value_size, "%.0f", (double)value);
    }
    
    snprintf(unit_buf, unit_size, "%s", unit);
}

/* ==========================================================================
 * SCREEN CREATION
 * ========================================================================== */

void ui_misc_create(void)
{
    lv_obj_t *content;
    
    /* ===== Splash Screen ===== */
    m_splash_screen = ui_create_screen();
    
    /* Logo/title */
    lv_obj_t *title = ui_create_label_centered(m_splash_screen, "AgSys",
        UI_FONT_XLARGE, UI_COLOR_ACCENT);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);
    
    lv_obj_t *subtitle = ui_create_label_centered(m_splash_screen, "Water Meter",
        UI_FONT_LARGE, UI_COLOR_TEXT);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t *loading = ui_create_label_centered(m_splash_screen, "Initializing...",
        UI_FONT_SMALL, UI_COLOR_TEXT_LABEL);
    lv_obj_align(loading, LV_ALIGN_CENTER, 0, 60);
    
    /* ===== About Screen ===== */
    m_about_screen = ui_create_screen_with_header("About", &content);
    
    lv_obj_t *product = ui_create_label_centered(content, "AgSys Water Meter",
        UI_FONT_LARGE, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(product, 20, 0);
    
    lv_obj_t *model = ui_create_label_centered(content, "Model: MAG-100",
        UI_FONT_NORMAL, UI_COLOR_TEXT_LABEL);
    lv_obj_set_style_pad_top(model, 10, 0);
    
    m_about_version_label = ui_create_label_centered(content, "Version: --",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_about_version_label, 20, 0);
    
    m_about_build_label = ui_create_label_centered(content, "Build: --",
        UI_FONT_SMALL, UI_COLOR_TEXT_LABEL);
    lv_obj_set_style_pad_top(m_about_build_label, 5, 0);
    
    lv_obj_t *copyright = ui_create_label_centered(content, "(c) 2026 AgSys Inc.",
        UI_FONT_SMALL, UI_COLOR_TEXT_MUTED);
    lv_obj_set_style_pad_top(copyright, 40, 0);
    
    /* ===== Totalizer Screen ===== */
    m_totalizer_screen = ui_create_screen_with_header("Totalizer", &content);
    
    lv_obj_t *total_label = ui_create_label_centered(content, "Total Volume",
        UI_FONT_NORMAL, UI_COLOR_TEXT_LABEL);
    lv_obj_set_style_pad_top(total_label, 30, 0);
    
    m_totalizer_value_label = ui_create_label_centered(content, "0.00",
        UI_FONT_XLARGE, UI_COLOR_ACCENT);
    lv_obj_set_style_pad_top(m_totalizer_value_label, 10, 0);
    
    m_totalizer_unit_label = ui_create_label_centered(content, "L",
        UI_FONT_LARGE, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_totalizer_unit_label, 5, 0);
    
    lv_obj_t *reset_hint = ui_create_label_centered(content, "Hold SELECT to reset",
        UI_FONT_SMALL, UI_COLOR_TEXT_MUTED);
    lv_obj_set_style_pad_top(reset_hint, 50, 0);
    
    /* ===== Error Screen ===== */
    m_error_screen = ui_create_screen_with_header("Error", &content);
    
    lv_obj_t *error_icon = ui_create_label_centered(content, LV_SYMBOL_WARNING,
        UI_FONT_XLARGE, UI_COLOR_ERROR);
    lv_obj_set_style_pad_top(error_icon, 30, 0);
    
    m_error_msg_label = ui_create_label_centered(content, "",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_error_msg_label, 20, 0);
    lv_label_set_long_mode(m_error_msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m_error_msg_label, LV_PCT(90));
    
    lv_obj_t *dismiss_hint = ui_create_label_centered(content, "Press any button to dismiss",
        UI_FONT_SMALL, UI_COLOR_TEXT_MUTED);
    lv_obj_set_style_pad_top(dismiss_hint, 40, 0);
}

/* ==========================================================================
 * SHOW FUNCTIONS
 * ========================================================================== */

void ui_misc_show_splash(void)
{
    lv_scr_load(m_splash_screen);
}

void ui_misc_show_about(const char *version, const char *build_date)
{
    char buf[64];
    
    if (version && strlen(version) > 0) {
        snprintf(buf, sizeof(buf), "Version: %s", version);
        lv_label_set_text(m_about_version_label, buf);
    }
    
    if (build_date && strlen(build_date) > 0) {
        snprintf(buf, sizeof(buf), "Build: %s", build_date);
        lv_label_set_text(m_about_build_label, buf);
    }
    
    m_current_misc_screen = SCREEN_ABOUT;
    lv_scr_load(m_about_screen);
}

void ui_misc_show_totalizer(float total_liters)
{
    ui_misc_update_totalizer(total_liters);
    m_current_misc_screen = SCREEN_TOTALIZER;
    lv_scr_load(m_totalizer_screen);
}

void ui_misc_update_totalizer(float total_liters)
{
    char value_buf[32];
    char unit_buf[8];
    
    format_volume(value_buf, sizeof(value_buf), unit_buf, sizeof(unit_buf), total_liters);
    lv_label_set_text(m_totalizer_value_label, value_buf);
    lv_label_set_text(m_totalizer_unit_label, unit_buf);
}

void ui_misc_show_error(const char *message)
{
    lv_label_set_text(m_error_msg_label, message ? message : "Unknown error");
    m_current_misc_screen = SCREEN_ALARM;  /* Reuse SCREEN_ALARM for error */
    lv_scr_load(m_error_screen);
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

ScreenId_t ui_misc_handle_button(ButtonEvent_t event)
{
    switch (m_current_misc_screen) {
        case SCREEN_ABOUT:
            if (event == BTN_LEFT_SHORT || event == BTN_LEFT_LONG) {
                return SCREEN_MENU;
            }
            break;
            
        case SCREEN_TOTALIZER:
            if (event == BTN_LEFT_SHORT || event == BTN_LEFT_LONG) {
                return SCREEN_MENU;
            }
            if (event == BTN_SELECT_LONG) {
                /* Reset totalizer */
                if (m_reset_callback) {
                    m_reset_callback();
                }
                ui_misc_update_totalizer(0.0f);
            }
            break;
            
        case SCREEN_ALARM:  /* Error screen */
            /* Any button dismisses error */
            return SCREEN_MAIN;
            
        default:
            break;
    }
    
    return m_current_misc_screen;
}

void ui_misc_set_totalizer_callback(totalizer_reset_cb_t cb)
{
    m_reset_callback = cb;
}
