/**
 * @file display.cpp
 * @brief Display implementation for Mag Meter using LVGL and ST7789
 * 
 * Light theme optimized for transflective display daylight readability.
 * Layout based on user mockup with flow rate, trend, avg, and total volume.
 */

#include "display.h"
#include "magmeter_config.h"
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

// Menu screen elements
static lv_obj_t* screen_menu = NULL;
static int8_t menuSelection = 0;

// Tier names
static const char* TIER_NAMES[] = {"MM-S", "MM-M", "MM-L"};

// Forward declarations for static functions
static void updateMenuHighlight(void);
static void updateSettingsUnitsDisplay(void);
static void updateSettingsTrendDisplay(void);
static void updateSettingsAvgDisplay(void);
static void updateSettingsMaxFlowDisplay(void);
static void createSettingsScreen(const char* title, const char* hint);
static void updateCalMenuHighlight(void);
static void updateCalSpanDisplay(void);

// Calibration screen elements
static lv_obj_t* calLabels[3] = {NULL};
static int8_t calMenuSelection = 0;
static float calSpanValue = 1.0f;
static lv_obj_t* calValueLabel = NULL;

// Calibration callbacks (implemented in calibration.cpp)
extern void calibration_captureZero(void);
extern void calibration_setSpan(float span);

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
    
    // Create main screen with light background
    screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_main, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen_main, 0, 0);
    
    // ===== TOP SECTION: Current Flow Rate (large) =====
    lv_obj_t* flow_section = lv_obj_create(screen_main);
    lv_obj_set_size(flow_section, DISP_WIDTH, 100);
    lv_obj_align(flow_section, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(flow_section, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(flow_section, 0, 0);
    lv_obj_set_style_radius(flow_section, 0, 0);
    lv_obj_set_style_pad_all(flow_section, 5, 0);
    
    // Flow value (large)
    label_flow_value = lv_label_create(flow_section);
    lv_label_set_text(label_flow_value, "0.0");
    lv_obj_set_style_text_font(label_flow_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_flow_value, COLOR_TEXT, 0);
    lv_obj_align(label_flow_value, LV_ALIGN_TOP_MID, 0, 5);
    
    // Flow unit
    label_flow_unit = lv_label_create(flow_section);
    UnitSystem_t units = userSettings ? userSettings->unitSystem : UNIT_SYSTEM_METRIC;
    lv_label_set_text(label_flow_unit, getFlowUnitStr(units));
    lv_obj_set_style_text_font(label_flow_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_flow_unit, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_flow_unit, LV_ALIGN_TOP_MID, 0, 40);
    
    // "Current Vol" label
    lv_obj_t* label_current = lv_label_create(flow_section);
    lv_label_set_text(label_current, "Current Vol");
    lv_obj_set_style_text_font(label_current, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_current, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_current, LV_ALIGN_TOP_MID, 0, 55);
    
    // ===== FLOW BAR with arrow =====
    lv_obj_t* bar_container = lv_obj_create(screen_main);
    lv_obj_set_size(bar_container, DISP_WIDTH - 20, 30);
    lv_obj_align(bar_container, LV_ALIGN_TOP_MID, 0, 105);
    lv_obj_set_style_bg_color(bar_container, COLOR_BAR_BG, 0);
    lv_obj_set_style_border_width(bar_container, 1, 0);
    lv_obj_set_style_border_color(bar_container, COLOR_DIVIDER, 0);
    lv_obj_set_style_radius(bar_container, 4, 0);
    lv_obj_set_style_pad_all(bar_container, 2, 0);
    
    // Flow bar (fills based on % of max)
    obj_flow_bar = lv_bar_create(bar_container);
    lv_obj_set_size(obj_flow_bar, DISP_WIDTH - 60, 20);
    lv_obj_align(obj_flow_bar, LV_ALIGN_LEFT_MID, 0, 0);
    lv_bar_set_range(obj_flow_bar, 0, 100);
    lv_bar_set_value(obj_flow_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(obj_flow_bar, COLOR_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj_flow_bar, COLOR_FLOW_FWD, LV_PART_INDICATOR);
    
    // Flow arrow
    obj_flow_arrow = lv_label_create(bar_container);
    lv_label_set_text(obj_flow_arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(obj_flow_arrow, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(obj_flow_arrow, COLOR_FLOW_IDLE, 0);
    lv_obj_align(obj_flow_arrow, LV_ALIGN_RIGHT_MID, -5, 0);
    
    // ===== MIDDLE SECTION: Trend | Avg (split) =====
    // Horizontal divider
    lv_obj_t* divider1 = lv_obj_create(screen_main);
    lv_obj_set_size(divider1, DISP_WIDTH, 2);
    lv_obj_align(divider1, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_set_style_bg_color(divider1, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(divider1, 0, 0);
    lv_obj_set_style_radius(divider1, 0, 0);
    
    // Left panel: Trend
    lv_obj_t* trend_panel = lv_obj_create(screen_main);
    lv_obj_set_size(trend_panel, DISP_WIDTH / 2 - 1, 80);
    lv_obj_align(trend_panel, LV_ALIGN_TOP_LEFT, 0, 142);
    lv_obj_set_style_bg_color(trend_panel, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(trend_panel, 0, 0);
    lv_obj_set_style_radius(trend_panel, 0, 0);
    lv_obj_set_style_pad_all(trend_panel, 5, 0);
    
    label_trend_value = lv_label_create(trend_panel);
    lv_label_set_text(label_trend_value, "+0.0L");
    lv_obj_set_style_text_font(label_trend_value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_trend_value, COLOR_TEXT, 0);
    lv_obj_align(label_trend_value, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t* label_trend = lv_label_create(trend_panel);
    lv_label_set_text(label_trend, "Trend");
    lv_obj_set_style_text_font(label_trend, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_trend, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_trend, LV_ALIGN_CENTER, 0, 15);
    
    // Vertical divider
    lv_obj_t* vdivider = lv_obj_create(screen_main);
    lv_obj_set_size(vdivider, 2, 80);
    lv_obj_align(vdivider, LV_ALIGN_TOP_MID, 0, 142);
    lv_obj_set_style_bg_color(vdivider, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(vdivider, 0, 0);
    
    // Right panel: Avg Vol
    lv_obj_t* avg_panel = lv_obj_create(screen_main);
    lv_obj_set_size(avg_panel, DISP_WIDTH / 2 - 1, 80);
    lv_obj_align(avg_panel, LV_ALIGN_TOP_RIGHT, 0, 142);
    lv_obj_set_style_bg_color(avg_panel, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(avg_panel, 0, 0);
    lv_obj_set_style_radius(avg_panel, 0, 0);
    lv_obj_set_style_pad_all(avg_panel, 5, 0);
    
    label_avg_value = lv_label_create(avg_panel);
    lv_label_set_text(label_avg_value, "0.0L");
    lv_obj_set_style_text_font(label_avg_value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_avg_value, COLOR_TEXT, 0);
    lv_obj_align(label_avg_value, LV_ALIGN_CENTER, 0, -10);
    
    lv_obj_t* label_avg = lv_label_create(avg_panel);
    lv_label_set_text(label_avg, "AVG Vol");
    lv_obj_set_style_text_font(label_avg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_avg, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_avg, LV_ALIGN_CENTER, 0, 15);
    
    // ===== BOTTOM SECTION: Total Volume (large) =====
    // Horizontal divider
    lv_obj_t* divider2 = lv_obj_create(screen_main);
    lv_obj_set_size(divider2, DISP_WIDTH, 2);
    lv_obj_align(divider2, LV_ALIGN_TOP_MID, 0, 222);
    lv_obj_set_style_bg_color(divider2, COLOR_DIVIDER, 0);
    lv_obj_set_style_border_width(divider2, 0, 0);
    lv_obj_set_style_radius(divider2, 0, 0);
    
    lv_obj_t* total_section = lv_obj_create(screen_main);
    lv_obj_set_size(total_section, DISP_WIDTH, 96);
    lv_obj_align(total_section, LV_ALIGN_TOP_MID, 0, 224);
    lv_obj_set_style_bg_color(total_section, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(total_section, 0, 0);
    lv_obj_set_style_radius(total_section, 0, 0);
    lv_obj_set_style_pad_all(total_section, 5, 0);
    
    // Total value (large)
    label_total_value = lv_label_create(total_section);
    lv_label_set_text(label_total_value, "0.0");
    lv_obj_set_style_text_font(label_total_value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_total_value, COLOR_TEXT, 0);
    lv_obj_align(label_total_value, LV_ALIGN_CENTER, 0, -15);
    
    // Total unit
    label_total_unit = lv_label_create(total_section);
    lv_label_set_text(label_total_unit, "L");
    lv_obj_set_style_text_font(label_total_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_total_unit, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_total_unit, LV_ALIGN_CENTER, 0, 15);
    
    // "Total Vol" label
    lv_obj_t* label_total = lv_label_create(total_section);
    lv_label_set_text(label_total, "Total Vol");
    lv_obj_set_style_text_font(label_total, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_total, COLOR_TEXT_LABEL, 0);
    lv_obj_align(label_total, LV_ALIGN_CENTER, 0, 35);
    
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

// Menu items
#define MENU_ITEM_COUNT 6
static const char* menuItems[MENU_ITEM_COUNT] = {
    "Units",
    "Trend Period",
    "Avg Period", 
    "Max Flow",
    "Calibration",
    "About"
};

static lv_obj_t* menuLabels[MENU_ITEM_COUNT] = {NULL};
static lv_obj_t* settingValueLabel = NULL;
static int settingEditValue = 0;

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

void display_handleButton(ButtonEvent_t event) {
    switch (currentScreen) {
        case SCREEN_MAIN:
            if (event == BTN_SELECT_SHORT) {
                display_showMenu();
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
                switch (menuSelection) {
                    case 0: display_showSettingsUnits(); break;
                    case 1: display_showSettingsTrend(); break;
                    case 2: display_showSettingsAvg(); break;
                    case 3: display_showSettingsMaxFlow(); break;
                    case 4: display_showCalibration(); break;
                    case 5: display_showAbout(); break;
                }
            } else if (event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMain();
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
                }
                display_showMenu();
            } else if (event == BTN_SELECT_LONG) {
                display_showMenu();
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
                }
                display_showMenu();
            } else if (event == BTN_SELECT_LONG) {
                display_showMenu();
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
                }
                display_showMenu();
            } else if (event == BTN_SELECT_LONG) {
                display_showMenu();
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
                }
                display_showMenu();
            } else if (event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
            
        case SCREEN_ABOUT:
            if (event == BTN_SELECT_SHORT || event == BTN_LEFT_SHORT || event == BTN_SELECT_LONG) {
                display_showMenu();
            }
            break;
            
        case SCREEN_CALIBRATION:
            if (event == BTN_UP_SHORT) {
                calMenuSelection--;
                if (calMenuSelection < 0) calMenuSelection = 2;
                updateCalMenuHighlight();
            } else if (event == BTN_DOWN_SHORT) {
                calMenuSelection++;
                if (calMenuSelection > 2) calMenuSelection = 0;
                updateCalMenuHighlight();
            } else if (event == BTN_SELECT_SHORT) {
                switch (calMenuSelection) {
                    case 0: display_showCalZero(); break;
                    case 1: display_showCalSpan(); break;
                    case 2: display_showMenu(); break;  // Back
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
            
        case SCREEN_CAL_SPAN:
            if (event == BTN_UP_SHORT || event == BTN_RIGHT_SHORT) {
                calSpanValue += 0.01f;
                if (calSpanValue > 2.0f) calSpanValue = 2.0f;
                updateCalSpanDisplay();
            } else if (event == BTN_DOWN_SHORT || event == BTN_LEFT_SHORT) {
                calSpanValue -= 0.01f;
                if (calSpanValue < 0.5f) calSpanValue = 0.5f;
                updateCalSpanDisplay();
            } else if (event == BTN_SELECT_SHORT) {
                calibration_setSpan(calSpanValue);
                display_showCalibration();
            } else if (event == BTN_SELECT_LONG) {
                display_showCalibration();
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
    
    // Menu items
    int yPos = 50;
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        lv_obj_t* item = lv_obj_create(screen);
        lv_obj_set_size(item, DISP_WIDTH - 20, 40);
        lv_obj_align(item, LV_ALIGN_TOP_MID, 0, yPos);
        lv_obj_set_style_bg_color(item, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_color(item, COLOR_DIVIDER, 0);
        lv_obj_set_style_radius(item, 5, 0);
        lv_obj_set_style_pad_left(item, 10, 0);
        
        menuLabels[i] = lv_label_create(item);
        lv_label_set_text(menuLabels[i], menuItems[i]);
        lv_obj_set_style_text_font(menuLabels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(menuLabels[i], COLOR_TEXT, 0);
        lv_obj_align(menuLabels[i], LV_ALIGN_LEFT_MID, 0, 0);
        
        yPos += 45;
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
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    
    // Title bar - orange for caution
    lv_obj_t* titleBar = lv_obj_create(screen);
    lv_obj_set_size(titleBar, DISP_WIDTH, 40);
    lv_obj_align(titleBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(titleBar, lv_color_hex(0xCC6600), 0);
    lv_obj_set_style_border_width(titleBar, 0, 0);
    lv_obj_set_style_radius(titleBar, 0, 0);
    
    lv_obj_t* title = lv_label_create(titleBar);
    lv_label_set_text(title, "Calibration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    
    // Warning
    lv_obj_t* warning = lv_label_create(screen);
    lv_label_set_text(warning, LV_SYMBOL_WARNING " Affects accuracy");
    lv_obj_set_style_text_font(warning, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(warning, lv_color_hex(0xCC6600), 0);
    lv_obj_align(warning, LV_ALIGN_TOP_MID, 0, 50);
    
    // Calibration menu items
    const char* calItems[] = {"Zero Offset", "Span Factor", "Back"};
    int yPos = 80;
    for (int i = 0; i < 3; i++) {
        lv_obj_t* item = lv_obj_create(screen);
        lv_obj_set_size(item, DISP_WIDTH - 20, 45);
        lv_obj_align(item, LV_ALIGN_TOP_MID, 0, yPos);
        lv_obj_set_style_bg_color(item, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_color(item, COLOR_DIVIDER, 0);
        lv_obj_set_style_radius(item, 5, 0);
        lv_obj_set_style_pad_left(item, 10, 0);
        
        calLabels[i] = lv_label_create(item);
        lv_label_set_text(calLabels[i], calItems[i]);
        lv_obj_set_style_text_font(calLabels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(calLabels[i], COLOR_TEXT, 0);
        lv_obj_align(calLabels[i], LV_ALIGN_LEFT_MID, 0, 0);
        
        yPos += 50;
    }
    
    // Navigation hint
    lv_obj_t* calHint = lv_label_create(screen);
    lv_label_set_text(calHint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate  " LV_SYMBOL_OK " Select");
    lv_obj_set_style_text_font(calHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(calHint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(calHint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    updateCalMenuHighlight();
    lv_scr_load(screen);
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

void display_showCalSpan(void) {
    currentScreen = SCREEN_CAL_SPAN;
    calSpanValue = 1.0f;
    
    lv_obj_t* screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    
    lv_obj_t* spanTitle = lv_label_create(screen);
    lv_label_set_text(spanTitle, "Span Factor");
    lv_obj_set_style_text_font(spanTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(spanTitle, lv_color_hex(0xCC6600), 0);
    lv_obj_align(spanTitle, LV_ALIGN_TOP_MID, 0, 20);
    
    lv_obj_t* spanInstr = lv_label_create(screen);
    lv_label_set_text(spanInstr, "Adjust to match\nreference meter");
    lv_obj_set_style_text_font(spanInstr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(spanInstr, COLOR_TEXT_LABEL, 0);
    lv_obj_set_style_text_align(spanInstr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(spanInstr, LV_ALIGN_TOP_MID, 0, 60);
    
    lv_obj_t* valuePanel = lv_obj_create(screen);
    lv_obj_set_size(valuePanel, DISP_WIDTH - 40, 80);
    lv_obj_align(valuePanel, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(valuePanel, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(valuePanel, 2, 0);
    lv_obj_set_style_border_color(valuePanel, lv_color_hex(0xCC6600), 0);
    lv_obj_set_style_radius(valuePanel, 10, 0);
    
    calValueLabel = lv_label_create(valuePanel);
    lv_obj_set_style_text_font(calValueLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(calValueLabel, COLOR_TEXT, 0);
    lv_obj_align(calValueLabel, LV_ALIGN_CENTER, 0, 0);
    updateCalSpanDisplay();
    
    lv_obj_t* rangeLabel = lv_label_create(screen);
    lv_label_set_text(rangeLabel, "Range: 0.50 - 2.00");
    lv_obj_set_style_text_font(rangeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(rangeLabel, COLOR_TEXT_LABEL, 0);
    lv_obj_align(rangeLabel, LV_ALIGN_CENTER, 0, 80);
    
    lv_obj_t* spanHint = lv_label_create(screen);
    lv_label_set_text(spanHint, LV_SYMBOL_UP LV_SYMBOL_DOWN " Adjust  " LV_SYMBOL_OK " Save");
    lv_obj_set_style_text_font(spanHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(spanHint, COLOR_TEXT_LABEL, 0);
    lv_obj_align(spanHint, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_scr_load(screen);
}
