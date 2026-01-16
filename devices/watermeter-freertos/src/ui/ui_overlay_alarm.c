/**
 * @file ui_overlay_alarm.c
 * @brief Alarm overlay for water meter
 */

#include "ui_overlay_alarm.h"
#include "ui/ui_common.h"
#include <stdio.h>

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

static lv_obj_t *m_overlay = NULL;
static lv_obj_t *m_title_label = NULL;
static lv_obj_t *m_detail_label = NULL;
static bool m_active = false;
static bool m_acknowledged = false;
static AlarmType_t m_current_type = ALARM_CLEARED;

/* ==========================================================================
 * ALARM NAMES
 * ========================================================================== */

const char *ui_alarm_get_name(AlarmType_t type)
{
    switch (type) {
        case ALARM_LEAK:         return "LEAK";
        case ALARM_REVERSE_FLOW: return "REVERSE";
        case ALARM_TAMPER:       return "TAMPER";
        case ALARM_HIGH_FLOW:    return "HIGH FLOW";
        default:                 return "ALARM";
    }
}

static lv_color_t get_alarm_color(AlarmType_t type)
{
    switch (type) {
        case ALARM_LEAK:
        case ALARM_TAMPER:
            return UI_COLOR_ERROR;
        case ALARM_REVERSE_FLOW:
        case ALARM_HIGH_FLOW:
            return UI_COLOR_WARNING;
        default:
            return UI_COLOR_WARNING;
    }
}

/* ==========================================================================
 * CREATION
 * ========================================================================== */

void ui_alarm_create(lv_obj_t *parent)
{
    if (parent == NULL) return;
    
    /* Create overlay container */
    m_overlay = lv_obj_create(parent);
    lv_obj_set_size(m_overlay, LV_PCT(100), 80);
    lv_obj_align(m_overlay, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(m_overlay, UI_COLOR_WARNING, 0);
    lv_obj_set_style_bg_opa(m_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_overlay, 0, 0);
    lv_obj_set_style_radius(m_overlay, 0, 0);
    lv_obj_set_style_pad_all(m_overlay, 8, 0);
    lv_obj_clear_flag(m_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
    
    /* Title label */
    m_title_label = lv_label_create(m_overlay);
    lv_label_set_text(m_title_label, "");
    lv_obj_set_style_text_font(m_title_label, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(m_title_label, lv_color_white(), 0);
    lv_obj_align(m_title_label, LV_ALIGN_TOP_MID, 0, 0);
    
    /* Detail label */
    m_detail_label = lv_label_create(m_overlay);
    lv_label_set_text(m_detail_label, "");
    lv_obj_set_style_text_font(m_detail_label, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(m_detail_label, lv_color_white(), 0);
    lv_obj_align(m_detail_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    m_active = false;
    m_acknowledged = false;
}

/* ==========================================================================
 * SHOW/HIDE
 * ========================================================================== */

void ui_alarm_show(AlarmType_t type, uint32_t duration_sec,
                   float flow_lpm, float volume_liters)
{
    if (m_overlay == NULL) return;
    
    m_current_type = type;
    m_active = true;
    m_acknowledged = false;
    
    /* Set color based on alarm type */
    lv_obj_set_style_bg_color(m_overlay, get_alarm_color(type), 0);
    
    /* Set title */
    lv_label_set_text(m_title_label, ui_alarm_get_name(type));
    
    /* Format detail text */
    char buf[64];
    uint32_t mins = duration_sec / 60;
    uint32_t secs = duration_sec % 60;
    
    if (type == ALARM_LEAK) {
        snprintf(buf, sizeof(buf), "%.1f L over %lu:%02lu", 
                 (double)volume_liters, (unsigned long)mins, (unsigned long)secs);
    } else if (type == ALARM_HIGH_FLOW) {
        snprintf(buf, sizeof(buf), "%.1f LPM for %lu:%02lu",
                 (double)flow_lpm, (unsigned long)mins, (unsigned long)secs);
    } else {
        snprintf(buf, sizeof(buf), "Duration: %lu:%02lu",
                 (unsigned long)mins, (unsigned long)secs);
    }
    lv_label_set_text(m_detail_label, buf);
    
    /* Show overlay */
    lv_obj_clear_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
}

void ui_alarm_acknowledge(void)
{
    m_acknowledged = true;
    /* Could change appearance to indicate acknowledged */
}

void ui_alarm_dismiss(void)
{
    if (m_overlay == NULL) return;
    
    m_active = false;
    m_acknowledged = false;
    m_current_type = ALARM_CLEARED;
    lv_obj_add_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
}

bool ui_alarm_is_active(void)
{
    return m_active;
}
