/**
 * @file display.cpp
 * @brief Display implementation for Mag Meter using LVGL and ST7789
 * 
 * Light theme optimized for transflective display daylight readability.
 * Layout based on user mockup with flow rate, trend, avg, and total volume.
 */

#include "display.h"
#include "magmeter_config.h"
#include "settings.h"
#include "calibration.h"
#include <TFT_eSPI.h>

// Display driver
static TFT_eSPI tft = TFT_eSPI();

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_WIDTH * 20];  // 20 lines buffer

// LVGL display driver
static lv_disp_drv_t disp_drv;

// Current screen
static ScreenId_t currentScreen = SCREEN_MAIN;

// User settings pointer
static UserSettings_t* userSettings = NULL;

// Color definitions (light theme for daylight readability)
#define COLOR_BG            lv_color_hex(0xE0E0E0)  // Light gray background
#define COLOR_TEXT          lv_color_hex(0x202020)  // Dark text
#define COLOR_TEXT_LABEL    lv_color_hex(0x606060)  // Medium gray labels
#define COLOR_DIVIDER       lv_color_hex(0x808080)  // Divider lines
#define COLOR_FLOW_FWD      lv_color_hex(0x0066CC)  // Blue - forward flow
#define COLOR_FLOW_REV      lv_color_hex(0xFF6600)  // Orange - reverse flow
#define COLOR_FLOW_IDLE     lv_color_hex(0x909090)  // Gray - no flow
#define COLOR_BAR_BG        lv_color_hex(0xC0C0C0)  // Bar background
#define COLOR_PANEL_BG      lv_color_hex(0xF0F0F0)  // Panel background

// Main screen UI elements
static lv_obj_t* screen_main = NULL;
static lv_obj_t* label_flow_value = NULL;
static lv_obj_t* label_flow_unit = NULL;
static lv_obj_t* obj_flow_bar = NULL;
static lv_obj_t* obj_flow_arrow = NULL;
static lv_obj_t* label_trend_value = NULL;
static lv_obj_t* label_avg_value = NULL;
static lv_obj_t* label_total_value = NULL;
static lv_obj_t* label_total_unit = NULL;

// Bottom section elements (for alarm overlay)
static lv_obj_t* total_section = NULL;
static lv_obj_t* alarm_overlay = NULL;
static lv_obj_t* alarm_title_label = NULL;
static lv_obj_t* alarm_detail_label = NULL;
static lv_obj_t* alarm_hint_label = NULL;
static bool alarmOverlayActive = false;
static AlarmType_t currentAlarmType = ALARM_CLEARED;

// Menu lock state
static MenuLockState_t menuLockState = MENU_LOCKED;
static uint32_t lastActivityMs = 0;
static uint16_t enteredPin[4] = {0, 0, 0, 0};
static int8_t pinDigitIndex = 0;
static lv_obj_t* pinDigitLabels[4] = {NULL};

// Display power state
static DisplayPowerState_t displayPowerState = DISPLAY_ACTIVE;
static uint32_t lastInputMs = 0;           // Last button press time
static bool pinOverlayMode = false;        // True when PIN overlay is on main screen

// Menu screen elements
static lv_obj_t* screen_menu = NULL;
static int8_t menuSelection = 0;

// Tier names
static const char* TIER_NAMES[] = {"MM-S", "MM-M", "MM-L"};

// Alarm colors (needed early for alarm overlay)
#define COLOR_ALARM_CRITICAL    lv_color_hex(0xCC0000)
#define COLOR_ALARM_WARNING     lv_color_hex(0xCC6600)

// Forward declarations for static functions
static void updateMenuHighlight(void);
static void updateSettingsUnitsDisplay(void);
static void updateSettingsTrendDisplay(void);
static void updateSettingsAvgDisplay(void);
static void updateSettingsMaxFlowDisplay(void);
static void createSettingsScreen(const char* title, const char* hint);
static void updateCalMenuHighlight(void);
static void updateCalSpanDisplay(void);
static void setBacklightBrightness(uint8_t percent);
static void updatePinDisplay(void);
static bool checkPin(void);

// Calibration screen elements
static lv_obj_t* calLabels[3] = {NULL};
static int8_t calMenuSelection = 0;
static float calSpanValue = 1.0f;
static lv_obj_t* calValueLabel = NULL;

// Calibration callbacks (implemented in calibration.cpp)
extern void calibration_captureZero(void);
extern void calibration_setSpan(float span);

// Diagnostics data getters (implemented in main.cpp)
extern void getLoRaStats(LoRaStats_t* stats);
extern void getADCValues(ADCValues_t* values);

// LoRa ping function (implemented in main.cpp)
extern bool sendLoRaPing(void);

/* ==========================================================================
 * LVGL DISPLAY FLUSH CALLBACK
 * ========================================================================== */

void display_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)color_p, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

void display_init(void) {
    // Initialize TFT
    tft.init();
    tft.setRotation(DISP_ROTATION);
    tft.fillScreen(TFT_BLACK);
    
    // Initialize LVGL
    lv_init();
    
    // Initialize display buffer
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DISP_WIDTH * 20);
    
    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_WIDTH;
    disp_drv.ver_res = DISP_HEIGHT;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Initialize power state timers
    lastInputMs = millis();
    lastActivityMs = millis();
    displayPowerState = DISPLAY_ACTIVE;
}

/* ==========================================================================
 * DISPLAY POWER MANAGEMENT
 * ========================================================================== */

static void setBacklightBrightness(uint8_t percent) {
    // For now, simple on/off control via digital pin
    // TODO: Implement PWM for dimming if hardware supports it
    if (percent == 0) {
        digitalWrite(PIN_DISP_BL_EN, LOW);
    } else {
        digitalWrite(PIN_DISP_BL_EN, HIGH);
        // Future: analogWrite(PIN_DISP_BL_EN, map(percent, 0, 100, 0, 255));
    }
}

void display_updatePowerState(void) {
    uint32_t now = millis();
    uint32_t idleMs = now - lastInputMs;
    
    // Don't sleep if alarm is active
    if (alarmOverlayActive) {
        if (displayPowerState != DISPLAY_ACTIVE) {
            displayPowerState = DISPLAY_ACTIVE;
            setBacklightBrightness(100);
            tft.writecommand(0x29);  // Display ON
        }
        return;
    }
    
    // Menu timeout - return to main screen (dimmed) after 60s
    if (currentScreen != SCREEN_MAIN && currentScreen != SCREEN_MENU_LOCKED) {
        if (idleMs >= (DEFAULT_MENU_TIMEOUT_SEC * 1000UL)) {
            // Exit menu without saving, go to main dimmed
            display_showMain();
            displayPowerState = DISPLAY_DIM;
            setBacklightBrightness(50);
            return;
        }
    }
    
    // State transitions based on idle time
    switch (displayPowerState) {
        case DISPLAY_ACTIVE:
            if (idleMs >= (DEFAULT_DIM_TIMEOUT_SEC * 1000UL)) {
                displayPowerState = DISPLAY_DIM;
                setBacklightBrightness(50);
            }
            break;
            
        case DISPLAY_DIM:
            if (idleMs >= ((DEFAULT_DIM_TIMEOUT_SEC + DEFAULT_SLEEP_TIMEOUT_SEC) * 1000UL)) {
                displayPowerState = DISPLAY_SLEEP;
                setBacklightBrightness(0);
                tft.writecommand(0x28);  // Display OFF
            }
            break;
            
        case DISPLAY_SLEEP:
            // Stay asleep until button press
            break;
    }
}

void display_wake(void) {
    lastInputMs = millis();
    
    if (displayPowerState == DISPLAY_SLEEP) {
        tft.writecommand(0x29);  // Display ON
    }
    
    displayPowerState = DISPLAY_ACTIVE;
    setBacklightBrightness(100);
}

void display_resetActivityTimer(void) {
    lastInputMs = millis();
    lastActivityMs = millis();  // Also reset menu lock timer
}

DisplayPowerState_t display_getPowerState(void) {
    return displayPowerState;
}

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static void formatFlowValue(char* buf, size_t bufSize, float value) {
    float absVal = fabs(value);
    if (absVal < 10.0f) {
        snprintf(buf, bufSize, "%.1f", absVal);
    } else if (absVal < 100.0f) {
        snprintf(buf, bufSize, "%.1f", absVal);
    } else {
        snprintf(buf, bufSize, "%.0f", absVal);
    }
}

static void formatVolumeWithUnit(char* valueBuf, size_t valueBufSize,
                                  char* unitBuf, size_t unitBufSize,
                                  float liters, UnitSystem_t unitSystem) {
    float absLiters = fabs(liters);
    
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
        float absGal = fabs(gallons);
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
    } else { // UNIT_SYSTEM_IMPERIAL_AG
        float gallons = liters * LITERS_TO_GALLONS;
        float acreFt = liters * LITERS_TO_ACRE_FT;
        float absGal = fabs(gallons);
        if (absGal < 10000.0f) {
            snprintf(valueBuf, valueBufSize, "%.1f", gallons);
            snprintf(unitBuf, unitBufSize, "gal");
        } else if (fabs(acreFt) < 1.0f) {
            snprintf(valueBuf, valueBufSize, "%.2f", acreFt * 12.0f);
            snprintf(unitBuf, unitBufSize, "ac-in");
        } else {
            snprintf(valueBuf, valueBufSize, "%.2f", acreFt);
            snprintf(unitBuf, unitBufSize, "ac-ft");
        }
    }
}

static const char* getFlowUnitStr(UnitSystem_t unitSystem) {
    if (unitSystem == UNIT_SYSTEM_METRIC) {
        return "L/min";
    } else {
        return "GPM";
    }
}

static float convertFlowRate(float lpm, UnitSystem_t unitSystem) {
    if (unitSystem == UNIT_SYSTEM_METRIC) {
        return lpm;
    } else {
        return lpm * LITERS_TO_GALLONS;
    }
}

/* ==========================================================================
 * SCREENS
 * ========================================================================== */

void display_showSplash(void) {
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "AgSys");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, COLOR_FLOW_FWD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);
    
    // Subtitle
    lv_obj_t* subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Mag Meter");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(subtitle, COLOR_TEXT, 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 0);
    
    // Version
    lv_obj_t* version = lv_label_create(screen);
    char ver_str[32];
    snprintf(ver_str, sizeof(ver_str), "v%d.%d.%d", 
             FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
    lv_label_set_text(version, ver_str);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version, COLOR_TEXT_LABEL, 0);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, 40);
    
    lv_scr_load(screen);
    
    for (int i = 0; i < 10; i++) {
        lv_timer_handler();
        delay(100);
    }
}

void display_showMain(void) {
    currentScreen = SCREEN_MAIN;
    
    // Create main screen with background
    screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_main, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen_main, 0, 0);
    
    // ===== OUTER FRAME: Thin border with rounded corners (LCD bezel) =====
    #define FRAME_BORDER 2
    #define FRAME_RADIUS 8
    #define FRAME_PAD 3
    #define CONTENT_WIDTH (DISP_WIDTH - 2 * (FRAME_BORDER + FRAME_PAD))
    #define CONTENT_HEIGHT (DISP_HEIGHT - 2 * (FRAME_BORDER + FRAME_PAD))
    
    lv_obj_t* frame = lv_obj_create(screen_main);
    lv_obj_set_size(frame, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_align(frame, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(frame, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(frame, FRAME_BORDER, 0);
    lv_obj_set_style_border_color(frame, COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(frame, FRAME_RADIUS, 0);
    lv_obj_set_style_pad_all(frame, FRAME_PAD, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    
    // ===== TOP SECTION: Current Flow Rate =====
    // Height: ~95px for flow value + bar + label
    #define FLOW_SECTION_H 95
    
    lv_obj_t* flow_section = lv_obj_create(frame);
    lv_obj_set_size(flow_section, CONTENT_WIDTH, FLOW_SECTION_H);
    lv_obj_align(flow_section, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(flow_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(flow_section, 0, 0);
    lv_obj_set_style_pad_all(flow_section, 0, 0);
    lv_obj_clear_flag(flow_section, LV_OBJ_FLAG_SCROLLABLE);
    
    // Flow value + unit on same line (e.g., "55.4 L/min")
    // Use largest font that fits - 48pt for the number
    label_flow_value = lv_label_create(flow_section);
    lv_label_set_text(label_flow_value, "0.0");
    lv_obj_set_style_text_font(label_flow_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_flow_value, COLOR_TEXT, 0);
    lv_obj_align(label_flow_value, LV_ALIGN_TOP_MID, -20, 0);
    
    // Flow unit inline with value
    label_flow_unit = lv_label_create(flow_section);
    UnitSystem_t units = userSettings ? userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    lv_label_set_text(label_flow_unit, units == UNIT_SYSTEM_METRIC ? "L/min" : "GPM");
    lv_obj_set_style_text_font(label_flow_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_flow_unit, COLOR_TEXT_LABEL, 0);
    lv_obj_align_to(label_flow_unit, label_flow_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, -8);
    
    // Flow bar with gradient effect and arrow
    lv_obj_t* bar_container = lv_obj_create(flow_section);
    lv_obj_set_size(bar_container, CONTENT_WIDTH - 10, 22);
    lv_obj_align(bar_container, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_bg_color(bar_container, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_border_width(bar_container, 1, 0);
    lv_obj_set_style_border_color(bar_container, COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(bar_container, 4, 0);
    lv_obj_set_style_pad_all(bar_container, 2, 0);
    lv_obj_set_style_shadow_width(bar_container, 2, 0);
    lv_obj_set_style_shadow_color(bar_container, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_shadow_ofs_y(bar_container, 1, 0);
    lv_obj_clear_flag(bar_container, LV_OBJ_FLAG_SCROLLABLE);
    
    obj_flow_bar = lv_bar_create(bar_container);
    lv_obj_set_size(obj_flow_bar, CONTENT_WIDTH - 50, 14);
    lv_obj_align(obj_flow_bar, LV_ALIGN_LEFT_MID, 2, 0);
    lv_bar_set_range(obj_flow_bar, 0, 100);
    lv_bar_set_value(obj_flow_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(obj_flow_bar, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj_flow_bar, COLOR_FLOW_FWD, LV_PART_INDICATOR);
    lv_obj_set_style_radius(obj_flow_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(obj_flow_bar, 3, LV_PART_INDICATOR);
    
    obj_flow_arrow = lv_label_create(bar_container);
    lv_label_set_text(obj_flow_arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(obj_flow_arrow, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(obj_flow_arrow, COLOR_FLOW_IDLE, 0);
    lv_obj_align(obj_flow_arrow, LV_ALIGN_RIGHT_MID, -2, 0);
    
    // "Current Flow Rate" label - small
    lv_obj_t* label_current = lv_label_create(flow_section);
    lv_label_set_text(label_current, "Current Flow Rate");
    lv_obj_set_style_text_font(label_current, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_current, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_current, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    // ===== HORIZONTAL DIVIDER 1 =====
    lv_obj_t* divider1 = lv_obj_create(frame);
    lv_obj_set_size(divider1, CONTENT_WIDTH, 1);
    lv_obj_align(divider1, LV_ALIGN_TOP_MID, 0, FLOW_SECTION_H);
    lv_obj_set_style_bg_color(divider1, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(divider1, 0, 0);
    
    // ===== MIDDLE SECTION: Trend | Avg Vol =====
    #define MID_SECTION_H 70
    #define MID_SECTION_Y (FLOW_SECTION_H + 1)
    
    // Left panel: Trend
    lv_obj_t* trend_panel = lv_obj_create(frame);
    lv_obj_set_size(trend_panel, CONTENT_WIDTH / 2 - 1, MID_SECTION_H);
    lv_obj_align(trend_panel, LV_ALIGN_TOP_LEFT, 0, MID_SECTION_Y);
    lv_obj_set_style_bg_opa(trend_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(trend_panel, 0, 0);
    lv_obj_set_style_pad_all(trend_panel, 2, 0);
    lv_obj_clear_flag(trend_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    label_trend_value = lv_label_create(trend_panel);
    lv_label_set_text(label_trend_value, "+0.0L");
    lv_obj_set_style_text_font(label_trend_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_trend_value, COLOR_TEXT, 0);
    lv_obj_align(label_trend_value, LV_ALIGN_CENTER, 0, -8);
    
    lv_obj_t* label_trend = lv_label_create(trend_panel);
    lv_label_set_text(label_trend, "Trend");
    lv_obj_set_style_text_font(label_trend, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_trend, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_trend, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    // Vertical divider
    lv_obj_t* vdivider = lv_obj_create(frame);
    lv_obj_set_size(vdivider, 1, MID_SECTION_H);
    lv_obj_align(vdivider, LV_ALIGN_TOP_MID, 0, MID_SECTION_Y);
    lv_obj_set_style_bg_color(vdivider, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(vdivider, 0, 0);
    
    // Right panel: Avg Vol
    lv_obj_t* avg_panel = lv_obj_create(frame);
    lv_obj_set_size(avg_panel, CONTENT_WIDTH / 2 - 1, MID_SECTION_H);
    lv_obj_align(avg_panel, LV_ALIGN_TOP_RIGHT, 0, MID_SECTION_Y);
    lv_obj_set_style_bg_opa(avg_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(avg_panel, 0, 0);
    lv_obj_set_style_pad_all(avg_panel, 2, 0);
    lv_obj_clear_flag(avg_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    label_avg_value = lv_label_create(avg_panel);
    lv_label_set_text(label_avg_value, "0.0L");
    lv_obj_set_style_text_font(label_avg_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_avg_value, COLOR_TEXT, 0);
    lv_obj_align(label_avg_value, LV_ALIGN_CENTER, 0, -8);
    
    lv_obj_t* label_avg = lv_label_create(avg_panel);
    lv_label_set_text(label_avg, "AVG Vol");
    lv_obj_set_style_text_font(label_avg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_avg, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_avg, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    // ===== HORIZONTAL DIVIDER 2 =====
    #define TOTAL_SECTION_Y (MID_SECTION_Y + MID_SECTION_H)
    
    lv_obj_t* divider2 = lv_obj_create(frame);
    lv_obj_set_size(divider2, CONTENT_WIDTH, 1);
    lv_obj_align(divider2, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y);
    lv_obj_set_style_bg_color(divider2, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(divider2, 0, 0);
    
    // ===== BOTTOM SECTION: Total Volume =====
    // Remaining height for total section
    #define TOTAL_SECTION_H (CONTENT_HEIGHT - TOTAL_SECTION_Y - 1)
    
    total_section = lv_obj_create(frame);
    lv_obj_set_size(total_section, CONTENT_WIDTH, TOTAL_SECTION_H);
    lv_obj_align(total_section, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y + 1);
    lv_obj_set_style_bg_opa(total_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(total_section, 0, 0);
    lv_obj_set_style_pad_all(total_section, 2, 0);
    lv_obj_clear_flag(total_section, LV_OBJ_FLAG_SCROLLABLE);
    
    // Total value + unit inline (e.g., "649.1 ML")
    label_total_value = lv_label_create(total_section);
    lv_label_set_text(label_total_value, "0.0");
    lv_obj_set_style_text_font(label_total_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_total_value, COLOR_TEXT, 0);
    lv_obj_align(label_total_value, LV_ALIGN_CENTER, -15, -8);
    
    label_total_unit = lv_label_create(total_section);
    lv_label_set_text(label_total_unit, "L");
    lv_obj_set_style_text_font(label_total_unit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_total_unit, COLOR_TEXT_LABEL, 0);
    lv_obj_align_to(label_total_unit, label_total_value, LV_ALIGN_OUT_RIGHT_BOTTOM, 3, -5);
    
    lv_obj_t* label_total = lv_label_create(total_section);
    lv_label_set_text(label_total, "Total Vol");
    lv_obj_set_style_text_font(label_total, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_total, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_total, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    // ===== ALARM OVERLAY (hidden by default, replaces total section) =====
    alarm_overlay = lv_obj_create(frame);
    lv_obj_set_size(alarm_overlay, CONTENT_WIDTH, TOTAL_SECTION_H);
    lv_obj_align(alarm_overlay, LV_ALIGN_TOP_MID, 0, TOTAL_SECTION_Y + 1);
    lv_obj_set_style_bg_color(alarm_overlay, COLOR_ALARM_WARNING, 0);
    lv_obj_set_style_border_width(alarm_overlay, 0, 0);
    lv_obj_set_style_radius(alarm_overlay, 0, 0);
    lv_obj_set_style_pad_all(alarm_overlay, 4, 0);
    lv_obj_add_flag(alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(alarm_overlay, LV_OBJ_FLAG_SCROLLABLE);
    
    alarm_title_label = lv_label_create(alarm_overlay);
    lv_label_set_text(alarm_title_label, "");
    lv_obj_set_style_text_font(alarm_title_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(alarm_title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(alarm_title_label, LV_ALIGN_TOP_MID, 0, 2);
    
    alarm_detail_label = lv_label_create(alarm_overlay);
    lv_label_set_text(alarm_detail_label, "");
    lv_obj_set_style_text_font(alarm_detail_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(alarm_detail_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(alarm_detail_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(alarm_detail_label, LV_ALIGN_CENTER, 0, 2);
    
    alarm_hint_label = lv_label_create(alarm_overlay);
    lv_label_set_text(alarm_hint_label, LV_SYMBOL_OK " Ack  " LV_SYMBOL_LEFT " Dismiss");
    lv_obj_set_style_text_font(alarm_hint_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(alarm_hint_label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(alarm_hint_label, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    alarmOverlayActive = false;
    
    lv_scr_load(screen_main);
}

void display_updateMain(float flowRate_LPM, float totalVolume_L, 
                        float trendVolume_L, float avgVolume_L,
                        bool reverseFlow) {
    if (screen_main == NULL) return;
    
    UnitSystem_t units = userSettings ? userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    float maxFlow = userSettings ? userSettings->maxFlowLPM : DEFAULT_MAX_FLOW_MM_S;
    
    char valueBuf[32];
    char unitBuf[16];
    
    // Update flow rate
    float displayFlow = convertFlowRate(fabs(flowRate_LPM), units);
    formatFlowValue(valueBuf, sizeof(valueBuf), displayFlow);
    lv_label_set_text(label_flow_value, valueBuf);
    lv_label_set_text(label_flow_unit, getFlowUnitStr(units));
    
    // Update flow bar (0-100%)
    int barPercent = (int)((fabs(flowRate_LPM) / maxFlow) * 100.0f);
    if (barPercent > 100) barPercent = 100;
    lv_bar_set_value(obj_flow_bar, barPercent, LV_ANIM_ON);
    
    // Update flow arrow color and direction
    lv_color_t arrowColor;
    const char* arrowSymbol;
    if (fabs(flowRate_LPM) < 0.1f) {
        arrowColor = COLOR_FLOW_IDLE;
        arrowSymbol = LV_SYMBOL_RIGHT;
    } else if (reverseFlow) {
        arrowColor = COLOR_FLOW_REV;
        arrowSymbol = LV_SYMBOL_LEFT;
        lv_obj_set_style_bg_color(obj_flow_bar, COLOR_FLOW_REV, LV_PART_INDICATOR);
    } else {
        arrowColor = COLOR_FLOW_FWD;
        arrowSymbol = LV_SYMBOL_RIGHT;
        lv_obj_set_style_bg_color(obj_flow_bar, COLOR_FLOW_FWD, LV_PART_INDICATOR);
    }
    lv_obj_set_style_text_color(obj_flow_arrow, arrowColor, 0);
    lv_label_set_text(obj_flow_arrow, arrowSymbol);
    
    // Update trend (with +/- sign)
    formatVolumeWithUnit(valueBuf, sizeof(valueBuf), unitBuf, sizeof(unitBuf), 
                         fabs(trendVolume_L), units);
    char trendBuf[48];
    snprintf(trendBuf, sizeof(trendBuf), "%s%s%s", 
             trendVolume_L >= 0 ? "+" : "-", valueBuf, unitBuf);
    lv_label_set_text(label_trend_value, trendBuf);
    
    // Update avg
    formatVolumeWithUnit(valueBuf, sizeof(valueBuf), unitBuf, sizeof(unitBuf), 
                         avgVolume_L, units);
    char avgBuf[48];
    snprintf(avgBuf, sizeof(avgBuf), "%s%s", valueBuf, unitBuf);
    lv_label_set_text(label_avg_value, avgBuf);
    
    // Update total volume
    formatVolumeWithUnit(valueBuf, sizeof(valueBuf), unitBuf, sizeof(unitBuf), 
                         totalVolume_L, units);
    lv_label_set_text(label_total_value, valueBuf);
    lv_label_set_text(label_total_unit, unitBuf);
}

void display_showError(const char* message) {
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFE0E0), 0);
    
    lv_obj_t* icon = lv_label_create(screen);
    lv_label_set_text(icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xCC0000), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -30);
    
    lv_obj_t* msg = lv_label_create(screen);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(msg, COLOR_TEXT, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 20);
    
    lv_scr_load(screen);
}

void display_setSettings(UserSettings_t* settings) {
    userSettings = settings;
}

ScreenId_t display_getCurrentScreen(void) {
    return currentScreen;
}

/* ==========================================================================
 * MENU NAVIGATION
 * ========================================================================== */

// Main menu items
#define MENU_ITEM_COUNT 8
static const char* menuItems[MENU_ITEM_COUNT] = {
    "Display Settings",
    "Flow Settings",
    "Alarm Settings",
    "LoRa Config",
    "Calibration",
    "Totalizer",
    "Diagnostics",
    "About"
};

// Submenu items
#define DISPLAY_MENU_COUNT 4
static const char* displayMenuItems[DISPLAY_MENU_COUNT] = {
    "Units", "Trend Period", "Avg Period", "Back"
};

#define FLOW_MENU_COUNT 2
static const char* flowMenuItems[FLOW_MENU_COUNT] = {
    "Max Flow Rate", "Back"
};

#define ALARM_MENU_COUNT 4
static const char* alarmMenuItems[ALARM_MENU_COUNT] = {
    "Leak Threshold", "Leak Duration", "High Flow Thresh", "Back"
};

#define LORA_MENU_COUNT 5
static const char* loraMenuItems[LORA_MENU_COUNT] = {
    "Report Interval", "Spreading Factor", "Ping Controller", "Set Secret", "Back"
};

#define CAL_MENU_COUNT 2
static const char* calMenuItems[CAL_MENU_COUNT] = {
    "Zero Offset", "Back"
};

#define TOTAL_MENU_COUNT 2
static const char* totalMenuItems[TOTAL_MENU_COUNT] = {
    "Reset Total", "Back"
};

#define DIAG_MENU_COUNT 3
static const char* diagMenuItems[DIAG_MENU_COUNT] = {
    "LoRa Status", "ADC Values", "Back"
};

// Submenu selection tracking
static int submenuSelection = 0;

static lv_obj_t* menuLabels[MENU_ITEM_COUNT] = {NULL};
#define MAX_SUBMENU_ITEMS 8
static lv_obj_t* submenuLabels[MAX_SUBMENU_ITEMS] = {NULL};
static lv_obj_t* settingValueLabel = NULL;
static int settingEditValue = 0;

// Alarm settings edit value
static int alarmEditValue = 0;

// Totalizer state
static float currentTotalLiters = 0;

// LoRa spreading factor
static int spreadFactorValue = 7;

// LoRa edit value for settings screens
static int loraEditValue = 0;

static void updateMenuHighlight(void) {
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        if (menuLabels[i] == NULL) continue;
        if (i == menuSelection) {
            lv_obj_set_style_bg_color(lv_obj_get_parent(menuLabels[i]), COLOR_FLOW_FWD, 0);
            lv_obj_set_style_text_color(menuLabels[i], lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_set_style_bg_color(lv_obj_get_parent(menuLabels[i]), COLOR_PANEL_BG, 0);
            lv_obj_set_style_text_color(menuLabels[i], COLOR_TEXT, 0);
        }
    }
}

static void updateSubmenuHighlight(int itemCount) {
    for (int i = 0; i < itemCount && i < MAX_SUBMENU_ITEMS; i++) {
        if (submenuLabels[i] == NULL) continue;
        if (i == submenuSelection) {
            lv_obj_set_style_bg_color(lv_obj_get_parent(submenuLabels[i]), COLOR_FLOW_FWD, 0);
            lv_obj_set_style_text_color(submenuLabels[i], lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_set_style_bg_color(lv_obj_get_parent(submenuLabels[i]), COLOR_PANEL_BG, 0);
            lv_obj_set_style_text_color(submenuLabels[i], COLOR_TEXT, 0);
        }
    }
}

void display_handleButton(ButtonEvent_t event) {
    // Check if any long press event
    bool isLongPress = (event == BTN_UP_LONG || event == BTN_DOWN_LONG ||
                        event == BTN_LEFT_LONG || event == BTN_RIGHT_LONG ||
                        event == BTN_SELECT_LONG);
    
    // Handle display power states first
    if (displayPowerState == DISPLAY_SLEEP) {
        // Display is off - handle wake behavior
        if (isLongPress) {
            // Long press while sleeping: wake and show PIN entry (standalone)
            display_wake();
            if (display_isMenuLocked()) {
                display_showMenuLocked();
            } else {
                display_showMenu();
            }
        } else {
            // Short press while sleeping: just wake, don't process action
            display_wake();
        }
        return;
    }
    
    if (displayPowerState == DISPLAY_DIM) {
        // Display is dimmed - any press wakes to active
        display_wake();
        // Continue to process the button action
    }
    
    // Reset activity timer on any button press
    display_resetActivityTimer();
    
    switch (currentScreen) {
        case SCREEN_MAIN:
            // If alarm overlay is active, handle alarm buttons first
            if (alarmOverlayActive) {
                if (event == BTN_SELECT_SHORT) {
                    display_acknowledgeAlarm();
                    // Callback to main.cpp to handle alarm acknowledgment
                    extern void onAlarmAcknowledged(void);
                    onAlarmAcknowledged();
                } else if (event == BTN_LEFT_SHORT) {
                    display_dismissAlarm();  // Hide overlay but alarm still active
                }
                break;
            }
            
            // If PIN overlay is active, handle PIN entry
            if (pinOverlayMode) {
                if (event == BTN_UP_SHORT) {
                    enteredPin[pinDigitIndex]++;
                    if (enteredPin[pinDigitIndex] > 9) enteredPin[pinDigitIndex] = 0;
                    updatePinDisplay();
                } else if (event == BTN_DOWN_SHORT) {
                    if (enteredPin[pinDigitIndex] == 0) {
                        enteredPin[pinDigitIndex] = 9;
                    } else {
                        enteredPin[pinDigitIndex]--;
                    }
                    updatePinDisplay();
                } else if (event == BTN_RIGHT_SHORT) {
                    pinDigitIndex++;
                    if (pinDigitIndex > 3) pinDigitIndex = 0;
                    updatePinDisplay();
                } else if (event == BTN_LEFT_SHORT) {
                    // Cancel - hide PIN overlay
                    display_hidePinOverlay();
                } else if (event == BTN_SELECT_SHORT) {
                    // Try to unlock
                    if (checkPin()) {
                        menuLockState = MENU_UNLOCKED_PIN;
                        lastActivityMs = millis();
                        display_hidePinOverlay();
                        display_showMenu();
                    } else {
                        // Wrong PIN - reset
                        for (int i = 0; i < 4; i++) enteredPin[i] = 0;
                        pinDigitIndex = 0;
                        updatePinDisplay();
                    }
                }
                break;
            }
            
            // Normal main screen handling - long press to access menu
            if (isLongPress) {
                // Long press: access menu (with PIN if locked)
                if (display_isMenuLocked()) {
                    display_showPinOverlay();  // Show PIN overlay on main screen
                } else {
                    display_showMenu();
                }
            }
            break;
            
        case SCREEN_MENU_LOCKED:
            // PIN entry handling
            if (event == BTN_UP_SHORT) {
                enteredPin[pinDigitIndex]++;
                if (enteredPin[pinDigitIndex] > 9) enteredPin[pinDigitIndex] = 0;
                updatePinDisplay();
            } else if (event == BTN_DOWN_SHORT) {
                if (enteredPin[pinDigitIndex] == 0) {
                    enteredPin[pinDigitIndex] = 9;
                } else {
                    enteredPin[pinDigitIndex]--;
                }
                updatePinDisplay();
            } else if (event == BTN_RIGHT_SHORT) {
                pinDigitIndex++;
                if (pinDigitIndex > 3) pinDigitIndex = 0;
                updatePinDisplay();
            } else if (event == BTN_LEFT_SHORT) {
                // Cancel - go back to main
                display_showMain();
            } else if (event == BTN_SELECT_SHORT) {
                // Try to unlock
                if (checkPin()) {
                    menuLockState = MENU_UNLOCKED_PIN;
                    lastActivityMs = millis();
                    display_showMenu();
                } else {
                    // Wrong PIN - flash red or shake animation could go here
                    // For now just reset
                    for (int i = 0; i < 4; i++) enteredPin[i] = 0;
                    pinDigitIndex = 0;
                    updatePinDisplay();
                }
            }
            break;
            
        case SCREEN_MENU:
            if (event == BTN_UP_SHORT) {
                menuSelection--;
                if (menuSelection < 0) menuSelection = MENU_ITEM_COUNT - 1;
                updateMenuHighlight();
            } else if (event == BTN_DOWN_SHORT) {
                menuSelection++;
                if (menuSelection >= MENU_ITEM_COUNT) menuSelection = 0;
                updateMenuHighlight();
            } else if (event == BTN_SELECT_SHORT) {
                submenuSelection = 0;  // Reset submenu selection
                switch (menuSelection) {
                    case 0: display_showDisplaySettings(); break;
                    case 1: display_showFlowSettings(); break;
                    case 2: display_showAlarmSettings(); break;
                    case 3: display_showLoRaConfig(); break;
                    case 4: display_showCalibration(); break;
                    case 5: display_showTotalizer(0); break;
                    case 6: display_showDiagnostics(); break;
                    case 7: display_showAbout(); break;
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMain();
            }
            break;
        
        case SCREEN_DISPLAY_SETTINGS:
            if (event == BTN_UP_SHORT) {
                submenuSelection--;
                if (submenuSelection < 0) submenuSelection = DISPLAY_MENU_COUNT - 1;
                updateSubmenuHighlight(DISPLAY_MENU_COUNT);
            } else if (event == BTN_DOWN_SHORT) {
                submenuSelection++;
                if (submenuSelection >= DISPLAY_MENU_COUNT) submenuSelection = 0;
                updateSubmenuHighlight(DISPLAY_MENU_COUNT);
            } else if (event == BTN_SELECT_SHORT) {
                switch (submenuSelection) {
                    case 0: display_showSettingsUnits(); break;
                    case 1: display_showSettingsTrend(); break;
                    case 2: display_showSettingsAvg(); break;
                    case 3: display_showMenu(); break;  // Back
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
        
        case SCREEN_FLOW_SETTINGS:
            if (event == BTN_UP_SHORT) {
                submenuSelection--;
                if (submenuSelection < 0) submenuSelection = FLOW_MENU_COUNT - 1;
                updateSubmenuHighlight(FLOW_MENU_COUNT);
            } else if (event == BTN_DOWN_SHORT) {
                submenuSelection++;
                if (submenuSelection >= FLOW_MENU_COUNT) submenuSelection = 0;
                updateSubmenuHighlight(FLOW_MENU_COUNT);
            } else if (event == BTN_SELECT_SHORT) {
                switch (submenuSelection) {
                    case 0: display_showSettingsMaxFlow(); break;
                    case 1: display_showMenu(); break;  // Back
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
        
        case SCREEN_ALARM_SETTINGS:
            if (event == BTN_UP_SHORT) {
                submenuSelection--;
                if (submenuSelection < 0) submenuSelection = ALARM_MENU_COUNT - 1;
                updateSubmenuHighlight(ALARM_MENU_COUNT);
            } else if (event == BTN_DOWN_SHORT) {
                submenuSelection++;
                if (submenuSelection >= ALARM_MENU_COUNT) submenuSelection = 0;
                updateSubmenuHighlight(ALARM_MENU_COUNT);
            } else if (event == BTN_SELECT_SHORT) {
                switch (submenuSelection) {
                    case 0: display_showAlarmLeakThreshold(); break;
                    case 1: display_showAlarmLeakDuration(); break;
                    case 2: display_showAlarmHighFlow(); break;
                    case 3: display_showMenu(); break;  // Back
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
            
        case SCREEN_SETTINGS_UNITS:
            if (event == BTN_UP_SHORT || event == BTN_LEFT_SHORT) {
                settingEditValue--;
                if (settingEditValue < 0) settingEditValue = 2;
                updateSettingsUnitsDisplay();
            } else if (event == BTN_DOWN_SHORT || event == BTN_RIGHT_SHORT) {
                settingEditValue++;
                if (settingEditValue > 2) settingEditValue = 0;
                updateSettingsUnitsDisplay();
            } else if (event == BTN_SELECT_SHORT) {
                if (userSettings) {
                    userSettings->unitSystem = (UnitSystem_t)settingEditValue;
                    settings_save();
                }
                display_showDisplaySettings();
            } else if (event == BTN_SELECT_LONG) {
                display_showDisplaySettings();
            }
            break;
            
        case SCREEN_SETTINGS_TREND:
            if (event == BTN_UP_SHORT || event == BTN_RIGHT_SHORT) {
                settingEditValue += 1;
                if (settingEditValue > 60) settingEditValue = 60;
                updateSettingsTrendDisplay();
            } else if (event == BTN_DOWN_SHORT || event == BTN_LEFT_SHORT) {
                settingEditValue -= 1;
                if (settingEditValue < 1) settingEditValue = 1;
                updateSettingsTrendDisplay();
            } else if (event == BTN_SELECT_SHORT) {
                if (userSettings) {
                    userSettings->trendPeriodMin = settingEditValue;
                    settings_save();
                }
                display_showDisplaySettings();
            } else if (event == BTN_SELECT_LONG) {
                display_showDisplaySettings();
            }
            break;
            
        case SCREEN_SETTINGS_AVG:
            if (event == BTN_UP_SHORT || event == BTN_RIGHT_SHORT) {
                settingEditValue += 5;
                if (settingEditValue > 120) settingEditValue = 120;
                updateSettingsAvgDisplay();
            } else if (event == BTN_DOWN_SHORT || event == BTN_LEFT_SHORT) {
                settingEditValue -= 5;
                if (settingEditValue < 5) settingEditValue = 5;
                updateSettingsAvgDisplay();
            } else if (event == BTN_SELECT_SHORT) {
                if (userSettings) {
                    userSettings->avgPeriodMin = settingEditValue;
                    settings_save();
                }
                display_showDisplaySettings();
            } else if (event == BTN_SELECT_LONG) {
                display_showDisplaySettings();
            }
            break;
            
        case SCREEN_SETTINGS_MAX_FLOW:
            if (event == BTN_UP_SHORT || event == BTN_RIGHT_SHORT) {
                settingEditValue += 10;
                if (settingEditValue > 2000) settingEditValue = 2000;
                updateSettingsMaxFlowDisplay();
            } else if (event == BTN_DOWN_SHORT || event == BTN_LEFT_SHORT) {
                settingEditValue -= 10;
                if (settingEditValue < 10) settingEditValue = 10;
                updateSettingsMaxFlowDisplay();
            } else if (event == BTN_SELECT_SHORT) {
                if (userSettings) {
                    userSettings->maxFlowLPM = (float)settingEditValue;
                    settings_save();
                }
                display_showFlowSettings();
            } else if (event == BTN_SELECT_LONG) {
                display_showFlowSettings();
            }
            break;
            
        case SCREEN_ABOUT:
            if (event == BTN_SELECT_SHORT || event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
            
        case SCREEN_CALIBRATION:
            if (event == BTN_UP_SHORT) {
                submenuSelection--;
                if (submenuSelection < 0) submenuSelection = CAL_MENU_COUNT - 1;
                updateSubmenuHighlight(CAL_MENU_COUNT);
            } else if (event == BTN_DOWN_SHORT) {
                submenuSelection++;
                if (submenuSelection >= CAL_MENU_COUNT) submenuSelection = 0;
                updateSubmenuHighlight(CAL_MENU_COUNT);
            } else if (event == BTN_SELECT_SHORT) {
                switch (submenuSelection) {
                    case 0: display_showCalZero(); break;
                    case 1: display_showMenu(); break;  // Back
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
            
        case SCREEN_CAL_ZERO:
            if (event == BTN_SELECT_SHORT) {
                // Capture current ADC reading as zero offset
                calibration_captureZero();
                display_showCalibration();
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showCalibration();
            }
            break;
            
        case SCREEN_LORA_CONFIG:
            if (event == BTN_UP_SHORT) {
                submenuSelection--;
                if (submenuSelection < 0) submenuSelection = LORA_MENU_COUNT - 1;
                updateSubmenuHighlight(LORA_MENU_COUNT);
            } else if (event == BTN_DOWN_SHORT) {
                submenuSelection++;
                if (submenuSelection >= LORA_MENU_COUNT) submenuSelection = 0;
                updateSubmenuHighlight(LORA_MENU_COUNT);
            } else if (event == BTN_SELECT_SHORT) {
                switch (submenuSelection) {
                    case 0: display_showLoRaReportInterval(userSettings ? userSettings->loraReportIntervalSec : DEFAULT_LORA_REPORT_SEC); break;
                    case 1: display_showLoRaSpreadFactor(); break;
                    case 2: display_showLoRaPing(); break;
                    case 3: display_showLoRaSetSecret(); break;
                    case 4: display_showMenu(); break;  // Back
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
            
        case SCREEN_LORA_REPORT_INTERVAL:
            if (event == BTN_UP_SHORT) {
                loraEditValue += 10;
                if (loraEditValue > 300) loraEditValue = 300;
            } else if (event == BTN_DOWN_SHORT) {
                loraEditValue -= 10;
                if (loraEditValue < 10) loraEditValue = 10;
            }
            if (event == BTN_UP_SHORT || event == BTN_DOWN_SHORT) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d sec", loraEditValue);
                lv_label_set_text(settingValueLabel, buf);
            } else if (event == BTN_SELECT_SHORT) {
                // Save to settings
                if (userSettings) {
                    userSettings->loraReportIntervalSec = (uint16_t)loraEditValue;
                    settings_save();
                }
                display_showLoRaConfig();
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showLoRaConfig();
            }
            break;
            
        case SCREEN_LORA_SPREAD_FACTOR:
            if (event == BTN_UP_SHORT) {
                spreadFactorValue++;
                if (spreadFactorValue > 12) spreadFactorValue = 12;
            } else if (event == BTN_DOWN_SHORT) {
                spreadFactorValue--;
                if (spreadFactorValue < 7) spreadFactorValue = 7;
            }
            if (event == BTN_UP_SHORT || event == BTN_DOWN_SHORT) {
                char buf[16];
                snprintf(buf, sizeof(buf), "SF%d", spreadFactorValue);
                lv_label_set_text(settingValueLabel, buf);
            } else if (event == BTN_SELECT_SHORT) {
                // Save to settings
                if (userSettings) {
                    userSettings->loraSpreadingFactor = (uint8_t)spreadFactorValue;
                    settings_save();
                }
                display_showLoRaConfig();
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showLoRaConfig();
            }
            break;
        
        case SCREEN_LORA_PING:
            if (event == BTN_SELECT_SHORT) {
                // Send ping and show result
                bool success = sendLoRaPing();
                display_showLoRaPingResult(success);
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showLoRaConfig();
            }
            break;
        
        case SCREEN_LORA_SET_SECRET:
            // Secret entry handled via BLE provisioning, not LCD
            // This screen just shows instructions
            if (event == BTN_SELECT_SHORT || event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showLoRaConfig();
            }
            break;
        
        case SCREEN_ALARM_LEAK_THRESH:
            if (event == BTN_UP_SHORT) {
                alarmEditValue += 5;  // 0.5 L/min increments
                if (alarmEditValue > 100) alarmEditValue = 100;  // Max 10.0 L/min
            } else if (event == BTN_DOWN_SHORT) {
                alarmEditValue -= 5;
                if (alarmEditValue < 5) alarmEditValue = 5;  // Min 0.5 L/min
            }
            if (event == BTN_UP_SHORT || event == BTN_DOWN_SHORT) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f L/min", alarmEditValue / 10.0f);
                lv_label_set_text(settingValueLabel, buf);
            } else if (event == BTN_SELECT_SHORT) {
                // Save to settings
                if (userSettings) {
                    userSettings->alarmLeakThresholdLPM10 = (uint16_t)alarmEditValue;
                    settings_save();
                }
                display_showAlarmSettings();
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showAlarmSettings();
            }
            break;
        
        case SCREEN_ALARM_LEAK_DURATION:
            if (event == BTN_UP_SHORT) {
                alarmEditValue += 5;
                if (alarmEditValue > 240) alarmEditValue = 240;  // Max 4 hours
            } else if (event == BTN_DOWN_SHORT) {
                alarmEditValue -= 5;
                if (alarmEditValue < 5) alarmEditValue = 5;  // Min 5 min
            }
            if (event == BTN_UP_SHORT || event == BTN_DOWN_SHORT) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d min", alarmEditValue);
                lv_label_set_text(settingValueLabel, buf);
            } else if (event == BTN_SELECT_SHORT) {
                // Save to settings
                if (userSettings) {
                    userSettings->alarmLeakDurationMin = (uint16_t)alarmEditValue;
                    settings_save();
                }
                display_showAlarmSettings();
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showAlarmSettings();
            }
            break;
        
        case SCREEN_ALARM_HIGH_FLOW:
            if (event == BTN_UP_SHORT) {
                alarmEditValue += 10;
                if (alarmEditValue > 500) alarmEditValue = 500;  // Max 500 L/min
            } else if (event == BTN_DOWN_SHORT) {
                alarmEditValue -= 10;
                if (alarmEditValue < 50) alarmEditValue = 50;  // Min 50 L/min
            }
            if (event == BTN_UP_SHORT || event == BTN_DOWN_SHORT) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d L/min", alarmEditValue);
                lv_label_set_text(settingValueLabel, buf);
            } else if (event == BTN_SELECT_SHORT) {
                // Save to settings
                if (userSettings) {
                    userSettings->alarmHighFlowLPM = (uint16_t)alarmEditValue;
                    settings_save();
                }
                display_showAlarmSettings();
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showAlarmSettings();
            }
            break;
            
        case SCREEN_TOTALIZER:
            if (event == BTN_UP_SHORT) {
                submenuSelection--;
                if (submenuSelection < 0) submenuSelection = TOTAL_MENU_COUNT - 1;
                updateSubmenuHighlight(TOTAL_MENU_COUNT);
            } else if (event == BTN_DOWN_SHORT) {
                submenuSelection++;
                if (submenuSelection >= TOTAL_MENU_COUNT) submenuSelection = 0;
                updateSubmenuHighlight(TOTAL_MENU_COUNT);
            } else if (event == BTN_SELECT_SHORT) {
                switch (submenuSelection) {
                    case 0: display_showTotalizerReset(currentTotalLiters); break;
                    case 1: display_showMenu(); break;  // Back
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
            
        case SCREEN_TOTALIZER_RESET:
            // Reset requires 3-second hold - handled in main loop
            if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showTotalizer(currentTotalLiters);
            }
            break;
            
        case SCREEN_DIAGNOSTICS:
            if (event == BTN_UP_SHORT) {
                submenuSelection--;
                if (submenuSelection < 0) submenuSelection = DIAG_MENU_COUNT - 1;
                updateSubmenuHighlight(DIAG_MENU_COUNT);
            } else if (event == BTN_DOWN_SHORT) {
                submenuSelection++;
                if (submenuSelection >= DIAG_MENU_COUNT) submenuSelection = 0;
                updateSubmenuHighlight(DIAG_MENU_COUNT);
            } else if (event == BTN_SELECT_SHORT) {
                switch (submenuSelection) {
                    case 0: {
                        LoRaStats_t stats;
                        getLoRaStats(&stats);
                        display_showDiagLoRa(&stats);
                        break;
                    }
                    case 1: {
                        ADCValues_t values;
                        getADCValues(&values);
                        display_showDiagADC(&values);
                        break;
                    }
                    case 2: display_showMenu(); break;  // Back
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
            
        case SCREEN_DIAG_LORA:
        case SCREEN_DIAG_ADC:
            if (event == BTN_SELECT_SHORT || event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showDiagnostics();
            }
            break;
            
        default:
            break;
    }
}

/* ==========================================================================
 * MENU SCREEN
 * ========================================================================== */

void display_showMenu(void) {
    currentScreen = SCREEN_MENU;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    
    // Title bar
    lv_obj_t* titleBar = lv_obj_create(screen);
    lv_obj_set_size(titleBar, DISP_WIDTH, 40);
    lv_obj_align(titleBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(titleBar, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_set_style_radius(titleBar, 0, 0);
    
    lv_obj_t* title = lv_label_create(titleBar);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    
    // Menu items - use smaller height to fit 9 items (320px - 40px header - 25px hint = 255px / 9 = 28px)
    int yPos = 42;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        lv_obj_t* item = lv_obj_create(screen);
        lv_obj_set_size(item, DISP_WIDTH - 10, 20);
        lv_obj_align(item, LV_ALIGN_TOP_MID, 0, yPos);
        lv_obj_set_style_bg_color(item, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_radius(item, 3, 0);
        lv_obj_set_style_pad_left(item, 8, 0);
        
        menuLabels[i] = lv_label_create(item);
        lv_label_set_text(menuLabels[i], menuItems[i]);
        lv_obj_set_style_text_font(menuLabels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(menuLabels[i], COLOR_TEXT, 0);
        lv_obj_align(menuLabels[i], LV_ALIGN_LEFT_MID, 0, 0);
        
        yPos += 21;
    }
    
    // Navigation hint
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate  " LV_SYMBOL_OK " Select  " LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    updateMenuHighlight();
    lv_scr_load(screen);
    screen_menu = screen;
}

/* ==========================================================================
 * SUBMENU SCREENS
 * ========================================================================== */

static void createSubmenuScreen(const char* title, const char** items, int itemCount) {
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    
    // Title bar
    lv_obj_t* titleBar = lv_obj_create(screen);
    lv_obj_set_size(titleBar, DISP_WIDTH, 40);
    lv_obj_align(titleBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(titleBar, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_set_style_radius(titleBar, 0, 0);
    
    lv_obj_t* titleLabel = lv_label_create(titleBar);
    lv_label_set_text(titleLabel, title);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, 0);
    
    // Clear old submenu labels
    for (int i = 0; i < MAX_SUBMENU_ITEMS; i++) {
        submenuLabels[i] = NULL;
    }
    
    // Menu items
    int yPos = 50;
    int itemHeight = 28;
    for (int i = 0; i < itemCount && i < MAX_SUBMENU_ITEMS; i++) {
        lv_obj_t* item = lv_obj_create(screen);
        lv_obj_set_size(item, DISP_WIDTH - 20, itemHeight);
        lv_obj_align(item, LV_ALIGN_TOP_MID, 0, yPos);
        lv_obj_set_style_bg_color(item, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_radius(item, 4, 0);
        lv_obj_set_style_pad_left(item, 12, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        
        submenuLabels[i] = lv_label_create(item);
        lv_label_set_text(submenuLabels[i], items[i]);
        lv_obj_set_style_text_font(submenuLabels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(submenuLabels[i], COLOR_TEXT, 0);
        lv_obj_align(submenuLabels[i], LV_ALIGN_LEFT_MID, 0, 0);
        
        yPos += itemHeight + 4;
    }
    
    // Navigation hint
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate  " LV_SYMBOL_OK " Select  " LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    updateSubmenuHighlight(itemCount);
    lv_scr_load(screen);
}

void display_showDisplaySettings(void) {
    currentScreen = SCREEN_DISPLAY_SETTINGS;
    submenuSelection = 0;
    createSubmenuScreen("Display Settings", displayMenuItems, DISPLAY_MENU_COUNT);
}

void display_showFlowSettings(void) {
    currentScreen = SCREEN_FLOW_SETTINGS;
    submenuSelection = 0;
    createSubmenuScreen("Flow Settings", flowMenuItems, FLOW_MENU_COUNT);
}

void display_showAlarmSettings(void) {
    currentScreen = SCREEN_ALARM_SETTINGS;
    submenuSelection = 0;
    createSubmenuScreen("Alarm Settings", alarmMenuItems, ALARM_MENU_COUNT);
}

/* ==========================================================================
 * SETTINGS SCREENS
 * ========================================================================== */

static void createSettingsScreen(const char* title, const char* hint) {
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    
    // Title bar
    lv_obj_t* titleBar = lv_obj_create(screen);
    lv_obj_set_size(titleBar, DISP_WIDTH, 40);
    lv_obj_align(titleBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(titleBar, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_set_style_radius(titleBar, 0, 0);
    
    lv_obj_t* titleLabel = lv_label_create(titleBar);
    lv_label_set_text(titleLabel, title);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, 0);
    
    // Value display area
    lv_obj_t* valuePanel = lv_obj_create(screen);
    lv_obj_set_size(valuePanel, DISP_WIDTH - 40, 80);
    lv_obj_align(valuePanel, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(valuePanel, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(valuePanel, 2, 0);
    lv_obj_set_style_border_color(valuePanel, COLOR_FLOW_FWD, 0);
    lv_obj_set_style_radius(valuePanel, 10, 0);
    
    settingValueLabel = lv_label_create(valuePanel);
    lv_obj_set_style_text_font(settingValueLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(settingValueLabel, COLOR_TEXT, 0);
    lv_obj_align(settingValueLabel, LV_ALIGN_CENTER, 0, 0);
    
    // Navigation hint
    lv_obj_t* hintLabel = lv_label_create(screen);
    lv_label_set_text(hintLabel, hint);
    lv_obj_set_style_text_font(hintLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hintLabel, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hintLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}

static void updateSettingsUnitsDisplay(void) {
    if (settingValueLabel == NULL) return;
    const char* unitNames[] = {"Metric (L)", "Imperial (gal)", "Ag (acre-ft)"};
    lv_label_set_text(settingValueLabel, unitNames[settingEditValue]);
}

static void updateSettingsTrendDisplay(void) {
    if (settingValueLabel == NULL) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d min", settingEditValue);
    lv_label_set_text(settingValueLabel, buf);
}

static void updateSettingsAvgDisplay(void) {
    if (settingValueLabel == NULL) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d min", settingEditValue);
    lv_label_set_text(settingValueLabel, buf);
}

static void updateSettingsMaxFlowDisplay(void) {
    if (settingValueLabel == NULL) return;
    char buf[32];
    UnitSystem_t units = userSettings ? userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    if (units == UNIT_SYSTEM_METRIC) {
        snprintf(buf, sizeof(buf), "%d L/min", settingEditValue);
    } else {
        snprintf(buf, sizeof(buf), "%d GPM", (int)(settingEditValue * LITERS_TO_GALLONS));
    }
    lv_label_set_text(settingValueLabel, buf);
}

void display_showSettingsUnits(void) {
    currentScreen = SCREEN_SETTINGS_UNITS;
    settingEditValue = userSettings ? (int)userSettings->unitSystem : 0;
    createSettingsScreen("Units", LV_SYMBOL_UP LV_SYMBOL_DOWN " Change  " LV_SYMBOL_OK " Save  " LV_SYMBOL_LEFT " Cancel");
    updateSettingsUnitsDisplay();
}

void display_showSettingsTrend(void) {
    currentScreen = SCREEN_SETTINGS_TREND;
    settingEditValue = userSettings ? userSettings->trendPeriodMin : DEFAULT_TREND_PERIOD_MIN;
    createSettingsScreen("Trend Period", LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    updateSettingsTrendDisplay();
}

void display_showSettingsAvg(void) {
    currentScreen = SCREEN_SETTINGS_AVG;
    settingEditValue = userSettings ? userSettings->avgPeriodMin : DEFAULT_AVG_PERIOD_MIN;
    createSettingsScreen("Avg Period", LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    updateSettingsAvgDisplay();
}

void display_showSettingsMaxFlow(void) {
    currentScreen = SCREEN_SETTINGS_MAX_FLOW;
    settingEditValue = userSettings ? (int)userSettings->maxFlowLPM : (int)DEFAULT_MAX_FLOW_MM_S;
    createSettingsScreen("Max Flow", LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    updateSettingsMaxFlowDisplay();
}

void display_showAbout(void) {
    currentScreen = SCREEN_ABOUT;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "AgSys Mag Meter");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_FLOW_FWD, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);
    
    lv_obj_t* version = lv_label_create(screen);
    char ver_str[64];
    snprintf(ver_str, sizeof(ver_str), "Firmware v%d.%d.%d", 
             FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
    lv_label_set_text(version, ver_str);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(version, COLOR_TEXT, 0);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, -20);
    
    lv_obj_t* info = lv_label_create(screen);
    lv_label_set_text(info, "Electromagnetic\nFlow Meter");
    lv_obj_set_style_text_font(info, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(info, COLOR_TEXT_LABEL, 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 30);
    
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_OK " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * CALIBRATION SCREENS
 * ========================================================================== */

static void updateCalMenuHighlight(void) {
    for (int i = 0; i < 3; i++) {
        if (calLabels[i] == NULL) continue;
        if (i == calMenuSelection) {
            lv_obj_set_style_bg_color(lv_obj_get_parent(calLabels[i]), COLOR_FLOW_FWD, 0);
            lv_obj_set_style_text_color(calLabels[i], lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_set_style_bg_color(lv_obj_get_parent(calLabels[i]), COLOR_PANEL_BG, 0);
            lv_obj_set_style_text_color(calLabels[i], COLOR_TEXT, 0);
        }
    }
}

static void updateCalSpanDisplay(void) {
    if (calValueLabel == NULL) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", calSpanValue);
    lv_label_set_text(calValueLabel, buf);
}

void display_showCalibration(void) {
    currentScreen = SCREEN_CALIBRATION;
    submenuSelection = 0;
    createSubmenuScreen("Calibration", calMenuItems, CAL_MENU_COUNT);
}

void display_showCalZero(void) {
    currentScreen = SCREEN_CAL_ZERO;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    lv_obj_t* zeroTitle = lv_label_create(screen);
    lv_label_set_text(zeroTitle, "Zero Calibration");
    lv_obj_set_style_text_font(zeroTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(zeroTitle, lv_color_hex(0xCC6600), 0);
    lv_obj_align(zeroTitle, LV_ALIGN_TOP_MID, 0, 20);
    
    lv_obj_t* instr = lv_label_create(screen);
    lv_label_set_text(instr, "Ensure NO FLOW\nthrough the pipe.\n\nPress SELECT to\ncapture zero offset.");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(instr, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(instr, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t* zeroHint = lv_label_create(screen);
    lv_label_set_text(zeroHint, LV_SYMBOL_OK " Capture  " LV_SYMBOL_LEFT " Cancel");
    lv_obj_set_style_text_font(zeroHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(zeroHint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(zeroHint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * ALARM SCREEN
 * ========================================================================== */

static lv_obj_t* screen_alarm = NULL;
static bool alarmAcknowledged = false;

static const char* getAlarmTitle(AlarmType_t type) {
    switch (type) {
        case ALARM_LEAK:         return "LEAK ALARM";
        case ALARM_HIGH_FLOW:    return "HIGH FLOW ALARM";
        case ALARM_REVERSE_FLOW: return "REVERSE FLOW";
        case ALARM_TAMPER:       return "TAMPER ALARM";
        default:                 return "ALARM";
    }
}

static const char* getAlarmMessage(AlarmType_t type) {
    switch (type) {
        case ALARM_LEAK:         return "Continuous flow detected";
        case ALARM_HIGH_FLOW:    return "Flow rate exceeds maximum";
        case ALARM_REVERSE_FLOW: return "Reverse flow detected";
        case ALARM_TAMPER:       return "Tamper detected";
        default:                 return "Unknown alarm condition";
    }
}

static lv_color_t getAlarmColor(AlarmType_t type) {
    switch (type) {
        case ALARM_LEAK:
        case ALARM_TAMPER:
            return COLOR_ALARM_CRITICAL;
        default:
            return COLOR_ALARM_WARNING;
    }
}

void display_showAlarm(AlarmType_t alarmType, uint32_t durationSec,
                       float flowRateLPM, float volumeLiters) {
    // Alarm overlays the bottom section of main screen (replaces total volume)
    if (alarm_overlay == NULL || total_section == NULL) {
        return;  // Main screen not initialized
    }
    
    currentAlarmType = alarmType;
    alarmOverlayActive = true;
    
    // Set alarm overlay color based on type
    lv_color_t alarmColor = getAlarmColor(alarmType);
    lv_obj_set_style_bg_color(alarm_overlay, alarmColor, 0);
    
    // Update alarm title
    char titleBuf[32];
    snprintf(titleBuf, sizeof(titleBuf), LV_SYMBOL_WARNING " %s", getAlarmTitle(alarmType));
    lv_label_set_text(alarm_title_label, titleBuf);
    
    // Update alarm details - compact format for overlay
    char detailBuf[64];
    UnitSystem_t units = userSettings ? userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    
    // Format duration
    char durStr[16];
    if (durationSec >= 3600) {
        snprintf(durStr, sizeof(durStr), "%luh %lum", 
                 (unsigned long)(durationSec / 3600), (unsigned long)((durationSec % 3600) / 60));
    } else if (durationSec >= 60) {
        snprintf(durStr, sizeof(durStr), "%lum", (unsigned long)(durationSec / 60));
    } else {
        snprintf(durStr, sizeof(durStr), "%lus", (unsigned long)durationSec);
    }
    
    // Format flow and volume
    if (units == UNIT_SYSTEM_METRIC) {
        snprintf(detailBuf, sizeof(detailBuf), "%s: %s\nFlow: %.1f L/min  Vol: %.0f L",
                 getAlarmMessage(alarmType), durStr, flowRateLPM, volumeLiters);
    } else {
        snprintf(detailBuf, sizeof(detailBuf), "%s: %s\nFlow: %.1f GPM  Vol: %.0f gal",
                 getAlarmMessage(alarmType), durStr, 
                 flowRateLPM * LITERS_TO_GALLONS, volumeLiters * LITERS_TO_GALLONS);
    }
    lv_label_set_text(alarm_detail_label, detailBuf);
    
    // Hide total section, show alarm overlay
    lv_obj_add_flag(total_section, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    
    // Make sure we're on main screen
    if (currentScreen != SCREEN_MAIN) {
        display_showMain();
    }
}

void display_acknowledgeAlarm(void) {
    alarmAcknowledged = true;
    alarmOverlayActive = false;
    currentAlarmType = ALARM_CLEARED;
    
    // Hide alarm overlay, show total section
    if (alarm_overlay != NULL) {
        lv_obj_add_flag(alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (total_section != NULL) {
        lv_obj_clear_flag(total_section, LV_OBJ_FLAG_HIDDEN);
    }
}

// Check if alarm overlay is currently active
bool display_isAlarmActive(void) {
    return alarmOverlayActive;
}

// Dismiss alarm (hide overlay without acknowledging - alarm still active in firmware)
void display_dismissAlarm(void) {
    // Just hide the overlay, but don't clear the alarm state
    // The alarm can be re-shown by calling display_showAlarm again
    if (alarm_overlay != NULL) {
        lv_obj_add_flag(alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (total_section != NULL) {
        lv_obj_clear_flag(total_section, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ==========================================================================
 * MENU LOCK / PIN ENTRY
 * ========================================================================== */

static void updatePinDisplay(void) {
    for (int i = 0; i < 4; i++) {
        if (pinDigitLabels[i] == NULL) continue;
        char buf[2];
        snprintf(buf, sizeof(buf), "%d", enteredPin[i]);
        lv_label_set_text(pinDigitLabels[i], buf);
        
        // Highlight current digit
        if (i == pinDigitIndex) {
            lv_obj_set_style_bg_color(lv_obj_get_parent(pinDigitLabels[i]), COLOR_FLOW_FWD, 0);
            lv_obj_set_style_text_color(pinDigitLabels[i], lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_set_style_bg_color(lv_obj_get_parent(pinDigitLabels[i]), COLOR_PANEL_BG, 0);
            lv_obj_set_style_text_color(pinDigitLabels[i], COLOR_TEXT, 0);
        }
    }
}

static bool checkPin(void) {
    if (userSettings == NULL) return true;
    if (!userSettings->menuLockEnabled) return true;
    
    uint16_t entered = enteredPin[0] * 1000 + enteredPin[1] * 100 + 
                       enteredPin[2] * 10 + enteredPin[3];
    return (entered == userSettings->menuPin);
}

void display_unlockMenuRemote(void) {
    menuLockState = MENU_UNLOCKED_REMOTE;
    lastActivityMs = millis();
}

void display_lockMenu(void) {
    menuLockState = MENU_LOCKED;
}

bool display_isMenuLocked(void) {
    if (userSettings == NULL || !userSettings->menuLockEnabled) {
        return false;
    }
    
    // Check auto-lock timeout
    if (userSettings->menuAutoLockMin > 0 && menuLockState != MENU_LOCKED) {
        uint32_t elapsed = (millis() - lastActivityMs) / 60000;
        if (elapsed >= userSettings->menuAutoLockMin) {
            menuLockState = MENU_LOCKED;
        }
    }
    
    return (menuLockState == MENU_LOCKED);
}

void display_showMenuLocked(void) {
    currentScreen = SCREEN_MENU_LOCKED;
    
    // Reset PIN entry
    for (int i = 0; i < 4; i++) enteredPin[i] = 0;
    pinDigitIndex = 0;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    
    // Frame with border
    lv_obj_t* frame = lv_obj_create(screen);
    lv_obj_set_size(frame, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_align(frame, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(frame, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_border_color(frame, COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(frame, 8, 0);
    lv_obj_set_style_pad_all(frame, 10, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    
    // Lock icon
    lv_obj_t* lockIcon = lv_label_create(frame);
    lv_label_set_text(lockIcon, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_font(lockIcon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lockIcon, COLOR_DIVIDER, 0);
    lv_obj_align(lockIcon, LV_ALIGN_TOP_MID, 0, 10);
    
    // Title
    lv_obj_t* title = lv_label_create(frame);
    lv_label_set_text(title, "Menu Locked");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    // Instructions
    lv_obj_t* instr = lv_label_create(frame);
    lv_label_set_text(instr, "Enter PIN to unlock");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instr, COLOR_TEXT_LABEL, 0);
    lv_obj_align(instr, LV_ALIGN_TOP_MID, 0, 80);
    
    // PIN digit boxes
    int boxWidth = 45;
    int boxSpacing = 55;
    int startX = -(boxSpacing * 3 / 2);
    
    for (int i = 0; i < 4; i++) {
        lv_obj_t* box = lv_obj_create(frame);
        lv_obj_set_size(box, boxWidth, 55);
        lv_obj_align(box, LV_ALIGN_CENTER, startX + i * boxSpacing, 10);
        lv_obj_set_style_bg_color(box, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_border_color(box, COLOR_DIVIDER, 0);
        lv_obj_set_style_radius(box, 8, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        
        pinDigitLabels[i] = lv_label_create(box);
        lv_label_set_text(pinDigitLabels[i], "0");
        lv_obj_set_style_text_font(pinDigitLabels[i], &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(pinDigitLabels[i], COLOR_TEXT, 0);
        lv_obj_align(pinDigitLabels[i], LV_ALIGN_CENTER, 0, 0);
    }
    
    // Navigation hints
    lv_obj_t* hint = lv_label_create(frame);
    lv_label_set_text(hint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Digit  " 
                      LV_SYMBOL_RIGHT " Next  " LV_SYMBOL_OK " Unlock");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -30);
    
    lv_obj_t* hint2 = lv_label_create(frame);
    lv_label_set_text(hint2, LV_SYMBOL_LEFT " Cancel");
    lv_obj_set_style_text_font(hint2, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint2, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint2, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    updatePinDisplay();
    lv_scr_load(screen);
}

// PIN overlay panel (shown on main screen)
static lv_obj_t* pinOverlayPanel = NULL;

void display_showPinOverlay(void) {
    if (currentScreen != SCREEN_MAIN) return;
    
    // Reset PIN entry
    for (int i = 0; i < 4; i++) enteredPin[i] = 0;
    pinDigitIndex = 0;
    pinOverlayMode = true;
    
    // Create overlay panel on top of main screen (replaces total section area)
    if (pinOverlayPanel != NULL) {
        lv_obj_del(pinOverlayPanel);
    }
    
    // Get current screen
    lv_obj_t* scr = lv_scr_act();
    
    // Create overlay panel in the lower portion of the screen
    pinOverlayPanel = lv_obj_create(scr);
    lv_obj_set_size(pinOverlayPanel, DISP_WIDTH - 20, 120);
    lv_obj_align(pinOverlayPanel, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(pinOverlayPanel, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(pinOverlayPanel, 2, 0);
    lv_obj_set_style_border_color(pinOverlayPanel, COLOR_FLOW_FWD, 0);
    lv_obj_set_style_radius(pinOverlayPanel, 8, 0);
    lv_obj_set_style_pad_all(pinOverlayPanel, 8, 0);
    lv_obj_clear_flag(pinOverlayPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(pinOverlayPanel, 10, 0);
    lv_obj_set_style_shadow_color(pinOverlayPanel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(pinOverlayPanel, LV_OPA_30, 0);
    
    // Lock icon and title
    lv_obj_t* title = lv_label_create(pinOverlayPanel);
    lv_label_set_text(title, LV_SYMBOL_EYE_CLOSE " Enter PIN");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    
    // PIN digit boxes
    int boxWidth = 35;
    int boxSpacing = 45;
    int startX = -(boxSpacing * 3 / 2);
    
    for (int i = 0; i < 4; i++) {
        lv_obj_t* box = lv_obj_create(pinOverlayPanel);
        lv_obj_set_size(box, boxWidth, 40);
        lv_obj_align(box, LV_ALIGN_CENTER, startX + i * boxSpacing, 5);
        lv_obj_set_style_bg_color(box, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_border_color(box, COLOR_DIVIDER, 0);
        lv_obj_set_style_radius(box, 6, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        
        pinDigitLabels[i] = lv_label_create(box);
        lv_label_set_text(pinDigitLabels[i], "0");
        lv_obj_set_style_text_font(pinDigitLabels[i], &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(pinDigitLabels[i], COLOR_TEXT, 0);
        lv_obj_align(pinDigitLabels[i], LV_ALIGN_CENTER, 0, 0);
    }
    
    // Navigation hints
    lv_obj_t* hint = lv_label_create(pinOverlayPanel);
    lv_label_set_text(hint, LV_SYMBOL_UP LV_SYMBOL_DOWN " " LV_SYMBOL_RIGHT 
                      "  " LV_SYMBOL_OK " OK  " LV_SYMBOL_LEFT " Cancel");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -2);
    
    updatePinDisplay();
}

void display_hidePinOverlay(void) {
    pinOverlayMode = false;
    
    if (pinOverlayPanel != NULL) {
        lv_obj_del(pinOverlayPanel);
        pinOverlayPanel = NULL;
    }
    
    // Clear PIN digit label references
    for (int i = 0; i < 4; i++) {
        pinDigitLabels[i] = NULL;
    }
}

/* ==========================================================================
 * STATUS BAR (for MAIN screen)
 * ==========================================================================*/

static lv_obj_t* statusLoRaIcon = NULL;
static lv_obj_t* statusAlarmIcon = NULL;
static lv_obj_t* statusTimeLabel = NULL;

void display_updateStatusBar(bool loraConnected, bool hasAlarm,
                             AlarmType_t alarmType, uint32_t lastReportSec) {
    if (statusLoRaIcon == NULL) return;
    
    // LoRa status
    if (loraConnected) {
        lv_label_set_text(statusLoRaIcon, LV_SYMBOL_WIFI " OK");
        lv_obj_set_style_text_color(statusLoRaIcon, lv_color_hex(0x00AA00), 0);
    } else {
        lv_label_set_text(statusLoRaIcon, LV_SYMBOL_WIFI " --");
        lv_obj_set_style_text_color(statusLoRaIcon, COLOR_TEXT_LABEL, 0);
    }
    
    // Alarm icon
    if (hasAlarm && statusAlarmIcon != NULL) {
        const char* alarmText = LV_SYMBOL_WARNING;
        lv_color_t alarmColor = getAlarmColor(alarmType);
        lv_label_set_text(statusAlarmIcon, alarmText);
        lv_obj_set_style_text_color(statusAlarmIcon, alarmColor, 0);
        lv_obj_clear_flag(statusAlarmIcon, LV_OBJ_FLAG_HIDDEN);
    } else if (statusAlarmIcon != NULL) {
        lv_obj_add_flag(statusAlarmIcon, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Time since last report
    if (statusTimeLabel != NULL) {
        char timeBuf[16];
        if (lastReportSec < 60) {
            snprintf(timeBuf, sizeof(timeBuf), "%lus", (unsigned long)lastReportSec);
        } else if (lastReportSec < 3600) {
            snprintf(timeBuf, sizeof(timeBuf), "%lum", (unsigned long)(lastReportSec / 60));
        } else {
            snprintf(timeBuf, sizeof(timeBuf), "%luh", (unsigned long)(lastReportSec / 3600));
        }
        lv_label_set_text(statusTimeLabel, timeBuf);
    }
}

/* ==========================================================================
 * TOTALIZER SCREENS
 * ========================================================================== */

static lv_obj_t* resetProgressBar = NULL;

void display_showTotalizer(float totalLiters) {
    currentScreen = SCREEN_TOTALIZER;
    currentTotalLiters = totalLiters;
    submenuSelection = 0;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    
    // Title bar
    lv_obj_t* titleBar = lv_obj_create(screen);
    lv_obj_set_size(titleBar, DISP_WIDTH, 40);
    lv_obj_align(titleBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(titleBar, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_set_style_radius(titleBar, 0, 0);
    
    lv_obj_t* title = lv_label_create(titleBar);
    lv_label_set_text(title, "Totalizer");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    
    // Current total display
    lv_obj_t* totalLabel = lv_label_create(screen);
    lv_label_set_text(totalLabel, "Current Total:");
    lv_obj_set_style_text_font(totalLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(totalLabel, COLOR_TEXT_LABEL, 0);
    lv_obj_align(totalLabel, LV_ALIGN_TOP_MID, 0, 50);
    
    lv_obj_t* totalValue = lv_label_create(screen);
    char valBuf[16], unitBuf[8], totalBuf[32];
    UnitSystem_t units = userSettings ? userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    formatVolumeWithUnit(valBuf, sizeof(valBuf), unitBuf, sizeof(unitBuf), totalLiters, units);
    snprintf(totalBuf, sizeof(totalBuf), "%s %s", valBuf, unitBuf);
    lv_label_set_text(totalValue, totalBuf);
    lv_obj_set_style_text_font(totalValue, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(totalValue, COLOR_FLOW_FWD, 0);
    lv_obj_align(totalValue, LV_ALIGN_TOP_MID, 0, 75);
    
    // Menu items
    int yPos = 130;
    for (int i = 0; i < TOTAL_MENU_COUNT; i++) {
        lv_obj_t* item = lv_obj_create(screen);
        lv_obj_set_size(item, DISP_WIDTH - 20, 28);
        lv_obj_align(item, LV_ALIGN_TOP_MID, 0, yPos);
        lv_obj_set_style_bg_color(item, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_radius(item, 3, 0);
        lv_obj_set_style_pad_left(item, 8, 0);
        
        submenuLabels[i] = lv_label_create(item);
        lv_label_set_text(submenuLabels[i], totalMenuItems[i]);
        lv_obj_set_style_text_font(submenuLabels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(submenuLabels[i], COLOR_TEXT, 0);
        lv_obj_align(submenuLabels[i], LV_ALIGN_LEFT_MID, 0, 0);
        
        yPos += 32;
    }
    
    // Navigation hint
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Nav  " LV_SYMBOL_OK " Select  " LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    updateSubmenuHighlight(TOTAL_MENU_COUNT);
    lv_scr_load(screen);
}

void display_showTotalizerReset(float currentTotal) {
    currentScreen = SCREEN_TOTALIZER_RESET;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    // Warning title
    lv_obj_t* warnTitle = lv_label_create(screen);
    lv_label_set_text(warnTitle, LV_SYMBOL_WARNING " Reset Totalizer");
    lv_obj_set_style_text_font(warnTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(warnTitle, COLOR_ALARM_WARNING, 0);
    lv_obj_align(warnTitle, LV_ALIGN_TOP_MID, 0, 20);
    
    // Warning message
    lv_obj_t* warnMsg = lv_label_create(screen);
    lv_label_set_text(warnMsg, "This will reset the\ntotalizer to ZERO.");
    lv_obj_set_style_text_font(warnMsg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(warnMsg, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(warnMsg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(warnMsg, LV_ALIGN_TOP_MID, 0, 60);
    
    // Current value
    lv_obj_t* currentLabel = lv_label_create(screen);
    char valBuf[16], unitBuf[8], curBuf[48];
    UnitSystem_t units = userSettings ? userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    formatVolumeWithUnit(valBuf, sizeof(valBuf), unitBuf, sizeof(unitBuf), currentTotal, units);
    snprintf(curBuf, sizeof(curBuf), "Current: %s %s", valBuf, unitBuf);
    lv_label_set_text(currentLabel, curBuf);
    lv_obj_set_style_text_font(currentLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(currentLabel, COLOR_TEXT_LABEL, 0);
    lv_obj_align(currentLabel, LV_ALIGN_TOP_MID, 0, 115);
    
    // Hold instruction
    lv_obj_t* holdInstr = lv_label_create(screen);
    lv_label_set_text(holdInstr, "Hold SELECT for 3 seconds\nto confirm reset.");
    lv_obj_set_style_text_font(holdInstr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(holdInstr, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(holdInstr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(holdInstr, LV_ALIGN_CENTER, 0, 20);
    
    // Progress bar
    lv_obj_t* progContainer = lv_obj_create(screen);
    lv_obj_set_size(progContainer, DISP_WIDTH - 60, 30);
    lv_obj_align(progContainer, LV_ALIGN_CENTER, 0, 70);
    lv_obj_set_style_bg_color(progContainer, COLOR_BAR_BG, 0);
    lv_obj_set_style_border_width(progContainer, 1, 0);
    lv_obj_set_style_border_color(progContainer, COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(progContainer, 5, 0);
    lv_obj_set_style_pad_all(progContainer, 3, 0);
    
    resetProgressBar = lv_bar_create(progContainer);
    lv_obj_set_size(resetProgressBar, DISP_WIDTH - 70, 18);
    lv_obj_align(resetProgressBar, LV_ALIGN_CENTER, 0, 0);
    lv_bar_set_range(resetProgressBar, 0, 100);
    lv_bar_set_value(resetProgressBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(resetProgressBar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(resetProgressBar, COLOR_ALARM_WARNING, LV_PART_INDICATOR);
    
    // Navigation hint
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, "Hold " LV_SYMBOL_OK " 3s to Reset  " LV_SYMBOL_LEFT " Cancel");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}

void display_updateResetProgress(uint8_t percent) {
    if (resetProgressBar != NULL) {
        lv_bar_set_value(resetProgressBar, percent, LV_ANIM_ON);
    }
}

/* ==========================================================================
 * LORA CONFIG SCREENS
 * ========================================================================== */

static int8_t loraMenuSelection = 0;
static lv_obj_t* loraMenuLabels[5] = {NULL};

static void updateLoRaMenuHighlight(void) {
    for (int i = 0; i < 5; i++) {
        if (loraMenuLabels[i] == NULL) continue;
        if (i == loraMenuSelection) {
            lv_obj_set_style_bg_color(lv_obj_get_parent(loraMenuLabels[i]), COLOR_FLOW_FWD, 0);
            lv_obj_set_style_text_color(loraMenuLabels[i], lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_set_style_bg_color(lv_obj_get_parent(loraMenuLabels[i]), COLOR_PANEL_BG, 0);
            lv_obj_set_style_text_color(loraMenuLabels[i], COLOR_TEXT, 0);
        }
    }
}

void display_showLoRaConfig(void) {
    currentScreen = SCREEN_LORA_CONFIG;
    submenuSelection = 0;
    createSubmenuScreen("LoRa Config", loraMenuItems, LORA_MENU_COUNT);
}

void display_showLoRaReportInterval(uint16_t currentValue) {
    currentScreen = SCREEN_LORA_REPORT_INTERVAL;
    loraEditValue = currentValue;
    createSettingsScreen("Report Interval", LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%d sec", loraEditValue);
    lv_label_set_text(settingValueLabel, buf);
}

void display_showLoRaSpreadFactor(void) {
    currentScreen = SCREEN_LORA_SPREAD_FACTOR;
    spreadFactorValue = userSettings ? userSettings->loraSpreadingFactor : DEFAULT_LORA_SF;
    createSettingsScreen("Spreading Factor", LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    
    char buf[16];
    snprintf(buf, sizeof(buf), "SF%d", spreadFactorValue);
    lv_label_set_text(settingValueLabel, buf);
}

void display_showLoRaPing(void) {
    currentScreen = SCREEN_LORA_PING;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Ping Controller");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    lv_obj_t* status = lv_label_create(screen);
    lv_label_set_text(status, "Press SELECT to send\ntest packet...");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status, COLOR_TEXT_LABEL, 0);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_OK " Ping  " LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}

void display_showLoRaPingResult(bool success) {
    // Stay on SCREEN_LORA_PING so back button works
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Ping Result");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    lv_obj_t* result = lv_label_create(screen);
    if (success) {
        lv_label_set_text(result, LV_SYMBOL_OK "\nPing Sent!");
        lv_obj_set_style_text_color(result, COLOR_FLOW_FWD, 0);
    } else {
        lv_label_set_text(result, LV_SYMBOL_CLOSE "\nPing Failed");
        lv_obj_set_style_text_color(result, COLOR_ALARM_CRITICAL, 0);
    }
    lv_obj_set_style_text_font(result, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(result, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(result, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t* note = lv_label_create(screen);
    lv_label_set_text(note, success ? "Packet transmitted" : "Check LoRa connection");
    lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(note, COLOR_TEXT_LABEL, 0);
    lv_obj_align(note, LV_ALIGN_CENTER, 0, 50);
    
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_OK " Retry  " LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}

void display_showLoRaSetSecret(void) {
    currentScreen = SCREEN_LORA_SET_SECRET;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Set Device Secret");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // Instructions for BLE provisioning
    lv_obj_t* instr1 = lv_label_create(screen);
    lv_label_set_text(instr1, "To set the LoRa secret:");
    lv_obj_set_style_text_font(instr1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(instr1, COLOR_TEXT, 0);
    lv_obj_align(instr1, LV_ALIGN_TOP_MID, 0, 60);
    
    lv_obj_t* instr2 = lv_label_create(screen);
    lv_label_set_text(instr2, "1. Open AgSys mobile app\n2. Go to Device Setup\n3. Connect via Bluetooth\n4. Enter property secret");
    lv_obj_set_style_text_font(instr2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instr2, COLOR_TEXT_LABEL, 0);
    lv_obj_set_style_text_align(instr2, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(instr2, LV_ALIGN_CENTER, 0, 10);
    
    lv_obj_t* note = lv_label_create(screen);
    lv_label_set_text(note, "Secret is never displayed");
    lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(note, COLOR_ALARM_WARNING, 0);
    lv_obj_align(note, LV_ALIGN_CENTER, 0, 70);
    
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}

/* ==========================================================================
 * ALARM SETTINGS SCREENS
 * ========================================================================== */

void display_showAlarmLeakThreshold(void) {
    currentScreen = SCREEN_ALARM_LEAK_THRESH;
    alarmEditValue = userSettings ? userSettings->alarmLeakThresholdLPM10 : DEFAULT_ALARM_LEAK_THRESH;
    createSettingsScreen("Leak Threshold", LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f L/min", alarmEditValue / 10.0f);
    lv_label_set_text(settingValueLabel, buf);
}

void display_showAlarmLeakDuration(void) {
    currentScreen = SCREEN_ALARM_LEAK_DURATION;
    alarmEditValue = userSettings ? userSettings->alarmLeakDurationMin : DEFAULT_ALARM_LEAK_DURATION;
    createSettingsScreen("Leak Duration", LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%d min", alarmEditValue);
    lv_label_set_text(settingValueLabel, buf);
}

void display_showAlarmHighFlow(void) {
    currentScreen = SCREEN_ALARM_HIGH_FLOW;
    alarmEditValue = userSettings ? userSettings->alarmHighFlowLPM : DEFAULT_ALARM_HIGH_FLOW;
    createSettingsScreen("High Flow Thresh", LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%d L/min", alarmEditValue);
    lv_label_set_text(settingValueLabel, buf);
}

/* ==========================================================================
 * DIAGNOSTICS SCREENS
 * ========================================================================== */

void display_showDiagnostics(void) {
    currentScreen = SCREEN_DIAGNOSTICS;
    submenuSelection = 0;
    createSubmenuScreen("Diagnostics", diagMenuItems, DIAG_MENU_COUNT);
}

void display_showDiagLoRa(LoRaStats_t* stats) {
    currentScreen = SCREEN_DIAG_LORA;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 10, 0);
    
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "LoRa Status");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_FLOW_FWD, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    char infoBuf[256];
    snprintf(infoBuf, sizeof(infoBuf),
             "Status:   %s\n"
             "Last TX:  %lu sec\n"
             "Last RX:  %lu sec\n"
             "TX Count: %lu\n"
             "RX Count: %lu\n"
             "Errors:   %lu\n\n"
             "RSSI: %d dBm\n"
             "SNR:  %.1f dB",
             stats->connected ? "Connected" : "Disconnected",
             (unsigned long)stats->lastTxSec, (unsigned long)stats->lastRxSec,
             (unsigned long)stats->txCount, (unsigned long)stats->rxCount,
             (unsigned long)stats->errorCount,
             stats->rssi, stats->snr);
    
    lv_obj_t* info = lv_label_create(screen);
    lv_label_set_text(info, infoBuf);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info, COLOR_TEXT, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 10, 40);
    
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_OK " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}

void display_showDiagADC(ADCValues_t* values) {
    currentScreen = SCREEN_DIAG_ADC;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 10, 0);
    
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "ADC Values");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COLOR_FLOW_FWD, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    char infoBuf[256];
    snprintf(infoBuf, sizeof(infoBuf),
             "CH1: %+ld\n"
             "CH2: %+ld\n"
             "Diff: %+ld\n\n"
             "Temp: %.1f C\n"
             "Zero: %+ld\n"
             "Span: %.3f\n\n"
             "Raw:  %.2f L/min\n"
             "Cal:  %.2f L/min",
             (long)values->ch1Raw, (long)values->ch2Raw, (long)values->diffRaw,
             values->temperatureC, (long)values->zeroOffset, values->spanFactor,
             values->flowRaw, values->flowCal);
    
    lv_obj_t* info = lv_label_create(screen);
    lv_label_set_text(info, infoBuf);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info, COLOR_TEXT, 0);
    lv_obj_align(info, LV_ALIGN_TOP_LEFT, 10, 40);
    
    lv_obj_t* hint = lv_label_create(screen);
    lv_label_set_text(hint, LV_SYMBOL_OK " Back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}
