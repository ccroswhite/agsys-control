/**
 * @file display_new.c
 * @brief Display orchestration layer for water meter
 * 
 * Thin layer that initializes the display, creates all screens,
 * and routes events to the appropriate screen modules.
 * 
 * This replaces the monolithic display.c with a modular architecture.
 */

#include "display.h"
#include "ui/ui_display_driver.h"
#include "ui/ui_common.h"
#include "ui/ui_screens.h"
#include "st7789.h"
#include "board_config.h"
#include <string.h>

/* ==========================================================================
 * DISPLAY DRIVER (ST7789)
 * ========================================================================== */

static void st7789_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    st7789_set_addr_window(area->x1, area->y1, area->x2, area->y2);
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    st7789_write_pixels((const uint16_t *)color_p, size);
    lv_display_flush_ready(disp);
}

static const ui_display_driver_t m_st7789_driver = {
    .width = DISPLAY_WIDTH,
    .height = DISPLAY_HEIGHT,
    .init = st7789_init,
    .flush = st7789_flush_cb,
    .set_backlight = st7789_set_backlight,
    .sleep = st7789_sleep,
    .wake = st7789_wake,
};

/* ==========================================================================
 * STATE
 * ========================================================================== */

static ScreenId_t m_current_screen = SCREEN_MAIN;
static UserSettings_t *m_settings = NULL;
static DisplayPowerState_t m_power_state = DISPLAY_ACTIVE;
static uint32_t m_last_input_ms = 0;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool display_init(void)
{
    /* Register and initialize display driver */
    ui_display_register_driver(&m_st7789_driver);
    if (!ui_display_init()) {
        return false;
    }
    
    /* Create all screens */
    ui_main_create();
    ui_menu_create();
    ui_ota_create();
    ui_cal_create();
    ui_diag_create();
    ui_settings_create();
    ui_misc_create();
    ui_pin_create();
    
    /* Show splash then main screen */
    ui_main_show();
    m_current_screen = SCREEN_MAIN;
    
    return true;
}

void display_tick(void)
{
    ui_display_tick(5);  /* 5ms tick */
}

void display_task_handler(void)
{
    ui_display_task_handler();
    ui_status_tick_ble();
    
    /* Check OTA error timeout */
    if (ui_ota_is_error_active()) {
        if (ui_ota_tick_error()) {
            display_showMain();
        }
    }
}

/* ==========================================================================
 * SCREEN NAVIGATION
 * ========================================================================== */

void display_showSplash(void)
{
    ui_misc_show_splash();
}

void display_showMain(void)
{
    ui_main_show();
    m_current_screen = SCREEN_MAIN;
}

void display_updateMain(const FlowData_t *data)
{
    ui_main_update(data);
}

void display_showMenu(void)
{
    if (ui_menu_is_locked()) {
        display_showMenuLocked();
        return;
    }
    ui_menu_show();
    m_current_screen = SCREEN_MENU;
}

void display_showMenuLocked(void)
{
    display_showPinOverlay();
}

static void pin_result_callback(bool success)
{
    if (success) {
        ui_menu_unlock();
        ui_menu_show();
        m_current_screen = SCREEN_MENU;
    } else {
        display_showMain();
    }
}

void display_showPinOverlay(void)
{
    uint32_t pin = m_settings ? m_settings->menuPin : 0;
    ui_pin_show(pin, pin_result_callback);
    m_current_screen = SCREEN_MENU_LOCKED;
}

void display_hidePinOverlay(void)
{
    ui_pin_hide();
}

void display_showDisplaySettings(void)
{
    ui_settings_show_display();
    m_current_screen = SCREEN_DISPLAY_SETTINGS;
}

void display_showFlowSettings(void)
{
    ui_settings_show_flow();
    m_current_screen = SCREEN_FLOW_SETTINGS;
}

void display_showAlarmSettings(void)
{
    ui_settings_show_alarm();
    m_current_screen = SCREEN_ALARM_SETTINGS;
}

void display_showLoRaConfig(void)
{
    ui_settings_show_lora();
    m_current_screen = SCREEN_LORA_CONFIG;
}

void display_showCalibration(void)
{
    ui_cal_show_menu();
    m_current_screen = SCREEN_CALIBRATION;
}

void display_showCalZero(void)
{
    ui_cal_show_zero();
    m_current_screen = SCREEN_CAL_ZERO;
}

void display_showCalSpan(void)
{
    ui_cal_show_span();
    m_current_screen = SCREEN_CAL_SPAN;
}

void display_showCalPipeSize(void)
{
    ui_cal_show_pipe_size();
    m_current_screen = SCREEN_CAL_PIPE_SIZE;
}

void display_showCalDutyCycle(void)
{
    ui_cal_show_duty_cycle();
    m_current_screen = SCREEN_CAL_DUTY_CYCLE;
}

void display_showCalView(void)
{
    ui_cal_show_view();
    m_current_screen = SCREEN_CAL_VIEW;
}

void display_showTotalizer(float totalLiters)
{
    ui_misc_show_totalizer(totalLiters);
    m_current_screen = SCREEN_TOTALIZER;
}

void display_showDiagnostics(void)
{
    ui_diag_show_menu();
    m_current_screen = SCREEN_DIAGNOSTICS;
}

void display_showAbout(void)
{
    ui_misc_show_about("1.0.0", __DATE__);
    m_current_screen = SCREEN_ABOUT;
}

/* ==========================================================================
 * OTA SCREENS
 * ========================================================================== */

void display_showOTAProgress(uint8_t percent, const char *status, const char *version)
{
    ui_ota_show_progress(percent, status, version);
    m_current_screen = SCREEN_OTA_PROGRESS;
}

void display_updateOTAProgress(uint8_t percent)
{
    ui_ota_update_progress(percent);
}

void display_updateOTAStatus(const char *status)
{
    ui_ota_update_status(status);
}

void display_showOTAError(const char *error_msg)
{
    ui_ota_show_error(error_msg);
}

bool display_isOTAErrorActive(void)
{
    return ui_ota_is_error_active();
}

bool display_tickOTAError(void)
{
    return ui_ota_tick_error();
}

/* ==========================================================================
 * ERROR DISPLAY
 * ========================================================================== */

void display_showError(const char *message)
{
    ui_misc_show_error(message);
}

/* ==========================================================================
 * ALARM OVERLAY
 * ========================================================================== */

void display_showAlarm(AlarmType_t alarmType, uint32_t durationSec,
                       float flowRateLPM, float volumeLiters)
{
    ui_alarm_show(alarmType, durationSec, flowRateLPM, volumeLiters);
}

void display_acknowledgeAlarm(void)
{
    ui_alarm_acknowledge();
}

void display_dismissAlarm(void)
{
    ui_alarm_dismiss();
}

bool display_isAlarmActive(void)
{
    return ui_alarm_is_active();
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

void display_handleButton(ButtonEvent_t event)
{
    display_resetActivityTimer();
    
    switch (m_current_screen) {
        case SCREEN_MAIN:
            if (ui_main_handle_button(event)) {
                display_showMenu();
            }
            break;
            
        case SCREEN_MENU: {
            ScreenId_t next = ui_menu_handle_button(event);
            if (next != SCREEN_MENU) {
                switch (next) {
                    case SCREEN_MAIN:
                        display_showMain();
                        break;
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
                    case SCREEN_TOTALIZER:
                        display_showTotalizer(0);
                        break;
                    case SCREEN_DIAGNOSTICS:
                        display_showDiagnostics();
                        break;
                    case SCREEN_ABOUT:
                        display_showAbout();
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        
        case SCREEN_MENU_LOCKED:
            ui_pin_handle_button(event);
            break;
            
        case SCREEN_CALIBRATION:
        case SCREEN_CAL_ZERO:
        case SCREEN_CAL_SPAN:
        case SCREEN_CAL_PIPE_SIZE:
        case SCREEN_CAL_DUTY_CYCLE:
        case SCREEN_CAL_VIEW: {
            ScreenId_t next = ui_cal_handle_button(event);
            if (next == SCREEN_MENU) {
                display_showMenu();
            } else {
                m_current_screen = next;
            }
            break;
        }
        
        case SCREEN_DIAGNOSTICS:
        case SCREEN_DIAG_LORA:
        case SCREEN_DIAG_ADC: {
            ScreenId_t next = ui_diag_handle_button(event);
            if (next == SCREEN_MENU) {
                display_showMenu();
            } else {
                m_current_screen = next;
            }
            break;
        }
        
        case SCREEN_DISPLAY_SETTINGS:
        case SCREEN_FLOW_SETTINGS:
        case SCREEN_ALARM_SETTINGS:
        case SCREEN_LORA_CONFIG: {
            ScreenId_t next = ui_settings_handle_button(event);
            if (next == SCREEN_MENU) {
                display_showMenu();
            } else {
                m_current_screen = next;
            }
            break;
        }
        
        case SCREEN_TOTALIZER:
        case SCREEN_ABOUT: {
            ScreenId_t next = ui_misc_handle_button(event);
            if (next == SCREEN_MENU) {
                display_showMenu();
            } else if (next == SCREEN_MAIN) {
                display_showMain();
            } else {
                m_current_screen = next;
            }
            break;
        }
        
        default:
            /* For unimplemented screens, BACK returns to menu */
            if (event == BTN_LEFT_SHORT || event == BTN_LEFT_LONG) {
                display_showMenu();
            }
            break;
    }
}

ScreenId_t display_getCurrentScreen(void)
{
    return m_current_screen;
}

/* ==========================================================================
 * SETTINGS
 * ========================================================================== */

void display_setSettings(UserSettings_t *settings)
{
    m_settings = settings;
    ui_main_set_settings(settings);
    ui_settings_set_ref(settings);
}

/* ==========================================================================
 * POWER MANAGEMENT
 * ========================================================================== */

void display_updatePowerState(void)
{
    uint32_t now = lv_tick_get();
    uint32_t idle_sec = (now - m_last_input_ms) / 1000;
    
    /* Use default timeouts */
    if (m_power_state == DISPLAY_ACTIVE && 
        idle_sec >= DEFAULT_DIM_TIMEOUT_SEC) {
        m_power_state = DISPLAY_DIM;
        ui_display_set_backlight(30);
    }
    
    if (m_power_state == DISPLAY_DIM &&
        idle_sec >= DEFAULT_SLEEP_TIMEOUT_SEC) {
        m_power_state = DISPLAY_SLEEP;
        ui_display_sleep();
    }
}

void display_wake(void)
{
    if (m_power_state != DISPLAY_ACTIVE) {
        m_power_state = DISPLAY_ACTIVE;
        ui_display_wake();
        ui_display_set_backlight(100);
    }
    m_last_input_ms = lv_tick_get();
}

void display_resetActivityTimer(void)
{
    display_wake();
}

DisplayPowerState_t display_getPowerState(void)
{
    return m_power_state;
}

/* ==========================================================================
 * MENU LOCK
 * ========================================================================== */

bool display_isMenuLocked(void)
{
    return ui_menu_is_locked();
}

void display_lockMenu(void)
{
    ui_menu_lock();
}

void display_unlockMenuRemote(void)
{
    ui_menu_unlock();
}

/* ==========================================================================
 * STATUS BAR
 * ========================================================================== */

void display_updateStatusBar(bool loraConnected, bool hasAlarm,
                             AlarmType_t alarmType, uint32_t lastReportSec)
{
    ui_main_update_status_bar(loraConnected, hasAlarm, alarmType, lastReportSec);
}

void display_showDiagLoRa(const LoRaStats_t *stats)
{
    ui_diag_show_lora();
    ui_diag_update_lora(stats);
    m_current_screen = SCREEN_DIAG_LORA;
}

void display_showDiagADC(const ADCValues_t *values)
{
    ui_diag_show_adc();
    ui_diag_update_adc(values);
    m_current_screen = SCREEN_DIAG_ADC;
}

/* ==========================================================================
 * BLE STATUS
 * ========================================================================== */

void display_updateBleStatus(BleUiState_t state)
{
    ui_status_update_ble(state);
}

BleUiState_t display_getBleStatus(void)
{
    return ui_status_get_ble();
}

void display_tickBleIcon(void)
{
    ui_status_tick_ble();
}
