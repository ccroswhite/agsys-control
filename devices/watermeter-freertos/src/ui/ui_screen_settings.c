/**
 * @file ui_screen_settings.c
 * @brief Settings screens for water meter (display, flow, alarm, LoRa)
 */

#include "ui_screen_settings.h"
#include "ui/ui_common.h"
#include <stdio.h>

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

/* Display settings */
static lv_obj_t *m_display_screen = NULL;
static lv_obj_t *m_display_list = NULL;
static int8_t m_display_selection = 0;

/* Flow settings */
static lv_obj_t *m_flow_screen = NULL;
static lv_obj_t *m_flow_list = NULL;
static int8_t m_flow_selection = 0;

/* Alarm settings */
static lv_obj_t *m_alarm_screen = NULL;
static lv_obj_t *m_alarm_list = NULL;
static int8_t m_alarm_selection = 0;

/* LoRa settings */
static lv_obj_t *m_lora_screen = NULL;
static lv_obj_t *m_lora_list = NULL;
static int8_t m_lora_selection = 0;

static ScreenId_t m_current_settings_screen = SCREEN_DISPLAY_SETTINGS;
static UserSettings_t *m_settings = NULL;
static settings_changed_cb_t m_changed_callback = NULL;

/* ==========================================================================
 * MENU ITEMS
 * ========================================================================== */

#define DISPLAY_ITEM_COUNT 3
#define FLOW_ITEM_COUNT 1
#define ALARM_ITEM_COUNT 3
#define LORA_ITEM_COUNT 3

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static void refresh_display_list(void)
{
    if (m_display_list == NULL || m_settings == NULL) return;
    lv_obj_clean(m_display_list);
    
    char buf[48];
    
    /* Units */
    const char *unit_str = (m_settings->unitSystem == UNIT_SYSTEM_METRIC) ? "Metric" :
                           (m_settings->unitSystem == UNIT_SYSTEM_IMPERIAL) ? "Imperial" : "Imperial AG";
    snprintf(buf, sizeof(buf), "Units: %s", unit_str);
    ui_add_menu_item(m_display_list, buf, 0, m_display_selection);
    
    /* Trend period */
    snprintf(buf, sizeof(buf), "Trend Period: %u min", m_settings->trendPeriodMin);
    ui_add_menu_item(m_display_list, buf, 1, m_display_selection);
    
    /* Avg period */
    snprintf(buf, sizeof(buf), "Avg Period: %u min", m_settings->avgPeriodMin);
    ui_add_menu_item(m_display_list, buf, 2, m_display_selection);
}

static void refresh_flow_list(void)
{
    if (m_flow_list == NULL || m_settings == NULL) return;
    lv_obj_clean(m_flow_list);
    
    char buf[48];
    snprintf(buf, sizeof(buf), "Max Flow: %.0f LPM", (double)m_settings->maxFlowLPM);
    ui_add_menu_item(m_flow_list, buf, 0, m_flow_selection);
}

static void refresh_alarm_list(void)
{
    if (m_alarm_list == NULL || m_settings == NULL) return;
    lv_obj_clean(m_alarm_list);
    
    char buf[48];
    
    /* Leak threshold */
    float thresh = m_settings->alarmLeakThresholdLPM10 / 10.0f;
    snprintf(buf, sizeof(buf), "Leak Threshold: %.1f LPM", (double)thresh);
    ui_add_menu_item(m_alarm_list, buf, 0, m_alarm_selection);
    
    /* Leak duration */
    snprintf(buf, sizeof(buf), "Leak Duration: %u min", m_settings->alarmLeakDurationMin);
    ui_add_menu_item(m_alarm_list, buf, 1, m_alarm_selection);
    
    /* High flow */
    snprintf(buf, sizeof(buf), "High Flow: %u LPM", m_settings->alarmHighFlowLPM);
    ui_add_menu_item(m_alarm_list, buf, 2, m_alarm_selection);
}

static void refresh_lora_list(void)
{
    if (m_lora_list == NULL || m_settings == NULL) return;
    lv_obj_clean(m_lora_list);
    
    char buf[48];
    
    /* Report interval */
    snprintf(buf, sizeof(buf), "Report Interval: %us", m_settings->loraReportIntervalSec);
    ui_add_menu_item(m_lora_list, buf, 0, m_lora_selection);
    
    /* Spreading factor */
    snprintf(buf, sizeof(buf), "Spreading Factor: SF%u", m_settings->loraSpreadingFactor);
    ui_add_menu_item(m_lora_list, buf, 1, m_lora_selection);
    
    /* Test ping */
    ui_add_menu_item(m_lora_list, "Send Test Ping", 2, m_lora_selection);
}

/* ==========================================================================
 * SCREEN CREATION
 * ========================================================================== */

void ui_settings_create(void)
{
    lv_obj_t *content;
    
    /* Display Settings */
    m_display_screen = ui_create_screen_with_header("Display Settings", &content);
    m_display_list = ui_create_menu_list(content);
    
    /* Flow Settings */
    m_flow_screen = ui_create_screen_with_header("Flow Settings", &content);
    m_flow_list = ui_create_menu_list(content);
    
    /* Alarm Settings */
    m_alarm_screen = ui_create_screen_with_header("Alarm Settings", &content);
    m_alarm_list = ui_create_menu_list(content);
    
    /* LoRa Config */
    m_lora_screen = ui_create_screen_with_header("LoRa Config", &content);
    m_lora_list = ui_create_menu_list(content);
}

/* ==========================================================================
 * SHOW FUNCTIONS
 * ========================================================================== */

void ui_settings_show_display(void)
{
    m_display_selection = 0;
    refresh_display_list();
    m_current_settings_screen = SCREEN_DISPLAY_SETTINGS;
    lv_scr_load(m_display_screen);
}

void ui_settings_show_flow(void)
{
    m_flow_selection = 0;
    refresh_flow_list();
    m_current_settings_screen = SCREEN_FLOW_SETTINGS;
    lv_scr_load(m_flow_screen);
}

void ui_settings_show_alarm(void)
{
    m_alarm_selection = 0;
    refresh_alarm_list();
    m_current_settings_screen = SCREEN_ALARM_SETTINGS;
    lv_scr_load(m_alarm_screen);
}

void ui_settings_show_lora(void)
{
    m_lora_selection = 0;
    refresh_lora_list();
    m_current_settings_screen = SCREEN_LORA_CONFIG;
    lv_scr_load(m_lora_screen);
}

/* ==========================================================================
 * VALUE ADJUSTMENT
 * ========================================================================== */

static void adjust_display_value(int8_t item, int8_t dir)
{
    if (m_settings == NULL) return;
    
    switch (item) {
        case 0: /* Units */
            if (dir > 0) {
                m_settings->unitSystem = (m_settings->unitSystem + 1) % 3;
            } else {
                m_settings->unitSystem = (m_settings->unitSystem + 2) % 3;
            }
            break;
        case 1: /* Trend period */
            if (dir > 0 && m_settings->trendPeriodMin < 60) {
                m_settings->trendPeriodMin++;
            } else if (dir < 0 && m_settings->trendPeriodMin > 1) {
                m_settings->trendPeriodMin--;
            }
            break;
        case 2: /* Avg period */
            if (dir > 0 && m_settings->avgPeriodMin < 120) {
                m_settings->avgPeriodMin += 5;
            } else if (dir < 0 && m_settings->avgPeriodMin > 5) {
                m_settings->avgPeriodMin -= 5;
            }
            break;
    }
    
    refresh_display_list();
    if (m_changed_callback) m_changed_callback();
}

static void adjust_flow_value(int8_t item, int8_t dir)
{
    if (m_settings == NULL) return;
    
    if (item == 0) { /* Max flow */
        if (dir > 0 && m_settings->maxFlowLPM < 1000.0f) {
            m_settings->maxFlowLPM += 10.0f;
        } else if (dir < 0 && m_settings->maxFlowLPM > 10.0f) {
            m_settings->maxFlowLPM -= 10.0f;
        }
    }
    
    refresh_flow_list();
    if (m_changed_callback) m_changed_callback();
}

static void adjust_alarm_value(int8_t item, int8_t dir)
{
    if (m_settings == NULL) return;
    
    switch (item) {
        case 0: /* Leak threshold */
            if (dir > 0 && m_settings->alarmLeakThresholdLPM10 < 100) {
                m_settings->alarmLeakThresholdLPM10 += 5;
            } else if (dir < 0 && m_settings->alarmLeakThresholdLPM10 > 5) {
                m_settings->alarmLeakThresholdLPM10 -= 5;
            }
            break;
        case 1: /* Leak duration */
            if (dir > 0 && m_settings->alarmLeakDurationMin < 240) {
                m_settings->alarmLeakDurationMin += 5;
            } else if (dir < 0 && m_settings->alarmLeakDurationMin > 5) {
                m_settings->alarmLeakDurationMin -= 5;
            }
            break;
        case 2: /* High flow */
            if (dir > 0 && m_settings->alarmHighFlowLPM < 500) {
                m_settings->alarmHighFlowLPM += 10;
            } else if (dir < 0 && m_settings->alarmHighFlowLPM > 10) {
                m_settings->alarmHighFlowLPM -= 10;
            }
            break;
    }
    
    refresh_alarm_list();
    if (m_changed_callback) m_changed_callback();
}

static void adjust_lora_value(int8_t item, int8_t dir)
{
    if (m_settings == NULL) return;
    
    switch (item) {
        case 0: /* Report interval */
            if (dir > 0 && m_settings->loraReportIntervalSec < 3600) {
                m_settings->loraReportIntervalSec += 10;
            } else if (dir < 0 && m_settings->loraReportIntervalSec > 10) {
                m_settings->loraReportIntervalSec -= 10;
            }
            break;
        case 1: /* Spreading factor */
            if (dir > 0 && m_settings->loraSpreadingFactor < 12) {
                m_settings->loraSpreadingFactor++;
            } else if (dir < 0 && m_settings->loraSpreadingFactor > 7) {
                m_settings->loraSpreadingFactor--;
            }
            break;
        case 2: /* Test ping - action on select, not adjust */
            break;
    }
    
    refresh_lora_list();
    if (m_changed_callback) m_changed_callback();
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

ScreenId_t ui_settings_handle_button(ButtonEvent_t event)
{
    int8_t *selection = NULL;
    int8_t count = 0;
    lv_obj_t *list = NULL;
    void (*adjust_fn)(int8_t, int8_t) = NULL;
    
    switch (m_current_settings_screen) {
        case SCREEN_DISPLAY_SETTINGS:
            selection = &m_display_selection;
            count = DISPLAY_ITEM_COUNT;
            list = m_display_list;
            adjust_fn = adjust_display_value;
            break;
        case SCREEN_FLOW_SETTINGS:
            selection = &m_flow_selection;
            count = FLOW_ITEM_COUNT;
            list = m_flow_list;
            adjust_fn = adjust_flow_value;
            break;
        case SCREEN_ALARM_SETTINGS:
            selection = &m_alarm_selection;
            count = ALARM_ITEM_COUNT;
            list = m_alarm_list;
            adjust_fn = adjust_alarm_value;
            break;
        case SCREEN_LORA_CONFIG:
            selection = &m_lora_selection;
            count = LORA_ITEM_COUNT;
            list = m_lora_list;
            adjust_fn = adjust_lora_value;
            break;
        default:
            return SCREEN_MENU;
    }
    
    int8_t old_sel = *selection;
    
    switch (event) {
        case BTN_UP_SHORT:
        case BTN_UP_LONG:
            if (*selection > 0) {
                (*selection)--;
                ui_menu_update_selection(list, old_sel, *selection);
            }
            break;
            
        case BTN_DOWN_SHORT:
        case BTN_DOWN_LONG:
            if (*selection < count - 1) {
                (*selection)++;
                ui_menu_update_selection(list, old_sel, *selection);
            }
            break;
            
        case BTN_RIGHT_SHORT:
            /* Increase value */
            if (adjust_fn) adjust_fn(*selection, 1);
            break;
            
        case BTN_RIGHT_LONG:
            /* Fast increase */
            if (adjust_fn) {
                for (int i = 0; i < 5; i++) adjust_fn(*selection, 1);
            }
            break;
            
        case BTN_SELECT_SHORT:
            /* Toggle or action */
            if (adjust_fn) adjust_fn(*selection, 1);
            break;
            
        case BTN_LEFT_SHORT:
        case BTN_LEFT_LONG:
            return SCREEN_MENU;
            
        default:
            break;
    }
    
    return m_current_settings_screen;
}

/* ==========================================================================
 * SETTINGS REFERENCE
 * ========================================================================== */

void ui_settings_set_ref(UserSettings_t *settings)
{
    m_settings = settings;
}

void ui_settings_set_callback(settings_changed_cb_t cb)
{
    m_changed_callback = cb;
}
