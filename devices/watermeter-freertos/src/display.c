/**
 * @file display.c
 * @brief Display implementation for Mag Meter using LVGL and ST7789
 * 
 * Light theme optimized for transflective display daylight readability.
 * Ported from Arduino version for FreeRTOS/nRF52840.
 */

#include "display.h"
#include "st7789.h"
#include "board_config.h"
#include "lvgl.h"
#include "nrf_gpio.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* LVGL display buffer */
#define DISP_BUF_LINES  20
static lv_disp_draw_buf_t m_draw_buf;
static lv_color_t m_buf1[DISPLAY_WIDTH * DISP_BUF_LINES];

/* LVGL display driver */
static lv_disp_drv_t m_disp_drv;

/* Current screen */
static ScreenId_t m_currentScreen = SCREEN_MAIN;

/* User settings pointer */
static UserSettings_t *m_userSettings = NULL;

/* Color definitions (light theme for daylight readability) */
#define COLOR_BG            lv_color_hex(0xE0E0E0)
#define COLOR_TEXT          lv_color_hex(0x202020)
#define COLOR_TEXT_LABEL    lv_color_hex(0x606060)
#define COLOR_DIVIDER       lv_color_hex(0x808080)
#define COLOR_FLOW_FWD      lv_color_hex(0x0066CC)
#define COLOR_FLOW_REV      lv_color_hex(0xFF6600)
#define COLOR_FLOW_IDLE     lv_color_hex(0x909090)
#define COLOR_BAR_BG        lv_color_hex(0xC0C0C0)
#define COLOR_PANEL_BG      lv_color_hex(0xF0F0F0)
#define COLOR_ALARM_CRITICAL lv_color_hex(0xCC0000)
#define COLOR_ALARM_WARNING  lv_color_hex(0xCC6600)
#define COLOR_BLE_ACTIVE     lv_color_hex(0x0082FC)  /* Bluetooth blue */

/* Main screen UI elements */
static lv_obj_t *m_screen_main = NULL;
static lv_obj_t *m_label_flow_value = NULL;
static lv_obj_t *m_label_flow_unit = NULL;
static lv_obj_t *m_obj_flow_bar = NULL;
static lv_obj_t *m_obj_flow_arrow = NULL;
static lv_obj_t *m_label_trend_value = NULL;
static lv_obj_t *m_label_avg_value = NULL;
static lv_obj_t *m_label_total_value = NULL;
static lv_obj_t *m_label_total_unit = NULL;

/* Alarm overlay elements */
static lv_obj_t *m_total_section = NULL;
static lv_obj_t *m_alarm_overlay = NULL;
static lv_obj_t *m_alarm_title_label = NULL;
static lv_obj_t *m_alarm_detail_label = NULL;
static bool m_alarmOverlayActive = false;
static AlarmType_t m_currentAlarmType = ALARM_CLEARED;

/* BLE icon elements */
static lv_obj_t *m_ble_icon = NULL;
static BleUiState_t m_bleUiState = BLE_UI_STATE_IDLE;
static bool m_bleIconVisible = true;
static uint32_t m_bleFlashLastMs = 0;
static uint8_t m_bleFlashCount = 0;  /* For triple flash on disconnect */

/* Menu lock state */
static MenuLockState_t m_menuLockState = MENU_LOCKED;
static uint32_t m_lastActivityMs = 0;
static uint16_t m_enteredPin[6] = {0, 0, 0, 0, 0, 0};
static int8_t m_pinDigitIndex = 0;

/* Display power state */
static DisplayPowerState_t m_displayPowerState = DISPLAY_ACTIVE;
static uint32_t m_lastInputMs = 0;

/* Menu elements */
static lv_obj_t *m_screen_menu = NULL;
static lv_obj_t *m_menu_list = NULL;
static int8_t m_menuSelection = 0;
static int8_t m_menuItemCount = 0;

/* Submenu elements */
static lv_obj_t *m_screen_submenu = NULL;
static lv_obj_t *m_submenu_list = NULL;
static int8_t m_submenuSelection = 0;
static int8_t m_submenuItemCount = 0;

/* Value editor elements (for future use) */
static lv_obj_t *m_screen_editor __attribute__((unused)) = NULL;
static lv_obj_t *m_editor_value_label __attribute__((unused)) = NULL;
static int32_t m_editorValue __attribute__((unused)) = 0;
static int32_t m_editorMin __attribute__((unused)) = 0;
static int32_t m_editorMax __attribute__((unused)) = 100;
static int32_t m_editorStep __attribute__((unused)) = 1;
static void (*m_editorCallback)(int32_t value) __attribute__((unused)) = NULL;

/* PIN entry elements */
static lv_obj_t *m_screen_pin = NULL;
static lv_obj_t *m_pin_digits[6] = {NULL};
static lv_obj_t *m_pin_overlay = NULL;
static lv_obj_t *m_pin_overlay_digits[6] = {NULL};

/* Diagnostics data */
static LoRaStats_t m_loraStats = {0};
static ADCValues_t m_adcValues = {0};
static float m_totalLiters = 0.0f;

/* Forward declarations */
static void menu_refresh(void);
static void submenu_refresh(void);
static void pin_update_display(void);
static bool pin_verify(void);

/* ==========================================================================
 * LVGL DISPLAY FLUSH CALLBACK
 * ========================================================================== */

static void display_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    st7789_set_addr_window(area->x1, area->y1, area->x2, area->y2);
    st7789_write_pixels((uint16_t *)color_p, w * h);
    
    lv_disp_flush_ready(disp_drv);
}

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static uint32_t get_tick_ms(void)
{
    /* Use FreeRTOS tick count */
    extern uint32_t xTaskGetTickCount(void);
    return xTaskGetTickCount();
}

static const char *getFlowUnitStr(UnitSystem_t unitSystem)
{
    if (unitSystem == UNIT_SYSTEM_METRIC) {
        return "L/min";
    } else {
        return "GPM";
    }
}

static float convertFlowRate(float lpm, UnitSystem_t unitSystem)
{
    if (unitSystem == UNIT_SYSTEM_METRIC) {
        return lpm;
    } else {
        return lpm * LITERS_TO_GALLONS;
    }
}

static void formatFlowValue(char *buf, size_t bufSize, float value)
{
    float absVal = fabsf(value);
    if (absVal < 10.0f) {
        snprintf(buf, bufSize, "%.1f", absVal);
    } else if (absVal < 100.0f) {
        snprintf(buf, bufSize, "%.1f", absVal);
    } else {
        snprintf(buf, bufSize, "%.0f", absVal);
    }
}

static void formatVolumeWithUnit(char *valueBuf, size_t valueBufSize,
                                  char *unitBuf, size_t unitBufSize,
                                  float liters, UnitSystem_t unitSystem)
{
    float absLiters = fabsf(liters);
    
    if (unitSystem == UNIT_SYSTEM_METRIC) {
        if (absLiters < 1.0f) {
            snprintf(valueBuf, valueBufSize, "%.0f", liters * 1000.0f);
            snprintf(unitBuf, unitBufSize, "mL");
        } else if (absLiters < 1000.0f) {
            snprintf(valueBuf, valueBufSize, "%.1f", liters);
            snprintf(unitBuf, unitBufSize, "L");
        } else if (absLiters < 1000000.0f) {
            snprintf(valueBuf, valueBufSize, "%.2f", liters / 1000.0f);
            snprintf(unitBuf, unitBufSize, "kL");
        } else {
            snprintf(valueBuf, valueBufSize, "%.2f", liters / 1000000.0f);
            snprintf(unitBuf, unitBufSize, "ML");
        }
    } else if (unitSystem == UNIT_SYSTEM_IMPERIAL) {
        float gallons = liters * LITERS_TO_GALLONS;
        float absGal = fabsf(gallons);
        if (absGal < 1000.0f) {
            snprintf(valueBuf, valueBufSize, "%.1f", gallons);
            snprintf(unitBuf, unitBufSize, "gal");
        } else if (absGal < 1000000.0f) {
            snprintf(valueBuf, valueBufSize, "%.2f", gallons / 1000.0f);
            snprintf(unitBuf, unitBufSize, "kgal");
        } else {
            snprintf(valueBuf, valueBufSize, "%.2f", gallons / 1000000.0f);
            snprintf(unitBuf, unitBufSize, "Mgal");
        }
    } else { /* UNIT_SYSTEM_IMPERIAL_AG */
        float gallons = liters * LITERS_TO_GALLONS;
        float acreFt = liters * LITERS_TO_ACRE_FT;
        float absGal = fabsf(gallons);
        if (absGal < 10000.0f) {
            snprintf(valueBuf, valueBufSize, "%.1f", gallons);
            snprintf(unitBuf, unitBufSize, "gal");
        } else if (fabsf(acreFt) < 1.0f) {
            snprintf(valueBuf, valueBufSize, "%.2f", acreFt * 12.0f);
            snprintf(unitBuf, unitBufSize, "ac-in");
        } else {
            snprintf(valueBuf, valueBufSize, "%.2f", acreFt);
            snprintf(unitBuf, unitBufSize, "ac-ft");
        }
    }
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool display_init(void)
{
    /* Initialize ST7789 display */
    if (!st7789_init()) {
        return false;
    }
    
    /* Set rotation (portrait) */
    st7789_set_rotation(0);
    
    /* Fill screen with background color */
    st7789_fill_screen(0xE0E0);  /* Light gray */
    
    /* Initialize LVGL */
    lv_init();
    
    /* Initialize display buffer */
    lv_disp_draw_buf_init(&m_draw_buf, m_buf1, NULL, DISPLAY_WIDTH * DISP_BUF_LINES);
    
    /* Initialize display driver */
    lv_disp_drv_init(&m_disp_drv);
    m_disp_drv.hor_res = DISPLAY_WIDTH;
    m_disp_drv.ver_res = DISPLAY_HEIGHT;
    m_disp_drv.flush_cb = display_flush_cb;
    m_disp_drv.draw_buf = &m_draw_buf;
    lv_disp_drv_register(&m_disp_drv);
    
    /* Initialize power state timers */
    m_lastInputMs = get_tick_ms();
    m_lastActivityMs = get_tick_ms();
    m_displayPowerState = DISPLAY_ACTIVE;
    
    return true;
}

void display_tick(void)
{
    lv_tick_inc(1);
}

void display_task_handler(void)
{
    lv_timer_handler();
}

/* ==========================================================================
 * DISPLAY POWER MANAGEMENT
 * ========================================================================== */

void display_updatePowerState(void)
{
    uint32_t now = get_tick_ms();
    uint32_t idleMs = now - m_lastInputMs;
    
    /* Don't sleep if alarm is active */
    if (m_alarmOverlayActive) {
        if (m_displayPowerState != DISPLAY_ACTIVE) {
            m_displayPowerState = DISPLAY_ACTIVE;
            st7789_set_backlight(100);
            st7789_wake();
        }
        return;
    }
    
    /* State transitions based on idle time */
    switch (m_displayPowerState) {
        case DISPLAY_ACTIVE:
            if (idleMs >= (DEFAULT_DIM_TIMEOUT_SEC * 1000UL)) {
                m_displayPowerState = DISPLAY_DIM;
                st7789_set_backlight(50);
            }
            break;
            
        case DISPLAY_DIM:
            if (idleMs >= ((DEFAULT_DIM_TIMEOUT_SEC + DEFAULT_SLEEP_TIMEOUT_SEC) * 1000UL)) {
                m_displayPowerState = DISPLAY_SLEEP;
                st7789_set_backlight(0);
                st7789_sleep();
            }
            break;
            
        case DISPLAY_SLEEP:
            /* Stay asleep until button press */
            break;
    }
}

void display_wake(void)
{
    m_lastInputMs = get_tick_ms();
    
    if (m_displayPowerState == DISPLAY_SLEEP) {
        st7789_wake();
    }
    
    m_displayPowerState = DISPLAY_ACTIVE;
    st7789_set_backlight(100);
}

void display_resetActivityTimer(void)
{
    m_lastInputMs = get_tick_ms();
    m_lastActivityMs = get_tick_ms();
}

DisplayPowerState_t display_getPowerState(void)
{
    return m_displayPowerState;
}

/* ==========================================================================
 * SPLASH SCREEN
 * ========================================================================== */

void display_showSplash(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "AgSys");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, COLOR_FLOW_FWD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);
    
    /* Subtitle */
    lv_obj_t *subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Mag Meter");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(subtitle, COLOR_TEXT, 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 0);
    
    /* Version */
    lv_obj_t *version = lv_label_create(screen);
    lv_label_set_text(version, "v1.0.0");
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version, COLOR_TEXT_LABEL, 0);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, 40);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * MAIN SCREEN
 * ========================================================================== */

void display_showMain(void)
{
    m_currentScreen = SCREEN_MAIN;
    
    /* Create main screen */
    m_screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(m_screen_main, COLOR_BG, 0);
    lv_obj_set_style_pad_all(m_screen_main, 0, 0);
    
    /* Frame dimensions */
    #define FRAME_BORDER 2
    #define FRAME_RADIUS 8
    #define FRAME_PAD 3
    #define CONTENT_WIDTH (DISPLAY_WIDTH - 2 * (FRAME_BORDER + FRAME_PAD))
    #define CONTENT_HEIGHT (DISPLAY_HEIGHT - 2 * (FRAME_BORDER + FRAME_PAD))
    
    /* Outer frame */
    lv_obj_t *frame = lv_obj_create(m_screen_main);
    lv_obj_set_size(frame, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_align(frame, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(frame, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(frame, FRAME_BORDER, 0);
    lv_obj_set_style_border_color(frame, COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(frame, FRAME_RADIUS, 0);
    lv_obj_set_style_pad_all(frame, FRAME_PAD, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Flow section */
    #define FLOW_SECTION_H 95
    
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
    lv_obj_set_style_text_font(m_label_flow_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(m_label_flow_value, COLOR_TEXT, 0);
    lv_obj_align(m_label_flow_value, LV_ALIGN_TOP_MID, -20, 0);
    
    /* Flow unit */
    m_label_flow_unit = lv_label_create(flow_section);
    UnitSystem_t units = m_userSettings ? m_userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    lv_label_set_text(m_label_flow_unit, getFlowUnitStr(units));
    lv_obj_set_style_text_font(m_label_flow_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(m_label_flow_unit, COLOR_TEXT_LABEL, 0);
    lv_obj_align_to(m_label_flow_unit, m_label_flow_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -8);
    
    /* Flow bar */
    lv_obj_t *bar_container = lv_obj_create(flow_section);
    lv_obj_set_size(bar_container, CONTENT_WIDTH - 10, 22);
    lv_obj_align(bar_container, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_bg_color(bar_container, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_border_width(bar_container, 1, 0);
    lv_obj_set_style_border_color(bar_container, COLOR_DIVIDER, 0);
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
    lv_obj_set_style_text_font(m_obj_flow_arrow, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(m_obj_flow_arrow, COLOR_FLOW_IDLE, 0);
    lv_obj_align(m_obj_flow_arrow, LV_ALIGN_RIGHT_MID, -2, 0);
    
    /* "Current Flow Rate" label */
    lv_obj_t *label_current = lv_label_create(flow_section);
    lv_label_set_text(label_current, "Current Flow Rate");
    lv_obj_set_style_text_font(label_current, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_current, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_current, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    /* Divider 1 */
    lv_obj_t *divider1 = lv_obj_create(frame);
    lv_obj_set_size(divider1, CONTENT_WIDTH, 1);
    lv_obj_align(divider1, LV_ALIGN_TOP_MID, 0, FLOW_SECTION_H);
    lv_obj_set_style_bg_color(divider1, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(divider1, 0, 0);
    
    /* Middle section: Trend | Avg */
    #define MID_SECTION_H 70
    #define MID_SECTION_Y (FLOW_SECTION_H + 1)
    
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
    lv_obj_set_style_text_font(m_label_trend_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(m_label_trend_value, COLOR_TEXT, 0);
    lv_obj_align(m_label_trend_value, LV_ALIGN_CENTER, 0, -8);
    
    lv_obj_t *label_trend = lv_label_create(trend_panel);
    lv_label_set_text(label_trend, "Trend");
    lv_obj_set_style_text_font(label_trend, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_trend, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_trend, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    /* Vertical divider */
    lv_obj_t *vdivider = lv_obj_create(frame);
    lv_obj_set_size(vdivider, 1, MID_SECTION_H);
    lv_obj_align(vdivider, LV_ALIGN_TOP_MID, 0, MID_SECTION_Y);
    lv_obj_set_style_bg_color(vdivider, COLOR_DIVIDER, 0);
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
    lv_obj_set_style_text_font(m_label_avg_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(m_label_avg_value, COLOR_TEXT, 0);
    lv_obj_align(m_label_avg_value, LV_ALIGN_CENTER, 0, -8);
    
    lv_obj_t *label_avg = lv_label_create(avg_panel);
    lv_label_set_text(label_avg, "AVG Vol");
    lv_obj_set_style_text_font(label_avg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_avg, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_avg, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    /* Divider 2 */
    #define TOTAL_SECTION_Y (MID_SECTION_Y + MID_SECTION_H)
    
    lv_obj_t *divider2 = lv_obj_create(frame);
    lv_obj_set_size(divider2, CONTENT_WIDTH, 1);
    lv_obj_align(divider2, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y);
    lv_obj_set_style_bg_color(divider2, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(divider2, 0, 0);
    
    /* Total section */
    #define TOTAL_SECTION_H (CONTENT_HEIGHT - TOTAL_SECTION_Y - 1)
    
    m_total_section = lv_obj_create(frame);
    lv_obj_set_size(m_total_section, CONTENT_WIDTH, TOTAL_SECTION_H);
    lv_obj_align(m_total_section, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y + 1);
    lv_obj_set_style_bg_opa(m_total_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_total_section, 0, 0);
    lv_obj_set_style_pad_all(m_total_section, 2, 0);
    lv_obj_clear_flag(m_total_section, LV_OBJ_FLAG_SCROLLABLE);
    
    m_label_total_value = lv_label_create(m_total_section);
    lv_label_set_text(m_label_total_value, "0.0");
    lv_obj_set_style_text_font(m_label_total_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(m_label_total_value, COLOR_TEXT, 0);
    lv_obj_align(m_label_total_value, LV_ALIGN_CENTER, -15, -8);
    
    m_label_total_unit = lv_label_create(m_total_section);
    lv_label_set_text(m_label_total_unit, "L");
    lv_obj_set_style_text_font(m_label_total_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(m_label_total_unit, COLOR_TEXT_LABEL, 0);
    lv_obj_align_to(m_label_total_unit, m_label_total_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 3, -5);
    
    lv_obj_t *label_total = lv_label_create(m_total_section);
    lv_label_set_text(label_total, "Total Vol");
    lv_obj_set_style_text_font(label_total, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_total, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_total, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    /* Alarm overlay (hidden by default) */
    m_alarm_overlay = lv_obj_create(frame);
    lv_obj_set_size(m_alarm_overlay, CONTENT_WIDTH, TOTAL_SECTION_H);
    lv_obj_align(m_alarm_overlay, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y + 1);
    lv_obj_set_style_bg_color(m_alarm_overlay, COLOR_ALARM_WARNING, 0);
    lv_obj_set_style_border_width(m_alarm_overlay, 0, 0);
    lv_obj_set_style_radius(m_alarm_overlay, 0, 0);
    lv_obj_set_style_pad_all(m_alarm_overlay, 4, 0);
    lv_obj_add_flag(m_alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(m_alarm_overlay, LV_OBJ_FLAG_SCROLLABLE);
    
    m_alarm_title_label = lv_label_create(m_alarm_overlay);
    lv_label_set_text(m_alarm_title_label, "");
    lv_obj_set_style_text_font(m_alarm_title_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(m_alarm_title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(m_alarm_title_label, LV_ALIGN_TOP_MID, 0, 2);
    
    m_alarm_detail_label = lv_label_create(m_alarm_overlay);
    lv_label_set_text(m_alarm_detail_label, "");
    lv_obj_set_style_text_font(m_alarm_detail_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_alarm_detail_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(m_alarm_detail_label, LV_ALIGN_CENTER, 0, 2);
    
    m_alarmOverlayActive = false;
    
    /* BLE icon (lower-right corner, small square, hidden by default) */
    #define BLE_ICON_SIZE 24
    m_ble_icon = lv_obj_create(frame);
    lv_obj_set_size(m_ble_icon, BLE_ICON_SIZE, BLE_ICON_SIZE);
    lv_obj_align(m_ble_icon, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_set_style_bg_color(m_ble_icon, COLOR_BLE_ACTIVE, 0);
    lv_obj_set_style_bg_opa(m_ble_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_ble_icon, 0, 0);
    lv_obj_set_style_radius(m_ble_icon, 4, 0);
    lv_obj_set_style_pad_all(m_ble_icon, 0, 0);
    lv_obj_clear_flag(m_ble_icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);  /* Hidden until BLE active */
    
    /* Bluetooth "B" symbol inside icon */
    lv_obj_t *ble_label = lv_label_create(m_ble_icon);
    lv_label_set_text(ble_label, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(ble_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ble_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ble_label, LV_ALIGN_CENTER, 0, 0);
    
    m_bleUiState = BLE_UI_STATE_IDLE;
    m_bleIconVisible = true;
    m_bleFlashLastMs = get_tick_ms();
    
    lv_scr_load(m_screen_main);
}

void display_updateMain(const FlowData_t *data)
{
    if (m_screen_main == NULL || data == NULL) return;
    
    UnitSystem_t units = m_userSettings ? m_userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    float maxFlow = m_userSettings ? m_userSettings->maxFlowLPM : DEFAULT_MAX_FLOW_MM_S;
    
    char valueBuf[32];
    char unitBuf[16];
    
    /* Update flow rate */
    float displayFlow = convertFlowRate(fabsf(data->flowRateLPM), units);
    formatFlowValue(valueBuf, sizeof(valueBuf), displayFlow);
    lv_label_set_text(m_label_flow_value, valueBuf);
    lv_label_set_text(m_label_flow_unit, getFlowUnitStr(units));
    
    /* Update flow bar */
    int barPercent = (int)((fabsf(data->flowRateLPM) / maxFlow) * 100.0f);
    if (barPercent > 100) barPercent = 100;
    lv_bar_set_value(m_obj_flow_bar, barPercent, LV_ANIM_ON);
    
    /* Update flow arrow */
    lv_color_t arrowColor;
    const char *arrowSymbol;
    if (fabsf(data->flowRateLPM) < 0.1f) {
        arrowColor = COLOR_FLOW_IDLE;
        arrowSymbol = LV_SYMBOL_RIGHT;
    } else if (data->reverseFlow) {
        arrowColor = COLOR_FLOW_REV;
        arrowSymbol = LV_SYMBOL_LEFT;
        lv_obj_set_style_bg_color(m_obj_flow_bar, COLOR_FLOW_REV, LV_PART_INDICATOR);
    } else {
        arrowColor = COLOR_FLOW_FWD;
        arrowSymbol = LV_SYMBOL_RIGHT;
        lv_obj_set_style_bg_color(m_obj_flow_bar, COLOR_FLOW_FWD, LV_PART_INDICATOR);
    }
    lv_obj_set_style_text_color(m_obj_flow_arrow, arrowColor, 0);
    lv_label_set_text(m_obj_flow_arrow, arrowSymbol);
    
    /* Update trend */
    formatVolumeWithUnit(valueBuf, sizeof(valueBuf), unitBuf, sizeof(unitBuf),
                         fabsf(data->trendVolumeLiters), units);
    char trendBuf[48];
    snprintf(trendBuf, sizeof(trendBuf), "%s%s%s",
             data->trendVolumeLiters >= 0 ? "+" : "-", valueBuf, unitBuf);
    lv_label_set_text(m_label_trend_value, trendBuf);
    
    /* Update avg */
    formatVolumeWithUnit(valueBuf, sizeof(valueBuf), unitBuf, sizeof(unitBuf),
                         data->avgVolumeLiters, units);
    char avgBuf[48];
    snprintf(avgBuf, sizeof(avgBuf), "%s%s", valueBuf, unitBuf);
    lv_label_set_text(m_label_avg_value, avgBuf);
    
    /* Update total volume */
    formatVolumeWithUnit(valueBuf, sizeof(valueBuf), unitBuf, sizeof(unitBuf),
                         data->totalVolumeLiters, units);
    lv_label_set_text(m_label_total_value, valueBuf);
    lv_label_set_text(m_label_total_unit, unitBuf);
}

/* ==========================================================================
 * ERROR SCREEN
 * ========================================================================== */

void display_showError(const char *message)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFE0E0), 0);
    
    lv_obj_t *icon = lv_label_create(screen);
    lv_label_set_text(icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xCC0000), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -30);
    
    lv_obj_t *msg = lv_label_create(screen);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(msg, COLOR_TEXT, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 20);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * OTA PROGRESS SCREEN
 * ========================================================================== */

static lv_obj_t *m_ota_progress_bar = NULL;
static lv_obj_t *m_ota_status_label = NULL;

void display_showOTAProgress(uint8_t percent, const char *status)
{
    m_currentScreen = SCREEN_OTA_PROGRESS;
    
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Firmware Update");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_FLOW_FWD, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);
    
    /* Progress bar */
    m_ota_progress_bar = lv_bar_create(screen);
    lv_obj_set_size(m_ota_progress_bar, 200, 20);
    lv_obj_align(m_ota_progress_bar, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(m_ota_progress_bar, 0, 100);
    lv_bar_set_value(m_ota_progress_bar, percent, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_ota_progress_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_ota_progress_bar, COLOR_FLOW_FWD, LV_PART_INDICATOR);
    
    /* Percent label */
    lv_obj_t *pct_label = lv_label_create(screen);
    char pct_str[16];
    snprintf(pct_str, sizeof(pct_str), "%d%%", percent);
    lv_label_set_text(pct_label, pct_str);
    lv_obj_set_style_text_font(pct_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(pct_label, COLOR_TEXT, 0);
    lv_obj_align(pct_label, LV_ALIGN_CENTER, 0, -50);
    
    /* Status label */
    m_ota_status_label = lv_label_create(screen);
    lv_label_set_text(m_ota_status_label, status ? status : "Updating...");
    lv_obj_set_style_text_font(m_ota_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_ota_status_label, COLOR_TEXT_LABEL, 0);
    lv_obj_align(m_ota_status_label, LV_ALIGN_CENTER, 0, 40);
    
    /* Warning */
    lv_obj_t *warning = lv_label_create(screen);
    lv_label_set_text(warning, "Do not power off");
    lv_obj_set_style_text_font(warning, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(warning, COLOR_ALARM_WARNING, 0);
    lv_obj_align(warning, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    lv_scr_load(screen);
}

void display_updateOTAProgress(uint8_t percent)
{
    if (m_ota_progress_bar != NULL) {
        lv_bar_set_value(m_ota_progress_bar, percent, LV_ANIM_ON);
    }
}

/* ==========================================================================
 * ALARM OVERLAY
 * ========================================================================== */

void display_showAlarm(AlarmType_t alarmType, uint32_t durationSec,
                       float flowRateLPM, float volumeLiters)
{
    if (m_alarm_overlay == NULL) return;
    
    m_currentAlarmType = alarmType;
    m_alarmOverlayActive = true;
    
    /* Set alarm color based on type */
    lv_color_t alarmColor = (alarmType == ALARM_LEAK || alarmType == ALARM_TAMPER)
                            ? COLOR_ALARM_CRITICAL : COLOR_ALARM_WARNING;
    lv_obj_set_style_bg_color(m_alarm_overlay, alarmColor, 0);
    
    /* Set title */
    const char *title = "";
    switch (alarmType) {
        case ALARM_LEAK:        title = "! LEAK"; break;
        case ALARM_REVERSE_FLOW: title = "R REVERSE"; break;
        case ALARM_HIGH_FLOW:   title = "! HIGH FLOW"; break;
        case ALARM_TAMPER:      title = "! TAMPER"; break;
        default:                title = "! ALARM"; break;
    }
    lv_label_set_text(m_alarm_title_label, title);
    
    /* Set detail */
    char detail[64];
    uint32_t hours = durationSec / 3600;
    uint32_t mins = (durationSec % 3600) / 60;
    snprintf(detail, sizeof(detail), "Duration: %luh %lum\nFlow: %.1f L/min",
             hours, mins, flowRateLPM);
    lv_label_set_text(m_alarm_detail_label, detail);
    
    /* Show overlay, hide total section */
    lv_obj_add_flag(m_total_section, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(m_alarm_overlay, LV_OBJ_FLAG_HIDDEN);
}

void display_acknowledgeAlarm(void)
{
    m_alarmOverlayActive = false;
    lv_obj_add_flag(m_alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(m_total_section, LV_OBJ_FLAG_HIDDEN);
}

void display_dismissAlarm(void)
{
    /* Hide overlay but alarm still active */
    lv_obj_add_flag(m_alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(m_total_section, LV_OBJ_FLAG_HIDDEN);
}

bool display_isAlarmActive(void)
{
    return m_alarmOverlayActive;
}

/* ==========================================================================
 * SETTINGS AND UTILITY FUNCTIONS
 * ========================================================================== */

void display_setSettings(UserSettings_t *settings)
{
    m_userSettings = settings;
}

ScreenId_t display_getCurrentScreen(void)
{
    return m_currentScreen;
}

bool display_isMenuLocked(void)
{
    return m_menuLockState == MENU_LOCKED;
}

void display_lockMenu(void)
{
    m_menuLockState = MENU_LOCKED;
}

void display_unlockMenuRemote(void)
{
    m_menuLockState = MENU_UNLOCKED_REMOTE;
    m_lastActivityMs = get_tick_ms();
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

void display_handleButton(ButtonEvent_t event)
{
    /* Wake display on any button press */
    if (m_displayPowerState == DISPLAY_SLEEP) {
        display_wake();
        return;
    }
    
    if (m_displayPowerState == DISPLAY_DIM) {
        display_wake();
    }
    
    display_resetActivityTimer();
    
    /* Handle based on current screen */
    switch (m_currentScreen) {
        case SCREEN_MAIN:
            if (m_alarmOverlayActive) {
                if (event == BTN_SELECT_SHORT) {
                    display_acknowledgeAlarm();
                } else if (event == BTN_LEFT_SHORT) {
                    display_dismissAlarm();
                }
            } else {
                /* Long press to enter menu */
                if (event == BTN_SELECT_LONG) {
                    if (display_isMenuLocked() && m_userSettings && m_userSettings->menuLockEnabled) {
                        display_showMenuLocked();
                    } else {
                        display_showMenu();
                    }
                }
            }
            break;
            
        case SCREEN_MENU:
            if (event == BTN_UP_SHORT) {
                if (m_menuSelection > 0) {
                    m_menuSelection--;
                    menu_refresh();
                }
            } else if (event == BTN_DOWN_SHORT) {
                if (m_menuSelection < m_menuItemCount - 1) {
                    m_menuSelection++;
                    menu_refresh();
                }
            } else if (event == BTN_SELECT_SHORT || event == BTN_RIGHT_SHORT) {
                /* Enter selected submenu */
                switch (m_menuSelection) {
                    case 0: display_showDisplaySettings(); break;
                    case 1: display_showFlowSettings(); break;
                    case 2: display_showAlarmSettings(); break;
                    case 3: display_showLoRaConfig(); break;
                    case 4: display_showCalibration(); break;
                    case 5: display_showDiagnostics(); break;
                    case 6: display_showAbout(); break;
                }
            } else if (event == BTN_LEFT_SHORT) {
                display_showMain();
            }
            break;
            
        case SCREEN_MENU_LOCKED:
            if (event == BTN_UP_SHORT) {
                m_enteredPin[m_pinDigitIndex] = (m_enteredPin[m_pinDigitIndex] + 1) % 10;
                pin_update_display();
            } else if (event == BTN_DOWN_SHORT) {
                m_enteredPin[m_pinDigitIndex] = (m_enteredPin[m_pinDigitIndex] + 9) % 10;
                pin_update_display();
            } else if (event == BTN_RIGHT_SHORT || event == BTN_SELECT_SHORT) {
                if (m_pinDigitIndex < 5) {
                    m_pinDigitIndex++;
                    pin_update_display();
                } else {
                    /* All digits entered - verify */
                    if (pin_verify()) {
                        m_menuLockState = MENU_UNLOCKED_PIN;
                        display_showMenu();
                    } else {
                        /* Wrong PIN - reset */
                        m_pinDigitIndex = 0;
                        memset(m_enteredPin, 0, sizeof(m_enteredPin));
                        pin_update_display();
                    }
                }
            } else if (event == BTN_LEFT_SHORT) {
                if (m_pinDigitIndex > 0) {
                    m_pinDigitIndex--;
                    pin_update_display();
                } else {
                    display_showMain();
                }
            }
            break;
            
        case SCREEN_DISPLAY_SETTINGS:
        case SCREEN_FLOW_SETTINGS:
        case SCREEN_ALARM_SETTINGS:
        case SCREEN_LORA_CONFIG:
        case SCREEN_CALIBRATION:
        case SCREEN_DIAGNOSTICS:
            if (event == BTN_UP_SHORT) {
                if (m_submenuSelection > 0) {
                    m_submenuSelection--;
                    submenu_refresh();
                }
            } else if (event == BTN_DOWN_SHORT) {
                if (m_submenuSelection < m_submenuItemCount - 1) {
                    m_submenuSelection++;
                    submenu_refresh();
                }
            } else if (event == BTN_SELECT_SHORT || event == BTN_RIGHT_SHORT) {
                /* Handle submenu item selection */
                if (m_currentScreen == SCREEN_DISPLAY_SETTINGS) {
                    /* Units, Trend, Avg - cycle through values */
                    if (m_submenuSelection == 0 && m_userSettings) {
                        m_userSettings->unitSystem = (m_userSettings->unitSystem + 1) % 3;
                        display_showDisplaySettings();
                    }
                } else if (m_currentScreen == SCREEN_DIAGNOSTICS) {
                    if (m_submenuSelection == 0) {
                        display_showDiagLoRa(&m_loraStats);
                    } else if (m_submenuSelection == 1) {
                        display_showDiagADC(&m_adcValues);
                    }
                } else if (m_currentScreen == SCREEN_LORA_CONFIG) {
                    if (m_submenuSelection == 1 && m_userSettings) {
                        /* Cycle spreading factor 7-12 */
                        m_userSettings->loraSpreadingFactor++;
                        if (m_userSettings->loraSpreadingFactor > 12) {
                            m_userSettings->loraSpreadingFactor = 7;
                        }
                        display_showLoRaConfig();
                    }
                }
            } else if (event == BTN_LEFT_SHORT) {
                display_showMenu();
            }
            break;
            
        case SCREEN_DIAG_LORA:
        case SCREEN_DIAG_ADC:
            if (event == BTN_LEFT_SHORT) {
                display_showDiagnostics();
            }
            break;
            
        case SCREEN_ABOUT:
            if (event == BTN_LEFT_SHORT) {
                display_showMenu();
            }
            break;
            
        case SCREEN_TOTALIZER:
            if (event == BTN_LEFT_SHORT) {
                display_showCalibration();
            }
            break;
            
        default:
            /* Other screens - back to main on LEFT */
            if (event == BTN_LEFT_SHORT) {
                display_showMain();
            }
            break;
    }
}

/* Submenu refresh helper */
static void submenu_refresh(void)
{
    switch (m_currentScreen) {
        case SCREEN_DISPLAY_SETTINGS:
            display_showDisplaySettings();
            break;
        case SCREEN_FLOW_SETTINGS:
            display_showFlowSettings();
            break;
        case SCREEN_ALARM_SETTINGS:
            display_showAlarmSettings();
            break;
        case SCREEN_LORA_CONFIG:
            display_showLoRaConfig();
            break;
        case SCREEN_CALIBRATION:
            display_showCalibration();
            break;
        case SCREEN_DIAGNOSTICS:
            display_showDiagnostics();
            break;
        default:
            break;
    }
}

/* ==========================================================================
 * MENU HELPER FUNCTIONS
 * ========================================================================== */

static lv_obj_t *create_menu_screen(const char *title)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 5, 0);
    
    /* Title bar */
    lv_obj_t *title_label = lv_label_create(screen);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, COLOR_FLOW_FWD, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 2);
    
    /* Divider */
    lv_obj_t *divider = lv_obj_create(screen);
    lv_obj_set_size(divider, DISPLAY_WIDTH - 10, 2);
    lv_obj_align(divider, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_color(divider, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    
    return screen;
}

static lv_obj_t *create_menu_list(lv_obj_t *parent)
{
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, DISPLAY_WIDTH - 10, DISPLAY_HEIGHT - 60);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    return list;
}

static lv_obj_t *add_menu_item(lv_obj_t *list, const char *text, int8_t index, int8_t selected)
{
    lv_obj_t *item = lv_obj_create(list);
    lv_obj_set_size(item, DISPLAY_WIDTH - 20, 32);
    lv_obj_set_style_pad_left(item, 10, 0);
    lv_obj_set_style_pad_right(item, 10, 0);
    lv_obj_set_style_radius(item, 4, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    
    if (index == selected) {
        lv_obj_set_style_bg_color(item, COLOR_FLOW_FWD, 0);
        lv_obj_set_style_border_width(item, 0, 0);
    } else {
        lv_obj_set_style_bg_color(item, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_color(item, COLOR_DIVIDER, 0);
    }
    
    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, index == selected ? lv_color_hex(0xFFFFFF) : COLOR_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    
    /* Arrow for submenus */
    lv_obj_t *arrow = lv_label_create(item);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(arrow, index == selected ? lv_color_hex(0xFFFFFF) : COLOR_TEXT_LABEL, 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 0, 0);
    
    return item;
}

static lv_obj_t *add_menu_item_value(lv_obj_t *list, const char *text, const char *value, int8_t index, int8_t selected)
{
    lv_obj_t *item = lv_obj_create(list);
    lv_obj_set_size(item, DISPLAY_WIDTH - 20, 32);
    lv_obj_set_style_pad_left(item, 10, 0);
    lv_obj_set_style_pad_right(item, 10, 0);
    lv_obj_set_style_radius(item, 4, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    
    if (index == selected) {
        lv_obj_set_style_bg_color(item, COLOR_FLOW_FWD, 0);
        lv_obj_set_style_border_width(item, 0, 0);
    } else {
        lv_obj_set_style_bg_color(item, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_color(item, COLOR_DIVIDER, 0);
    }
    
    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, index == selected ? lv_color_hex(0xFFFFFF) : COLOR_TEXT, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    
    lv_obj_t *val_label = lv_label_create(item);
    lv_label_set_text(val_label, value);
    lv_obj_set_style_text_font(val_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(val_label, index == selected ? lv_color_hex(0xFFFFFF) : COLOR_TEXT_LABEL, 0);
    lv_obj_align(val_label, LV_ALIGN_RIGHT_MID, 0, 0);
    
    return item;
}

/* ==========================================================================
 * MAIN MENU
 * ========================================================================== */

#define MAIN_MENU_ITEMS 7

static const char *main_menu_items[] = {
    "Display Settings",
    "Flow Settings",
    "Alarm Settings",
    "LoRa Config",
    "Calibration",
    "Diagnostics",
    "About"
};

void display_showMenu(void)
{
    m_currentScreen = SCREEN_MENU;
    m_menuSelection = 0;
    m_menuItemCount = MAIN_MENU_ITEMS;
    
    m_screen_menu = create_menu_screen("Settings");
    m_menu_list = create_menu_list(m_screen_menu);
    
    for (int i = 0; i < MAIN_MENU_ITEMS; i++) {
        add_menu_item(m_menu_list, main_menu_items[i], i, m_menuSelection);
    }
    
    /* Back hint */
    lv_obj_t *hint = lv_label_create(m_screen_menu);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back    " LV_SYMBOL_OK " Select");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(m_screen_menu);
}

static void menu_refresh(void)
{
    if (m_menu_list == NULL) return;
    lv_obj_clean(m_menu_list);
    for (int i = 0; i < MAIN_MENU_ITEMS; i++) {
        add_menu_item(m_menu_list, main_menu_items[i], i, m_menuSelection);
    }
}

/* ==========================================================================
 * PIN ENTRY SCREEN
 * ========================================================================== */

void display_showMenuLocked(void)
{
    m_currentScreen = SCREEN_MENU_LOCKED;
    m_pinDigitIndex = 0;
    memset(m_enteredPin, 0, sizeof(m_enteredPin));
    
    m_screen_pin = create_menu_screen("Enter PIN");
    
    /* PIN digit boxes */
    lv_obj_t *pin_container = lv_obj_create(m_screen_pin);
    lv_obj_set_size(pin_container, 200, 50);
    lv_obj_align(pin_container, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_opa(pin_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pin_container, 0, 0);
    lv_obj_set_flex_flow(pin_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pin_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pin_container, LV_OBJ_FLAG_SCROLLABLE);
    
    for (int i = 0; i < 6; i++) {
        lv_obj_t *box = lv_obj_create(pin_container);
        lv_obj_set_size(box, 28, 40);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_border_color(box, i == 0 ? COLOR_FLOW_FWD : COLOR_DIVIDER, 0);
        lv_obj_set_style_bg_color(box, COLOR_PANEL_BG, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        
        m_pin_digits[i] = lv_label_create(box);
        lv_label_set_text(m_pin_digits[i], "-");
        lv_obj_set_style_text_font(m_pin_digits[i], &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(m_pin_digits[i], COLOR_TEXT, 0);
        lv_obj_center(m_pin_digits[i]);
    }
    
    /* Instructions */
    lv_obj_t *instr = lv_label_create(m_screen_pin);
    lv_label_set_text(instr, LV_SYMBOL_UP "/" LV_SYMBOL_DOWN " Change   " LV_SYMBOL_RIGHT " Next");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(instr, COLOR_TEXT_LABEL, 0);
    lv_obj_align(instr, LV_ALIGN_CENTER, 0, 40);
    
    /* Back hint */
    lv_obj_t *hint = lv_label_create(m_screen_pin);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Cancel");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(m_screen_pin);
}

static void pin_update_display(void)
{
    for (int i = 0; i < 6; i++) {
        if (m_pin_digits[i] == NULL) continue;
        
        char buf[4];
        if (i < m_pinDigitIndex || (i == m_pinDigitIndex && m_enteredPin[i] > 0)) {
            snprintf(buf, sizeof(buf), "%d", m_enteredPin[i]);
        } else {
            snprintf(buf, sizeof(buf), "-");
        }
        lv_label_set_text(m_pin_digits[i], buf);
        
        /* Update border color for current digit */
        lv_obj_t *parent = lv_obj_get_parent(m_pin_digits[i]);
        if (parent) {
            lv_obj_set_style_border_color(parent, i == m_pinDigitIndex ? COLOR_FLOW_FWD : COLOR_DIVIDER, 0);
        }
    }
}

static bool pin_verify(void)
{
    if (m_userSettings == NULL) return true;
    
    uint32_t entered = 0;
    for (int i = 0; i < 6; i++) {
        entered = entered * 10 + m_enteredPin[i];
    }
    return entered == m_userSettings->menuPin;
}

/* ==========================================================================
 * PIN OVERLAY (on main screen)
 * ========================================================================== */

void display_showPinOverlay(void)
{
    if (m_screen_main == NULL) return;
    
    m_pinDigitIndex = 0;
    memset(m_enteredPin, 0, sizeof(m_enteredPin));
    
    m_pin_overlay = lv_obj_create(m_screen_main);
    lv_obj_set_size(m_pin_overlay, DISPLAY_WIDTH - 20, 120);
    lv_obj_align(m_pin_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(m_pin_overlay, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(m_pin_overlay, 2, 0);
    lv_obj_set_style_border_color(m_pin_overlay, COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(m_pin_overlay, 8, 0);
    lv_obj_clear_flag(m_pin_overlay, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *title = lv_label_create(m_pin_overlay);
    lv_label_set_text(title, "Enter PIN");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, COLOR_FLOW_FWD, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    /* PIN boxes */
    lv_obj_t *pin_row = lv_obj_create(m_pin_overlay);
    lv_obj_set_size(pin_row, 180, 40);
    lv_obj_align(pin_row, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(pin_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pin_row, 0, 0);
    lv_obj_set_flex_flow(pin_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pin_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pin_row, LV_OBJ_FLAG_SCROLLABLE);
    
    for (int i = 0; i < 6; i++) {
        lv_obj_t *box = lv_obj_create(pin_row);
        lv_obj_set_size(box, 26, 34);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_border_color(box, i == 0 ? COLOR_FLOW_FWD : COLOR_DIVIDER, 0);
        lv_obj_set_style_bg_color(box, lv_color_hex(0xFFFFFF), 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        
        m_pin_overlay_digits[i] = lv_label_create(box);
        lv_label_set_text(m_pin_overlay_digits[i], "-");
        lv_obj_set_style_text_font(m_pin_overlay_digits[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(m_pin_overlay_digits[i], COLOR_TEXT, 0);
        lv_obj_center(m_pin_overlay_digits[i]);
    }
    
    lv_obj_t *hint = lv_label_create(m_pin_overlay);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Cancel");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
}

void display_hidePinOverlay(void)
{
    if (m_pin_overlay != NULL) {
        lv_obj_del(m_pin_overlay);
        m_pin_overlay = NULL;
        for (int i = 0; i < 6; i++) {
            m_pin_overlay_digits[i] = NULL;
        }
    }
}

/* ==========================================================================
 * DISPLAY SETTINGS SUBMENU
 * ========================================================================== */

void display_showDisplaySettings(void)
{
    m_currentScreen = SCREEN_DISPLAY_SETTINGS;
    m_submenuSelection = 0;
    m_submenuItemCount = 3;
    
    m_screen_submenu = create_menu_screen("Display");
    m_submenu_list = create_menu_list(m_screen_submenu);
    
    const char *unitStr = "Metric";
    if (m_userSettings) {
        switch (m_userSettings->unitSystem) {
            case UNIT_SYSTEM_IMPERIAL: unitStr = "Imperial"; break;
            case UNIT_SYSTEM_IMPERIAL_AG: unitStr = "Imperial-Ag"; break;
            default: unitStr = "Metric"; break;
        }
    }
    
    char trendBuf[16], avgBuf[16];
    snprintf(trendBuf, sizeof(trendBuf), "%d min", m_userSettings ? m_userSettings->trendPeriodMin : 1);
    snprintf(avgBuf, sizeof(avgBuf), "%d min", m_userSettings ? m_userSettings->avgPeriodMin : 30);
    
    add_menu_item_value(m_submenu_list, "Units", unitStr, 0, m_submenuSelection);
    add_menu_item_value(m_submenu_list, "Trend Period", trendBuf, 1, m_submenuSelection);
    add_menu_item_value(m_submenu_list, "Avg Period", avgBuf, 2, m_submenuSelection);
    
    lv_obj_t *hint = lv_label_create(m_screen_submenu);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back    " LV_SYMBOL_OK " Edit");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(m_screen_submenu);
}

/* ==========================================================================
 * FLOW SETTINGS SUBMENU
 * ========================================================================== */

void display_showFlowSettings(void)
{
    m_currentScreen = SCREEN_FLOW_SETTINGS;
    m_submenuSelection = 0;
    m_submenuItemCount = 1;
    
    m_screen_submenu = create_menu_screen("Flow");
    m_submenu_list = create_menu_list(m_screen_submenu);
    
    char maxFlowBuf[16];
    snprintf(maxFlowBuf, sizeof(maxFlowBuf), "%.0f L/min", m_userSettings ? m_userSettings->maxFlowLPM : 100.0f);
    
    add_menu_item_value(m_submenu_list, "Max Flow", maxFlowBuf, 0, m_submenuSelection);
    
    lv_obj_t *hint = lv_label_create(m_screen_submenu);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back    " LV_SYMBOL_OK " Edit");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(m_screen_submenu);
}

/* ==========================================================================
 * ALARM SETTINGS SUBMENU
 * ========================================================================== */

void display_showAlarmSettings(void)
{
    m_currentScreen = SCREEN_ALARM_SETTINGS;
    m_submenuSelection = 0;
    m_submenuItemCount = 3;
    
    m_screen_submenu = create_menu_screen("Alarms");
    m_submenu_list = create_menu_list(m_screen_submenu);
    
    char leakThreshBuf[16], leakDurBuf[16], highFlowBuf[16];
    float leakThresh = m_userSettings ? (m_userSettings->alarmLeakThresholdLPM10 / 10.0f) : 2.0f;
    snprintf(leakThreshBuf, sizeof(leakThreshBuf), "%.1f L/min", leakThresh);
    snprintf(leakDurBuf, sizeof(leakDurBuf), "%d min", m_userSettings ? m_userSettings->alarmLeakDurationMin : 60);
    snprintf(highFlowBuf, sizeof(highFlowBuf), "%d L/min", m_userSettings ? m_userSettings->alarmHighFlowLPM : 150);
    
    add_menu_item_value(m_submenu_list, "Leak Threshold", leakThreshBuf, 0, m_submenuSelection);
    add_menu_item_value(m_submenu_list, "Leak Duration", leakDurBuf, 1, m_submenuSelection);
    add_menu_item_value(m_submenu_list, "High Flow", highFlowBuf, 2, m_submenuSelection);
    
    lv_obj_t *hint = lv_label_create(m_screen_submenu);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back    " LV_SYMBOL_OK " Edit");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(m_screen_submenu);
}

/* ==========================================================================
 * LORA CONFIG SUBMENU
 * ========================================================================== */

void display_showLoRaConfig(void)
{
    m_currentScreen = SCREEN_LORA_CONFIG;
    m_submenuSelection = 0;
    m_submenuItemCount = 3;
    
    m_screen_submenu = create_menu_screen("LoRa");
    m_submenu_list = create_menu_list(m_screen_submenu);
    
    char intervalBuf[16], sfBuf[16];
    snprintf(intervalBuf, sizeof(intervalBuf), "%d sec", m_userSettings ? m_userSettings->loraReportIntervalSec : 60);
    snprintf(sfBuf, sizeof(sfBuf), "SF%d", m_userSettings ? m_userSettings->loraSpreadingFactor : 7);
    
    add_menu_item_value(m_submenu_list, "Report Interval", intervalBuf, 0, m_submenuSelection);
    add_menu_item_value(m_submenu_list, "Spreading Factor", sfBuf, 1, m_submenuSelection);
    add_menu_item(m_submenu_list, "Send Test Ping", 2, m_submenuSelection);
    
    lv_obj_t *hint = lv_label_create(m_screen_submenu);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back    " LV_SYMBOL_OK " Edit");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(m_screen_submenu);
}

/* ==========================================================================
 * CALIBRATION SUBMENU
 * ========================================================================== */

void display_showCalibration(void)
{
    m_currentScreen = SCREEN_CALIBRATION;
    m_submenuSelection = 0;
    m_submenuItemCount = 2;
    
    m_screen_submenu = create_menu_screen("Calibration");
    m_submenu_list = create_menu_list(m_screen_submenu);
    
    add_menu_item(m_submenu_list, "Zero Calibration", 0, m_submenuSelection);
    add_menu_item(m_submenu_list, "Reset Totalizer", 1, m_submenuSelection);
    
    lv_obj_t *hint = lv_label_create(m_screen_submenu);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back    " LV_SYMBOL_OK " Select");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(m_screen_submenu);
}

/* ==========================================================================
 * TOTALIZER SCREEN
 * ========================================================================== */

void display_showTotalizer(float totalLiters)
{
    m_currentScreen = SCREEN_TOTALIZER;
    m_totalLiters = totalLiters;
    
    lv_obj_t *screen = create_menu_screen("Totalizer");
    
    char valueBuf[32], unitBuf[16];
    UnitSystem_t units = m_userSettings ? m_userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    formatVolumeWithUnit(valueBuf, sizeof(valueBuf), unitBuf, sizeof(unitBuf), totalLiters, units);
    
    lv_obj_t *value = lv_label_create(screen);
    char fullBuf[48];
    snprintf(fullBuf, sizeof(fullBuf), "%s %s", valueBuf, unitBuf);
    lv_label_set_text(value, fullBuf);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(value, COLOR_TEXT, 0);
    lv_obj_align(value, LV_ALIGN_CENTER, 0, -20);
    
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Total Volume");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);
    
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back    " LV_SYMBOL_OK " Reset");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * DIAGNOSTICS SUBMENU
 * ========================================================================== */

void display_showDiagnostics(void)
{
    m_currentScreen = SCREEN_DIAGNOSTICS;
    m_submenuSelection = 0;
    m_submenuItemCount = 2;
    
    m_screen_submenu = create_menu_screen("Diagnostics");
    m_submenu_list = create_menu_list(m_screen_submenu);
    
    add_menu_item(m_submenu_list, "LoRa Status", 0, m_submenuSelection);
    add_menu_item(m_submenu_list, "ADC Values", 1, m_submenuSelection);
    
    lv_obj_t *hint = lv_label_create(m_screen_submenu);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back    " LV_SYMBOL_OK " View");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(m_screen_submenu);
}

/* ==========================================================================
 * LORA DIAGNOSTICS SCREEN
 * ========================================================================== */

void display_showDiagLoRa(const LoRaStats_t *stats)
{
    m_currentScreen = SCREEN_DIAG_LORA;
    if (stats) m_loraStats = *stats;
    
    lv_obj_t *screen = create_menu_screen("LoRa Status");
    
    char buf[128];
    snprintf(buf, sizeof(buf),
        "Connected: %s\n"
        "Last TX: %lu sec ago\n"
        "Last RX: %lu sec ago\n"
        "TX Count: %lu\n"
        "RX Count: %lu\n"
        "Errors: %lu\n"
        "RSSI: %d dBm\n"
        "SNR: %.1f dB",
        m_loraStats.connected ? "Yes" : "No",
        m_loraStats.lastTxSec,
        m_loraStats.lastRxSec,
        m_loraStats.txCount,
        m_loraStats.rxCount,
        m_loraStats.errorCount,
        m_loraStats.rssi,
        m_loraStats.snr);
    
    lv_obj_t *info = lv_label_create(screen);
    lv_label_set_text(info, buf);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(info, COLOR_TEXT, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 10, 35);
    
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * ADC DIAGNOSTICS SCREEN
 * ========================================================================== */

void display_showDiagADC(const ADCValues_t *values)
{
    m_currentScreen = SCREEN_DIAG_ADC;
    if (values) m_adcValues = *values;
    
    lv_obj_t *screen = create_menu_screen("ADC Values");
    
    char buf[160];
    snprintf(buf, sizeof(buf),
        "CH1 Raw: %ld\n"
        "CH2 Raw: %ld\n"
        "Diff: %ld\n"
        "Temp: %.1f C\n"
        "Zero: %ld\n"
        "Span: %.4f\n"
        "Flow Raw: %.2f\n"
        "Flow Cal: %.2f L/min",
        m_adcValues.ch1Raw,
        m_adcValues.ch2Raw,
        m_adcValues.diffRaw,
        m_adcValues.temperatureC,
        m_adcValues.zeroOffset,
        m_adcValues.spanFactor,
        m_adcValues.flowRaw,
        m_adcValues.flowCal);
    
    lv_obj_t *info = lv_label_create(screen);
    lv_label_set_text(info, buf);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(info, COLOR_TEXT, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 10, 35);
    
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * ABOUT SCREEN
 * ========================================================================== */

void display_showAbout(void)
{
    m_currentScreen = SCREEN_ABOUT;
    
    lv_obj_t *screen = create_menu_screen("About");
    
    lv_obj_t *logo = lv_label_create(screen);
    lv_label_set_text(logo, "AgSys");
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(logo, COLOR_FLOW_FWD, 0);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -50);
    
    lv_obj_t *model = lv_label_create(screen);
    lv_label_set_text(model, "Mag Meter");
    lv_obj_set_style_text_font(model, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(model, COLOR_TEXT, 0);
    lv_obj_align(model, LV_ALIGN_CENTER, 0, -15);
    
    lv_obj_t *version = lv_label_create(screen);
    lv_label_set_text(version, "Firmware: v1.0.0");
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version, COLOR_TEXT_LABEL, 0);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, 15);
    
    lv_obj_t *hw = lv_label_create(screen);
    lv_label_set_text(hw, "Hardware: nRF52840");
    lv_obj_set_style_text_font(hw, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hw, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hw, LV_ALIGN_CENTER, 0, 35);
    
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * STATUS BAR UPDATE
 * ========================================================================== */

void display_updateStatusBar(bool loraConnected, bool hasAlarm,
                             AlarmType_t alarmType, uint32_t lastReportSec)
{
    /* Status bar is integrated into main screen - update indicators */
    (void)loraConnected;
    (void)hasAlarm;
    (void)alarmType;
    (void)lastReportSec;
}

/* ==========================================================================
 * BLE ICON UPDATE
 * ========================================================================== */

void display_updateBleStatus(BleUiState_t state)
{
    m_bleUiState = state;
    
    if (m_ble_icon == NULL) return;
    
    /* Show/hide based on state */
    if (state == BLE_UI_STATE_IDLE) {
        lv_obj_add_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
        m_bleIconVisible = true;
        m_bleFlashLastMs = get_tick_ms();
        m_bleFlashCount = 0;  /* Reset flash counter */
    }
}

BleUiState_t display_getBleStatus(void)
{
    return m_bleUiState;
}

void display_tickBleIcon(void)
{
    if (m_ble_icon == NULL || m_bleUiState == BLE_UI_STATE_IDLE) return;
    
    uint32_t now = get_tick_ms();
    uint32_t elapsed = now - m_bleFlashLastMs;
    uint32_t flash_period_ms;
    
    /* Flash rate depends on state (matches LED patterns) */
    switch (m_bleUiState) {
        case BLE_UI_STATE_ADVERTISING:
            flash_period_ms = 500;  /* Slow blink: 1Hz */
            break;
        case BLE_UI_STATE_CONNECTED:
            flash_period_ms = 250;  /* Fast blink: 2Hz */
            break;
        case BLE_UI_STATE_AUTHENTICATED:
            /* Solid on - no flashing */
            if (lv_obj_has_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
            }
            return;
        case BLE_UI_STATE_DISCONNECTED:
            /* Triple flash (3 on/off cycles) then return to idle */
            flash_period_ms = 100;
            if (m_bleFlashCount >= 6) {  /* 3 on + 3 off = 6 toggles */
                m_bleUiState = BLE_UI_STATE_IDLE;
                lv_obj_add_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
                return;
            }
            break;
        default:
            return;
    }
    
    /* Toggle visibility on flash period */
    if (elapsed >= flash_period_ms) {
        m_bleFlashLastMs = now;
        m_bleIconVisible = !m_bleIconVisible;
        m_bleFlashCount++;
        
        if (m_bleIconVisible) {
            lv_obj_clear_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
