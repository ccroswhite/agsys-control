/**
 * @file ui_screen_diag.c
 * @brief Diagnostics screens for water meter
 */

#include "ui_screen_diag.h"
#include "ui/ui_common.h"
#include <stdio.h>

/* ==========================================================================
 * DIAGNOSTICS MENU ITEMS
 * ========================================================================== */

typedef enum {
    DIAG_MENU_LORA = 0,
    DIAG_MENU_ADC,
    DIAG_MENU_COUNT
} diag_menu_item_t;

static const char *m_diag_menu_text[] = {
    "LoRa Status",
    "ADC Values"
};

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

static lv_obj_t *m_menu_screen = NULL;
static lv_obj_t *m_menu_list = NULL;
static int8_t m_menu_selection = 0;

/* LoRa screen */
static lv_obj_t *m_lora_screen = NULL;
static lv_obj_t *m_lora_status_label = NULL;
static lv_obj_t *m_lora_rssi_label = NULL;
static lv_obj_t *m_lora_snr_label = NULL;
static lv_obj_t *m_lora_tx_label = NULL;
static lv_obj_t *m_lora_rx_label = NULL;
static lv_obj_t *m_lora_err_label = NULL;

/* ADC screen */
static lv_obj_t *m_adc_screen = NULL;
static lv_obj_t *m_adc_ch1_label = NULL;
static lv_obj_t *m_adc_ch2_label = NULL;
static lv_obj_t *m_adc_diff_label = NULL;
static lv_obj_t *m_adc_temp_label = NULL;
static lv_obj_t *m_adc_flow_label = NULL;

static ScreenId_t m_current_diag_screen = SCREEN_DIAGNOSTICS;

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static void refresh_menu(void)
{
    if (m_menu_list == NULL) return;
    lv_obj_clean(m_menu_list);
    
    for (int i = 0; i < DIAG_MENU_COUNT; i++) {
        ui_add_menu_item(m_menu_list, m_diag_menu_text[i], i, m_menu_selection);
    }
}

/* ==========================================================================
 * SCREEN CREATION
 * ========================================================================== */

void ui_diag_create(void)
{
    lv_obj_t *content;
    
    /* ===== Diagnostics Menu ===== */
    m_menu_screen = ui_create_screen_with_header("Diagnostics", &content);
    m_menu_list = ui_create_menu_list(content);
    refresh_menu();
    
    /* ===== LoRa Screen ===== */
    m_lora_screen = ui_create_screen_with_header("LoRa Status", &content);
    
    m_lora_status_label = ui_create_label(content, "Status: --",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_lora_status_label, 15, 0);
    
    m_lora_rssi_label = ui_create_label(content, "RSSI: -- dBm",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_lora_rssi_label, 8, 0);
    
    m_lora_snr_label = ui_create_label(content, "SNR: -- dB",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_lora_snr_label, 8, 0);
    
    m_lora_tx_label = ui_create_label(content, "TX Count: 0",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_lora_tx_label, 8, 0);
    
    m_lora_rx_label = ui_create_label(content, "RX Count: 0",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_lora_rx_label, 8, 0);
    
    m_lora_err_label = ui_create_label(content, "Errors: 0",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_lora_err_label, 8, 0);
    
    /* ===== ADC Screen ===== */
    m_adc_screen = ui_create_screen_with_header("ADC Values", &content);
    
    m_adc_ch1_label = ui_create_label(content, "CH1: 0",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_adc_ch1_label, 15, 0);
    
    m_adc_ch2_label = ui_create_label(content, "CH2: 0",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_adc_ch2_label, 8, 0);
    
    m_adc_diff_label = ui_create_label(content, "Diff: 0",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_adc_diff_label, 8, 0);
    
    m_adc_temp_label = ui_create_label(content, "Temp: -- C",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_adc_temp_label, 8, 0);
    
    m_adc_flow_label = ui_create_label(content, "Flow: 0.00 LPM",
        UI_FONT_XLARGE, UI_COLOR_ACCENT);
    lv_obj_set_style_pad_top(m_adc_flow_label, 20, 0);
}

/* ==========================================================================
 * SHOW FUNCTIONS
 * ========================================================================== */

void ui_diag_show_menu(void)
{
    m_menu_selection = 0;
    refresh_menu();
    m_current_diag_screen = SCREEN_DIAGNOSTICS;
    lv_scr_load(m_menu_screen);
}

void ui_diag_show_lora(void)
{
    m_current_diag_screen = SCREEN_DIAG_LORA;
    lv_scr_load(m_lora_screen);
}

void ui_diag_show_adc(void)
{
    m_current_diag_screen = SCREEN_DIAG_ADC;
    lv_scr_load(m_adc_screen);
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

ScreenId_t ui_diag_handle_button(ButtonEvent_t event)
{
    switch (m_current_diag_screen) {
        case SCREEN_DIAGNOSTICS: {
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
                    if (m_menu_selection < DIAG_MENU_COUNT - 1) {
                        m_menu_selection++;
                        ui_menu_update_selection(m_menu_list, old_sel, m_menu_selection);
                    }
                    break;
                case BTN_SELECT_SHORT:
                case BTN_RIGHT_SHORT:
                    switch (m_menu_selection) {
                        case DIAG_MENU_LORA:
                            ui_diag_show_lora();
                            return SCREEN_DIAG_LORA;
                        case DIAG_MENU_ADC:
                            ui_diag_show_adc();
                            return SCREEN_DIAG_ADC;
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
        
        case SCREEN_DIAG_LORA:
        case SCREEN_DIAG_ADC:
            if (event == BTN_LEFT_SHORT || event == BTN_LEFT_LONG) {
                ui_diag_show_menu();
                return SCREEN_DIAGNOSTICS;
            }
            break;
            
        default:
            break;
    }
    
    return m_current_diag_screen;
}

/* ==========================================================================
 * UPDATE FUNCTIONS
 * ========================================================================== */

void ui_diag_update_lora(const LoRaStats_t *stats)
{
    if (stats == NULL) return;
    
    char buf[64];
    
    lv_label_set_text(m_lora_status_label, 
        stats->connected ? "Status: Connected" : "Status: Disconnected");
    lv_obj_set_style_text_color(m_lora_status_label,
        stats->connected ? UI_COLOR_SUCCESS : UI_COLOR_ERROR, 0);
    
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", stats->rssi);
    lv_label_set_text(m_lora_rssi_label, buf);
    
    snprintf(buf, sizeof(buf), "SNR: %.1f dB", (double)stats->snr);
    lv_label_set_text(m_lora_snr_label, buf);
    
    snprintf(buf, sizeof(buf), "TX Count: %lu", (unsigned long)stats->txCount);
    lv_label_set_text(m_lora_tx_label, buf);
    
    snprintf(buf, sizeof(buf), "RX Count: %lu", (unsigned long)stats->rxCount);
    lv_label_set_text(m_lora_rx_label, buf);
    
    snprintf(buf, sizeof(buf), "Errors: %lu", (unsigned long)stats->errorCount);
    lv_label_set_text(m_lora_err_label, buf);
    if (stats->errorCount > 0) {
        lv_obj_set_style_text_color(m_lora_err_label, UI_COLOR_WARNING, 0);
    }
}

void ui_diag_update_adc(const ADCValues_t *values)
{
    if (values == NULL) return;
    
    char buf[64];
    
    snprintf(buf, sizeof(buf), "CH1: %ld", (long)values->ch1Raw);
    lv_label_set_text(m_adc_ch1_label, buf);
    
    snprintf(buf, sizeof(buf), "CH2: %ld", (long)values->ch2Raw);
    lv_label_set_text(m_adc_ch2_label, buf);
    
    snprintf(buf, sizeof(buf), "Diff: %ld", (long)values->diffRaw);
    lv_label_set_text(m_adc_diff_label, buf);
    
    snprintf(buf, sizeof(buf), "Temp: %.1f C", (double)values->temperatureC);
    lv_label_set_text(m_adc_temp_label, buf);
    
    snprintf(buf, sizeof(buf), "Flow: %.2f LPM", (double)values->flowCal);
    lv_label_set_text(m_adc_flow_label, buf);
}
