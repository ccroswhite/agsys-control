/**
 * @file ui_screen_cal.c
 * @brief Calibration screens for water meter
 */

#include "ui_screen_cal.h"
#include "ui/ui_common.h"
#include <stdio.h>

/* ==========================================================================
 * CALIBRATION MENU ITEMS
 * ========================================================================== */

typedef enum {
    CAL_MENU_ZERO = 0,
    CAL_MENU_SPAN,
    CAL_MENU_PIPE_SIZE,
    CAL_MENU_DUTY_CYCLE,
    CAL_MENU_VIEW,
    CAL_MENU_COUNT
} cal_menu_item_t;

static const char *m_cal_menu_text[] = {
    "Zero Calibration",
    "Span Calibration",
    "Pipe Size",
    "Duty Cycle",
    "View Calibration"
};

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

static lv_obj_t *m_menu_screen = NULL;
static lv_obj_t *m_menu_list = NULL;
static int8_t m_menu_selection = 0;

static lv_obj_t *m_zero_screen = NULL;
static lv_obj_t *m_zero_value_label = NULL;
static lv_obj_t *m_zero_status_label = NULL;

static lv_obj_t *m_span_screen = NULL;
static lv_obj_t *m_span_value_label = NULL;
static lv_obj_t *m_span_status_label = NULL;

static lv_obj_t *m_view_screen = NULL;
static lv_obj_t *m_view_zero_label = NULL;
static lv_obj_t *m_view_span_label = NULL;
static lv_obj_t *m_view_kfactor_label = NULL;
static lv_obj_t *m_view_date_label = NULL;

static ScreenId_t m_current_cal_screen = SCREEN_CALIBRATION;

static cal_action_cb_t m_zero_callback = NULL;
static cal_action_cb_t m_span_callback = NULL;

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static void refresh_menu(void)
{
    if (m_menu_list == NULL) return;
    lv_obj_clean(m_menu_list);
    
    for (int i = 0; i < CAL_MENU_COUNT; i++) {
        ui_add_menu_item(m_menu_list, m_cal_menu_text[i], i, m_menu_selection);
    }
}

/* ==========================================================================
 * SCREEN CREATION
 * ========================================================================== */

void ui_cal_create(void)
{
    lv_obj_t *content;
    
    /* ===== Calibration Menu ===== */
    m_menu_screen = ui_create_screen_with_header("Calibration", &content);
    m_menu_list = ui_create_menu_list(content);
    refresh_menu();
    
    /* ===== Zero Calibration Screen ===== */
    m_zero_screen = ui_create_screen_with_header("Zero Calibration", &content);
    
    lv_obj_t *zero_info = ui_create_label_centered(content, 
        "Ensure no flow through meter.\nPress SELECT to capture zero.", 
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(zero_info, 20, 0);
    lv_label_set_long_mode(zero_info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(zero_info, LV_PCT(90));
    
    m_zero_value_label = ui_create_label_centered(content, "Current: 0", 
        UI_FONT_XLARGE, UI_COLOR_ACCENT);
    lv_obj_set_style_pad_top(m_zero_value_label, 30, 0);
    
    m_zero_status_label = ui_create_label_centered(content, "", 
        UI_FONT_NORMAL, UI_COLOR_SUCCESS);
    lv_obj_set_style_pad_top(m_zero_status_label, 10, 0);
    
    /* ===== Span Calibration Screen ===== */
    m_span_screen = ui_create_screen_with_header("Span Calibration", &content);
    
    lv_obj_t *span_info = ui_create_label_centered(content,
        "Flow known reference rate.\nPress SELECT to set span.",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(span_info, 20, 0);
    lv_label_set_long_mode(span_info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(span_info, LV_PCT(90));
    
    m_span_value_label = ui_create_label_centered(content, "Factor: 1.000",
        UI_FONT_XLARGE, UI_COLOR_ACCENT);
    lv_obj_set_style_pad_top(m_span_value_label, 30, 0);
    
    m_span_status_label = ui_create_label_centered(content, "",
        UI_FONT_NORMAL, UI_COLOR_SUCCESS);
    lv_obj_set_style_pad_top(m_span_status_label, 10, 0);
    
    /* ===== View Calibration Screen ===== */
    m_view_screen = ui_create_screen_with_header("Calibration Data", &content);
    
    m_view_zero_label = ui_create_label(content, "Zero Offset: 0",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_view_zero_label, 20, 0);
    
    m_view_span_label = ui_create_label(content, "Span Factor: 1.000",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_view_span_label, 10, 0);
    
    m_view_kfactor_label = ui_create_label(content, "K-Factor: 1.000",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_view_kfactor_label, 10, 0);
    
    m_view_date_label = ui_create_label(content, "Cal Date: --",
        UI_FONT_NORMAL, UI_COLOR_TEXT_LABEL);
    lv_obj_set_style_pad_top(m_view_date_label, 20, 0);
}

/* ==========================================================================
 * SHOW FUNCTIONS
 * ========================================================================== */

void ui_cal_show_menu(void)
{
    m_menu_selection = 0;
    refresh_menu();
    m_current_cal_screen = SCREEN_CALIBRATION;
    lv_scr_load(m_menu_screen);
}

void ui_cal_show_zero(void)
{
    lv_label_set_text(m_zero_status_label, "");
    m_current_cal_screen = SCREEN_CAL_ZERO;
    lv_scr_load(m_zero_screen);
}

void ui_cal_show_span(void)
{
    lv_label_set_text(m_span_status_label, "");
    m_current_cal_screen = SCREEN_CAL_SPAN;
    lv_scr_load(m_span_screen);
}

void ui_cal_show_pipe_size(void)
{
    /* TODO: Implement pipe size screen */
    m_current_cal_screen = SCREEN_CAL_PIPE_SIZE;
}

void ui_cal_show_duty_cycle(void)
{
    /* TODO: Implement duty cycle screen */
    m_current_cal_screen = SCREEN_CAL_DUTY_CYCLE;
}

void ui_cal_show_view(void)
{
    m_current_cal_screen = SCREEN_CAL_VIEW;
    lv_scr_load(m_view_screen);
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

ScreenId_t ui_cal_handle_button(ButtonEvent_t event)
{
    switch (m_current_cal_screen) {
        case SCREEN_CALIBRATION: {
            int8_t old_sel = m_menu_selection;
            switch (event) {
                case BTN_UP_SHORT:
                case BTN_UP_LONG:
                    if (m_menu_selection > 0) {
                        m_menu_selection--;
                        ui_menu_update_selection(m_menu_list, old_sel, m_menu_selection);
                    }
                    break;
                case BTN_DOWN_SHORT:
                case BTN_DOWN_LONG:
                    if (m_menu_selection < CAL_MENU_COUNT - 1) {
                        m_menu_selection++;
                        ui_menu_update_selection(m_menu_list, old_sel, m_menu_selection);
                    }
                    break;
                case BTN_SELECT_SHORT:
                case BTN_RIGHT_SHORT:
                    switch (m_menu_selection) {
                        case CAL_MENU_ZERO:
                            ui_cal_show_zero();
                            return SCREEN_CAL_ZERO;
                        case CAL_MENU_SPAN:
                            ui_cal_show_span();
                            return SCREEN_CAL_SPAN;
                        case CAL_MENU_PIPE_SIZE:
                            return SCREEN_CAL_PIPE_SIZE;
                        case CAL_MENU_DUTY_CYCLE:
                            return SCREEN_CAL_DUTY_CYCLE;
                        case CAL_MENU_VIEW:
                            ui_cal_show_view();
                            return SCREEN_CAL_VIEW;
                        default:
                            break;
                    }
                    break;
                case BTN_LEFT_SHORT:
                case BTN_LEFT_LONG:
                    return SCREEN_MENU;
                default:
                    break;
            }
            break;
        }
        
        case SCREEN_CAL_ZERO:
            if (event == BTN_SELECT_SHORT) {
                if (m_zero_callback) {
                    m_zero_callback();
                }
                lv_label_set_text(m_zero_status_label, "Zero captured!");
            } else if (event == BTN_LEFT_SHORT || event == BTN_LEFT_LONG) {
                ui_cal_show_menu();
                return SCREEN_CALIBRATION;
            }
            break;
            
        case SCREEN_CAL_SPAN:
            if (event == BTN_SELECT_SHORT) {
                if (m_span_callback) {
                    m_span_callback();
                }
                lv_label_set_text(m_span_status_label, "Span set!");
            } else if (event == BTN_LEFT_SHORT || event == BTN_LEFT_LONG) {
                ui_cal_show_menu();
                return SCREEN_CALIBRATION;
            }
            break;
            
        case SCREEN_CAL_VIEW:
        case SCREEN_CAL_PIPE_SIZE:
        case SCREEN_CAL_DUTY_CYCLE:
            if (event == BTN_LEFT_SHORT || event == BTN_LEFT_LONG) {
                ui_cal_show_menu();
                return SCREEN_CALIBRATION;
            }
            break;
            
        default:
            break;
    }
    
    return m_current_cal_screen;
}

/* ==========================================================================
 * UPDATE FUNCTIONS
 * ========================================================================== */

void ui_cal_update(const CalibrationData_t *cal)
{
    if (cal == NULL) return;
    
    char buf[64];
    
    /* Update zero screen */
    snprintf(buf, sizeof(buf), "Current: %ld", (long)cal->zeroOffset);
    lv_label_set_text(m_zero_value_label, buf);
    
    /* Update span screen */
    snprintf(buf, sizeof(buf), "Factor: %.3f", (double)cal->spanFactor);
    lv_label_set_text(m_span_value_label, buf);
    
    /* Update view screen */
    snprintf(buf, sizeof(buf), "Zero Offset: %ld", (long)cal->zeroOffset);
    lv_label_set_text(m_view_zero_label, buf);
    
    snprintf(buf, sizeof(buf), "Span Factor: %.3f", (double)cal->spanFactor);
    lv_label_set_text(m_view_span_label, buf);
    
    snprintf(buf, sizeof(buf), "K-Factor: %.3f", (double)cal->kFactor);
    lv_label_set_text(m_view_kfactor_label, buf);
    
    if (cal->calDate > 0) {
        /* Simple date display - could be improved */
        snprintf(buf, sizeof(buf), "Cal Date: %lu", (unsigned long)cal->calDate);
    } else {
        snprintf(buf, sizeof(buf), "Cal Date: Not calibrated");
    }
    lv_label_set_text(m_view_date_label, buf);
}

void ui_cal_set_callbacks(cal_action_cb_t zero_cb, cal_action_cb_t span_cb)
{
    m_zero_callback = zero_cb;
    m_span_callback = span_cb;
}
