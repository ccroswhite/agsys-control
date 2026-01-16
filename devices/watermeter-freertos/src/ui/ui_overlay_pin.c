/**
 * @file ui_overlay_pin.c
 * @brief PIN entry overlay for water meter
 */

#include "ui_overlay_pin.h"
#include "ui/ui_common.h"
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

#define PIN_DIGITS      6
#define DIGIT_WIDTH     30
#define DIGIT_HEIGHT    40
#define DIGIT_SPACING   8

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

static lv_obj_t *m_screen = NULL;
static lv_obj_t *m_digit_labels[PIN_DIGITS] = {NULL};
static lv_obj_t *m_status_label = NULL;

static uint8_t m_digits[PIN_DIGITS] = {0};
static int8_t m_cursor = 0;
static uint32_t m_correct_pin = 0;
static pin_result_cb_t m_callback = NULL;
static bool m_active = false;

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static void update_display(void)
{
    for (int i = 0; i < PIN_DIGITS; i++) {
        char buf[4];
        if (i < m_cursor) {
            /* Entered digit - show asterisk */
            snprintf(buf, sizeof(buf), "*");
        } else if (i == m_cursor) {
            /* Current digit - show value */
            snprintf(buf, sizeof(buf), "%d", m_digits[i]);
        } else {
            /* Future digit - show dash */
            snprintf(buf, sizeof(buf), "-");
        }
        lv_label_set_text(m_digit_labels[i], buf);
        
        /* Highlight current digit */
        if (i == m_cursor) {
            lv_obj_set_style_text_color(m_digit_labels[i], UI_COLOR_ACCENT, 0);
        } else {
            lv_obj_set_style_text_color(m_digit_labels[i], UI_COLOR_TEXT, 0);
        }
    }
}

static uint32_t get_entered_pin(void)
{
    uint32_t pin = 0;
    for (int i = 0; i < PIN_DIGITS; i++) {
        pin = pin * 10 + m_digits[i];
    }
    return pin;
}

static void check_pin(void)
{
    uint32_t entered = get_entered_pin();
    bool success = (entered == m_correct_pin);
    
    if (success) {
        lv_label_set_text(m_status_label, "PIN Correct");
        lv_obj_set_style_text_color(m_status_label, UI_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(m_status_label, "Incorrect PIN");
        lv_obj_set_style_text_color(m_status_label, UI_COLOR_ERROR, 0);
        /* Reset for retry */
        memset(m_digits, 0, sizeof(m_digits));
        m_cursor = 0;
        update_display();
    }
    
    if (m_callback) {
        m_callback(success);
    }
    
    if (success) {
        m_active = false;
    }
}

/* ==========================================================================
 * CREATION
 * ========================================================================== */

void ui_pin_create(void)
{
    lv_obj_t *content;
    m_screen = ui_create_screen_with_header("Enter PIN", &content);
    
    /* Instructions */
    lv_obj_t *info = ui_create_label_centered(content,
        "Use UP/DOWN to change digit\nRIGHT to confirm digit",
        UI_FONT_SMALL, UI_COLOR_TEXT_LABEL);
    lv_obj_set_style_pad_top(info, 10, 0);
    
    /* Digit container */
    lv_obj_t *digit_row = lv_obj_create(content);
    lv_obj_set_size(digit_row, LV_PCT(90), DIGIT_HEIGHT + 20);
    lv_obj_set_style_bg_opa(digit_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(digit_row, 0, 0);
    lv_obj_set_style_pad_all(digit_row, 0, 0);
    lv_obj_set_flex_flow(digit_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(digit_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(digit_row, DIGIT_SPACING, 0);
    lv_obj_set_style_pad_top(digit_row, 20, 0);
    lv_obj_clear_flag(digit_row, LV_OBJ_FLAG_SCROLLABLE);
    
    /* Create digit boxes */
    for (int i = 0; i < PIN_DIGITS; i++) {
        lv_obj_t *box = lv_obj_create(digit_row);
        lv_obj_set_size(box, DIGIT_WIDTH, DIGIT_HEIGHT);
        lv_obj_set_style_bg_color(box, UI_COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_border_color(box, UI_COLOR_DIVIDER, 0);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        
        m_digit_labels[i] = lv_label_create(box);
        lv_label_set_text(m_digit_labels[i], "-");
        lv_obj_set_style_text_font(m_digit_labels[i], UI_FONT_XLARGE, 0);
        lv_obj_center(m_digit_labels[i]);
    }
    
    /* Status label */
    m_status_label = ui_create_label_centered(content, "",
        UI_FONT_NORMAL, UI_COLOR_TEXT);
    lv_obj_set_style_pad_top(m_status_label, 20, 0);
}

/* ==========================================================================
 * SHOW/HIDE
 * ========================================================================== */

void ui_pin_show(uint32_t correct_pin, pin_result_cb_t callback)
{
    m_correct_pin = correct_pin;
    m_callback = callback;
    m_active = true;
    
    /* Reset state */
    memset(m_digits, 0, sizeof(m_digits));
    m_cursor = 0;
    lv_label_set_text(m_status_label, "");
    update_display();
    
    lv_scr_load(m_screen);
}

void ui_pin_hide(void)
{
    m_active = false;
}

bool ui_pin_is_active(void)
{
    return m_active;
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

bool ui_pin_handle_button(ButtonEvent_t event)
{
    if (!m_active) return false;
    
    switch (event) {
        case BTN_UP_SHORT:
        case BTN_UP_LONG:
            /* Increment current digit */
            m_digits[m_cursor] = (m_digits[m_cursor] + 1) % 10;
            update_display();
            return true;
            
        case BTN_DOWN_SHORT:
        case BTN_DOWN_LONG:
            /* Decrement current digit */
            m_digits[m_cursor] = (m_digits[m_cursor] + 9) % 10;
            update_display();
            return true;
            
        case BTN_RIGHT_SHORT:
        case BTN_SELECT_SHORT:
            /* Confirm digit, move to next */
            if (m_cursor < PIN_DIGITS - 1) {
                m_cursor++;
                update_display();
            } else {
                /* All digits entered, check PIN */
                check_pin();
            }
            return true;
            
        case BTN_LEFT_SHORT:
            /* Go back to previous digit */
            if (m_cursor > 0) {
                m_cursor--;
                update_display();
            } else {
                /* Cancel PIN entry */
                m_active = false;
                if (m_callback) {
                    m_callback(false);
                }
            }
            return true;
            
        default:
            break;
    }
    
    return false;
}
