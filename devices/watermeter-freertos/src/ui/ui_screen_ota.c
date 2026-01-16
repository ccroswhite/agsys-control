/**
 * @file ui_screen_ota.c
 * @brief OTA update progress and error screens for water meter
 */

#include "ui_screen_ota.h"
#include "ui/ui_common.h"
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

static lv_obj_t *m_progress_screen = NULL;
static lv_obj_t *m_progress_bar = NULL;
static lv_obj_t *m_percent_label = NULL;
static lv_obj_t *m_status_label = NULL;
static lv_obj_t *m_version_label = NULL;

static lv_obj_t *m_error_screen = NULL;
static lv_obj_t *m_error_label = NULL;
static lv_obj_t *m_error_btn = NULL;

static bool m_error_active = false;
static uint32_t m_error_start_ms = 0;
#define OTA_ERROR_TIMEOUT_MS  60000

/* ==========================================================================
 * CALLBACKS
 * ========================================================================== */

static void error_btn_cb(lv_event_t *e)
{
    (void)e;
    m_error_active = false;
    /* Caller should switch back to main screen */
}

/* ==========================================================================
 * SCREEN CREATION
 * ========================================================================== */

void ui_ota_create(void)
{
    /* ===== Progress Screen ===== */
    lv_obj_t *content;
    m_progress_screen = ui_create_screen_with_header("Firmware Update", &content);
    
    /* Version label */
    m_version_label = ui_create_label_centered(content, "", UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_version_label, 20, 0);
    
    /* Status label */
    m_status_label = ui_create_label_centered(content, "Preparing...", UI_FONT_NORMAL, UI_COLOR_TEXT_LABEL);
    lv_obj_set_style_pad_top(m_status_label, 10, 0);
    
    /* Progress bar */
    m_progress_bar = ui_create_progress_bar(content, LV_PCT(80));
    lv_obj_set_style_pad_top(m_progress_bar, 20, 0);
    
    /* Percent label */
    m_percent_label = ui_create_label_centered(content, "0%", UI_FONT_LARGE, UI_COLOR_ACCENT);
    lv_obj_set_style_pad_top(m_percent_label, 10, 0);
    
    /* Warning label */
    lv_obj_t *warning = ui_create_label_centered(content, "Do not power off", UI_FONT_SMALL, UI_COLOR_WARNING);
    lv_obj_set_style_pad_top(warning, 30, 0);
    
    /* ===== Error Screen ===== */
    m_error_screen = ui_create_screen_with_header("Update Failed", &content);
    
    /* Error icon */
    lv_obj_t *icon = ui_create_label_centered(content, LV_SYMBOL_WARNING, UI_FONT_HERO, UI_COLOR_ERROR);
    lv_obj_set_style_pad_top(icon, 20, 0);
    
    /* Error message */
    m_error_label = ui_create_label_centered(content, "", UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_error_label, 20, 0);
    lv_label_set_long_mode(m_error_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m_error_label, LV_PCT(90));
    
    /* OK button */
    m_error_btn = ui_create_button(content, "OK", 100, error_btn_cb, NULL);
    lv_obj_set_style_pad_top(m_error_btn, 30, 0);
}

/* ==========================================================================
 * PROGRESS SCREEN
 * ========================================================================== */

void ui_ota_show_progress(uint8_t percent, const char *status, const char *version)
{
    if (m_progress_screen == NULL) return;
    
    if (version != NULL && strlen(version) > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Version: %s", version);
        lv_label_set_text(m_version_label, buf);
    } else {
        lv_label_set_text(m_version_label, "");
    }
    
    ui_ota_update_status(status);
    ui_ota_update_progress(percent);
    
    lv_scr_load(m_progress_screen);
}

void ui_ota_update_progress(uint8_t percent)
{
    if (m_progress_bar == NULL) return;
    
    if (percent > 100) percent = 100;
    lv_bar_set_value(m_progress_bar, percent, LV_ANIM_ON);
    
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", percent);
    lv_label_set_text(m_percent_label, buf);
}

void ui_ota_update_status(const char *status)
{
    if (m_status_label == NULL || status == NULL) return;
    lv_label_set_text(m_status_label, status);
}

/* ==========================================================================
 * ERROR SCREEN
 * ========================================================================== */

void ui_ota_show_error(const char *error_msg)
{
    if (m_error_screen == NULL) return;
    
    lv_label_set_text(m_error_label, error_msg ? error_msg : "Unknown error");
    m_error_active = true;
    m_error_start_ms = lv_tick_get();
    
    lv_scr_load(m_error_screen);
}

bool ui_ota_is_error_active(void)
{
    return m_error_active;
}

bool ui_ota_tick_error(void)
{
    if (!m_error_active) return false;
    
    uint32_t elapsed = lv_tick_get() - m_error_start_ms;
    if (elapsed >= OTA_ERROR_TIMEOUT_MS) {
        m_error_active = false;
        return true;  /* Timeout expired */
    }
    return false;
}

void ui_ota_dismiss(void)
{
    m_error_active = false;
}
