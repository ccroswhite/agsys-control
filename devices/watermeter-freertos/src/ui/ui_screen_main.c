/**
 * @file ui_screen_main.c
 * @brief Main flow display screen for water meter
 * 
 * Shows current flow rate, trend, average, and total volume.
 */

#include "ui_screen_main.h"
#include "ui/ui_common.h"
#include "board_config.h"
#include <stdio.h>
#include <math.h>

/* ==========================================================================
 * MAGMETER-SPECIFIC COLORS
 * ========================================================================== */

#define COLOR_FLOW_FWD      lv_color_hex(0x0066CC)
#define COLOR_FLOW_REV      lv_color_hex(0xFF6600)
#define COLOR_FLOW_IDLE     lv_color_hex(0x909090)
#define UI_COLOR_WARNING    lv_color_hex(0xCC6600)
#define UI_COLOR_BLE        lv_color_hex(0x0082FC)

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

static lv_obj_t *m_screen = NULL;
static lv_obj_t *m_label_flow_value = NULL;
static lv_obj_t *m_label_flow_unit = NULL;
static lv_obj_t *m_obj_flow_bar = NULL;
static lv_obj_t *m_obj_flow_arrow = NULL;
static lv_obj_t *m_label_trend_value = NULL;
static lv_obj_t *m_label_avg_value = NULL;
static lv_obj_t *m_label_total_value = NULL;
static lv_obj_t *m_label_total_unit = NULL;
static lv_obj_t *m_total_section = NULL;

/* Alarm overlay */
static lv_obj_t *m_alarm_overlay = NULL;
static lv_obj_t *m_alarm_title_label = NULL;
static lv_obj_t *m_alarm_detail_label = NULL;
static bool m_alarm_active = false;

/* BLE icon */
static lv_obj_t *m_ble_icon = NULL;

/* User settings reference */
static UserSettings_t *m_settings = NULL;

/* ==========================================================================
 * LAYOUT CONSTANTS
 * ========================================================================== */

#define FRAME_BORDER        2
#define FRAME_RADIUS        8
#define FRAME_PAD           3
#define CONTENT_WIDTH       (DISPLAY_WIDTH - 2 * (FRAME_BORDER + FRAME_PAD))
#define CONTENT_HEIGHT      (DISPLAY_HEIGHT - 2 * (FRAME_BORDER + FRAME_PAD))
#define FLOW_SECTION_H      95
#define MID_SECTION_H       70
#define MID_SECTION_Y       (FLOW_SECTION_H + 1)
#define TOTAL_SECTION_Y     (MID_SECTION_Y + MID_SECTION_H)
#define TOTAL_SECTION_H     (CONTENT_HEIGHT - TOTAL_SECTION_Y - 1)
#define BLE_ICON_SIZE       24

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static const char *get_flow_unit_str(UnitSystem_t units)
{
    switch (units) {
        case UNIT_SYSTEM_IMPERIAL: return "GPM";
        case UNIT_SYSTEM_METRIC:
        default: return "LPM";
    }
}

static float convert_flow_rate(float lpm, UnitSystem_t units)
{
    if (units == UNIT_SYSTEM_IMPERIAL) {
        return lpm * 0.264172f;  /* LPM to GPM */
    }
    return lpm;
}

static void format_flow_value(char *buf, size_t size, float value)
{
    if (value < 10.0f) {
        snprintf(buf, size, "%.2f", (double)value);
    } else if (value < 100.0f) {
        snprintf(buf, size, "%.1f", (double)value);
    } else {
        snprintf(buf, size, "%.0f", (double)value);
    }
}

static void format_volume_with_unit(char *value_buf, size_t value_size,
                                     char *unit_buf, size_t unit_size,
                                     float liters, UnitSystem_t units)
{
    float value;
    const char *unit;
    
    if (units == UNIT_SYSTEM_IMPERIAL) {
        float gallons = liters * 0.264172f;
        if (gallons >= 1000000.0f) {
            value = gallons / 1000000.0f;
            unit = "MG";
        } else if (gallons >= 1000.0f) {
            value = gallons / 1000.0f;
            unit = "kG";
        } else {
            value = gallons;
            unit = "G";
        }
    } else {
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

void ui_main_create(void)
{
    /* Create main screen */
    m_screen = ui_create_screen();
    
    /* Outer frame */
    lv_obj_t *frame = lv_obj_create(m_screen);
    lv_obj_set_size(frame, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_align(frame, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(frame, UI_COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(frame, FRAME_BORDER, 0);
    lv_obj_set_style_border_color(frame, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(frame, FRAME_RADIUS, 0);
    lv_obj_set_style_pad_all(frame, FRAME_PAD, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    
    /* ===== Flow Section ===== */
    lv_obj_t *flow_section = lv_obj_create(frame);
    lv_obj_set_size(flow_section, CONTENT_WIDTH, FLOW_SECTION_H);
    lv_obj_align(flow_section, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(flow_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flow_section, 0, 0);
    lv_obj_set_style_pad_all(flow_section, 0, 0);
    lv_obj_clear_flag(flow_section, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Flow value */
    m_label_flow_value = lv_label_create(flow_section);
    lv_label_set_text(m_label_flow_value, "0.0");
    lv_obj_set_style_text_font(m_label_flow_value, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(m_label_flow_value, UI_COLOR_TEXT, 0);
    lv_obj_align(m_label_flow_value, LV_ALIGN_TOP_MID, -20, 0);
    
    /* Flow unit */
    m_label_flow_unit = lv_label_create(flow_section);
    UnitSystem_t units = m_settings ? m_settings->unitSystem : UNIT_SYSTEM_METRIC;
    lv_label_set_text(m_label_flow_unit, get_flow_unit_str(units));
    lv_obj_set_style_text_font(m_label_flow_unit, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(m_label_flow_unit, UI_COLOR_TEXT_LABEL, 0);
    lv_obj_align_to(m_label_flow_unit, m_label_flow_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -8);
    
    /* Flow bar container */
    lv_obj_t *bar_container = lv_obj_create(flow_section);
    lv_obj_set_size(bar_container, CONTENT_WIDTH - 10, 22);
    lv_obj_align(bar_container, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_bg_color(bar_container, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_border_width(bar_container, 1, 0);
    lv_obj_set_style_border_color(bar_container, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(bar_container, 4, 0);
    lv_obj_set_style_pad_all(bar_container, 2, 0);
    lv_obj_clear_flag(bar_container, LV_OBJ_FLAG_SCROLLABLE);
    
    m_obj_flow_bar = lv_bar_create(bar_container);
    lv_obj_set_size(m_obj_flow_bar, CONTENT_WIDTH - 50, 14);
    lv_obj_align(m_obj_flow_bar, LV_ALIGN_LEFT_MID, 2, 0);
    lv_bar_set_range(m_obj_flow_bar, 0, 100);
    lv_bar_set_value(m_obj_flow_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_obj_flow_bar, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_obj_flow_bar, COLOR_FLOW_FWD, LV_PART_INDICATOR);
    lv_obj_set_style_radius(m_obj_flow_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(m_obj_flow_bar, 3, LV_PART_INDICATOR);
    
    m_obj_flow_arrow = lv_label_create(bar_container);
    lv_label_set_text(m_obj_flow_arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(m_obj_flow_arrow, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(m_obj_flow_arrow, COLOR_FLOW_IDLE, 0);
    lv_obj_align(m_obj_flow_arrow, LV_ALIGN_RIGHT_MID, -2, 0);
    
    /* "Current Flow Rate" label */
    lv_obj_t *label_current = lv_label_create(flow_section);
    lv_label_set_text(label_current, "Current Flow Rate");
    lv_obj_set_style_text_font(label_current, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(label_current, UI_COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_current, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    /* Divider 1 */
    lv_obj_t *divider1 = ui_create_divider(frame, CONTENT_WIDTH);
    lv_obj_align(divider1, LV_ALIGN_TOP_MID, 0, FLOW_SECTION_H);
    
    /* ===== Middle Section: Trend | Avg ===== */
    
    /* Trend panel */
    lv_obj_t *trend_panel = lv_obj_create(frame);
    lv_obj_set_size(trend_panel, CONTENT_WIDTH / 2 - 1, MID_SECTION_H);
    lv_obj_align(trend_panel, LV_ALIGN_TOP_LEFT, 0, MID_SECTION_Y);
    lv_obj_set_style_bg_opa(trend_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(trend_panel, 0, 0);
    lv_obj_set_style_pad_all(trend_panel, 2, 0);
    lv_obj_clear_flag(trend_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    m_label_trend_value = lv_label_create(trend_panel);
    lv_label_set_text(m_label_trend_value, "+0.0L");
    lv_obj_set_style_text_font(m_label_trend_value, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(m_label_trend_value, UI_COLOR_TEXT, 0);
    lv_obj_align(m_label_trend_value, LV_ALIGN_CENTER, 0, -8);
    
    lv_obj_t *label_trend = lv_label_create(trend_panel);
    lv_label_set_text(label_trend, "Trend");
    lv_obj_set_style_text_font(label_trend, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(label_trend, UI_COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_trend, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    /* Vertical divider */
    lv_obj_t *vdivider = lv_obj_create(frame);
    lv_obj_set_size(vdivider, 1, MID_SECTION_H);
    lv_obj_align(vdivider, LV_ALIGN_TOP_MID, 0, MID_SECTION_Y);
    lv_obj_set_style_bg_color(vdivider, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(vdivider, 0, 0);
    
    /* Avg panel */
    lv_obj_t *avg_panel = lv_obj_create(frame);
    lv_obj_set_size(avg_panel, CONTENT_WIDTH / 2 - 1, MID_SECTION_H);
    lv_obj_align(avg_panel, LV_ALIGN_TOP_RIGHT, 0, MID_SECTION_Y);
    lv_obj_set_style_bg_opa(avg_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(avg_panel, 0, 0);
    lv_obj_set_style_pad_all(avg_panel, 2, 0);
    lv_obj_clear_flag(avg_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    m_label_avg_value = lv_label_create(avg_panel);
    lv_label_set_text(m_label_avg_value, "0.0L");
    lv_obj_set_style_text_font(m_label_avg_value, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(m_label_avg_value, UI_COLOR_TEXT, 0);
    lv_obj_align(m_label_avg_value, LV_ALIGN_CENTER, 0, -8);
    
    lv_obj_t *label_avg = lv_label_create(avg_panel);
    lv_label_set_text(label_avg, "AVG Vol");
    lv_obj_set_style_text_font(label_avg, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(label_avg, UI_COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_avg, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    /* Divider 2 */
    lv_obj_t *divider2 = ui_create_divider(frame, CONTENT_WIDTH);
    lv_obj_align(divider2, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y);
    
    /* ===== Total Section ===== */
    m_total_section = lv_obj_create(frame);
    lv_obj_set_size(m_total_section, CONTENT_WIDTH, TOTAL_SECTION_H);
    lv_obj_align(m_total_section, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y + 1);
    lv_obj_set_style_bg_opa(m_total_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_total_section, 0, 0);
    lv_obj_set_style_pad_all(m_total_section, 2, 0);
    lv_obj_clear_flag(m_total_section, LV_OBJ_FLAG_SCROLLABLE);
    
    m_label_total_value = lv_label_create(m_total_section);
    lv_label_set_text(m_label_total_value, "0.0");
    lv_obj_set_style_text_font(m_label_total_value, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(m_label_total_value, UI_COLOR_TEXT, 0);
    lv_obj_align(m_label_total_value, LV_ALIGN_CENTER, -15, -8);
    
    m_label_total_unit = lv_label_create(m_total_section);
    lv_label_set_text(m_label_total_unit, "L");
    lv_obj_set_style_text_font(m_label_total_unit, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(m_label_total_unit, UI_COLOR_TEXT_LABEL, 0);
    lv_obj_align_to(m_label_total_unit, m_label_total_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 3, -5);
    
    lv_obj_t *label_total = lv_label_create(m_total_section);
    lv_label_set_text(label_total, "Total Vol");
    lv_obj_set_style_text_font(label_total, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(label_total, UI_COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_total, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    /* ===== Alarm Overlay (hidden by default) ===== */
    m_alarm_overlay = lv_obj_create(frame);
    lv_obj_set_size(m_alarm_overlay, CONTENT_WIDTH, TOTAL_SECTION_H);
    lv_obj_align(m_alarm_overlay, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y + 1);
    lv_obj_set_style_bg_color(m_alarm_overlay, UI_COLOR_WARNING, 0);
    lv_obj_set_style_border_width(m_alarm_overlay, 0, 0);
    lv_obj_set_style_radius(m_alarm_overlay, 0, 0);
    lv_obj_set_style_pad_all(m_alarm_overlay, 4, 0);
    lv_obj_add_flag(m_alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(m_alarm_overlay, LV_OBJ_FLAG_SCROLLABLE);
    
    m_alarm_title_label = lv_label_create(m_alarm_overlay);
    lv_label_set_text(m_alarm_title_label, "");
    lv_obj_set_style_text_font(m_alarm_title_label, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(m_alarm_title_label, lv_color_white(), 0);
    lv_obj_align(m_alarm_title_label, LV_ALIGN_TOP_MID, 0, 2);
    
    m_alarm_detail_label = lv_label_create(m_alarm_overlay);
    lv_label_set_text(m_alarm_detail_label, "");
    lv_obj_set_style_text_font(m_alarm_detail_label, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(m_alarm_detail_label, lv_color_white(), 0);
    lv_obj_align(m_alarm_detail_label, LV_ALIGN_CENTER, 0, 2);
    
    m_alarm_active = false;
    
    /* ===== BLE Icon (lower-right, hidden by default) ===== */
    m_ble_icon = lv_obj_create(frame);
    lv_obj_set_size(m_ble_icon, BLE_ICON_SIZE, BLE_ICON_SIZE);
    lv_obj_align(m_ble_icon, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_set_style_bg_color(m_ble_icon, UI_COLOR_BLE, 0);
    lv_obj_set_style_bg_opa(m_ble_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_ble_icon, 0, 0);
    lv_obj_set_style_radius(m_ble_icon, 4, 0);
    lv_obj_set_style_pad_all(m_ble_icon, 0, 0);
    lv_obj_clear_flag(m_ble_icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *ble_label = lv_label_create(m_ble_icon);
    lv_label_set_text(ble_label, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(ble_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(ble_label, UI_FONT_NORMAL, 0);
    lv_obj_align(ble_label, LV_ALIGN_CENTER, 0, 0);
}

void ui_main_show(void)
{
    if (m_screen != NULL) {
        lv_scr_load(m_screen);
    }
}

lv_obj_t *ui_main_get_screen(void)
{
    return m_screen;
}

/* ==========================================================================
 * UPDATE FUNCTIONS
 * ========================================================================== */

void ui_main_update(const FlowData_t *data)
{
    if (m_screen == NULL || data == NULL) return;
    
    UnitSystem_t units = m_settings ? m_settings->unitSystem : UNIT_SYSTEM_METRIC;
    float max_flow = m_settings ? m_settings->maxFlowLPM : 100.0f;
    
    char value_buf[32];
    char unit_buf[16];
    
    /* Update flow rate */
    float display_flow = convert_flow_rate(fabsf(data->flowRateLPM), units);
    format_flow_value(value_buf, sizeof(value_buf), display_flow);
    lv_label_set_text(m_label_flow_value, value_buf);
    
    /* Update flow bar */
    int32_t bar_value = (int32_t)((fabsf(data->flowRateLPM) / max_flow) * 100.0f);
    if (bar_value > 100) bar_value = 100;
    lv_bar_set_value(m_obj_flow_bar, bar_value, LV_ANIM_ON);
    
    /* Update flow direction arrow and colors */
    lv_color_t flow_color;
    const char *arrow_symbol;
    if (data->flowRateLPM > 0.1f) {
        flow_color = COLOR_FLOW_FWD;
        arrow_symbol = LV_SYMBOL_RIGHT;
    } else if (data->flowRateLPM < -0.1f) {
        flow_color = COLOR_FLOW_REV;
        arrow_symbol = LV_SYMBOL_LEFT;
    } else {
        flow_color = COLOR_FLOW_IDLE;
        arrow_symbol = LV_SYMBOL_MINUS;
    }
    lv_obj_set_style_bg_color(m_obj_flow_bar, flow_color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(m_obj_flow_arrow, flow_color, 0);
    lv_label_set_text(m_obj_flow_arrow, arrow_symbol);
    
    /* Update trend */
    float trend = data->trendVolumeLiters;
    char sign = (trend >= 0) ? '+' : '-';
    format_volume_with_unit(value_buf, sizeof(value_buf), unit_buf, sizeof(unit_buf),
                            fabsf(trend), units);
    char trend_buf[48];
    snprintf(trend_buf, sizeof(trend_buf), "%c%s%s", sign, value_buf, unit_buf);
    lv_label_set_text(m_label_trend_value, trend_buf);
    
    /* Update average */
    format_volume_with_unit(value_buf, sizeof(value_buf), unit_buf, sizeof(unit_buf),
                            data->avgVolumeLiters, units);
    char avg_buf[48];
    snprintf(avg_buf, sizeof(avg_buf), "%s%s", value_buf, unit_buf);
    lv_label_set_text(m_label_avg_value, avg_buf);
    
    /* Update total */
    format_volume_with_unit(value_buf, sizeof(value_buf), unit_buf, sizeof(unit_buf),
                            data->totalVolumeLiters, units);
    lv_label_set_text(m_label_total_value, value_buf);
    lv_label_set_text(m_label_total_unit, unit_buf);
    lv_obj_align_to(m_label_total_unit, m_label_total_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 3, -5);
}

void ui_main_update_status_bar(bool lora_connected, bool has_alarm,
                                AlarmType_t alarm_type, uint32_t last_report_sec)
{
    (void)lora_connected;
    (void)has_alarm;
    (void)alarm_type;
    (void)last_report_sec;
    /* TODO: Implement status bar updates */
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

bool ui_main_handle_button(ButtonEvent_t event)
{
    /* Main screen: SELECT or RIGHT goes to menu */
    if (event == BTN_SELECT_SHORT || event == BTN_SELECT_LONG || 
        event == BTN_RIGHT_SHORT) {
        return true;  /* Signal to show menu */
    }
    return false;
}

/* ==========================================================================
 * SETTINGS
 * ========================================================================== */

void ui_main_set_settings(UserSettings_t *settings)
{
    m_settings = settings;
}
