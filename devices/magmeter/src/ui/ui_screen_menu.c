/**
 * @file ui_screen_menu.c
 * @brief Menu navigation screens for water meter
 */

#include "ui_screen_menu.h"
#include "ui/ui_common.h"
#include <string.h>

/* ==========================================================================
 * MENU DEFINITIONS
 * ========================================================================== */

typedef struct {
    const char *text;
    ScreenId_t target_screen;
} menu_item_t;

static const menu_item_t m_main_menu_items[] = {
    {"Display Settings", SCREEN_DISPLAY_SETTINGS},
    {"Flow Settings",    SCREEN_FLOW_SETTINGS},
    {"Alarm Settings",   SCREEN_ALARM_SETTINGS},
    {"LoRa Config",      SCREEN_LORA_CONFIG},
    {"Calibration",      SCREEN_CALIBRATION},
    {"Totalizer",        SCREEN_TOTALIZER},
    {"Diagnostics",      SCREEN_DIAGNOSTICS},
    {"About",            SCREEN_ABOUT},
};
#define MAIN_MENU_COUNT (sizeof(m_main_menu_items) / sizeof(m_main_menu_items[0]))

/* ==========================================================================
 * SCREEN ELEMENTS
 * ========================================================================== */

static lv_obj_t *m_screen = NULL;
static lv_obj_t *m_menu_list = NULL;
static int8_t m_selection = 0;
static bool m_locked = true;

/* ==========================================================================
 * SCREEN CREATION
 * ========================================================================== */

static void refresh_menu_list(void)
{
    if (m_menu_list == NULL) return;
    
    /* Clear existing items */
    lv_obj_clean(m_menu_list);
    
    /* Add menu items */
    for (size_t i = 0; i < MAIN_MENU_COUNT; i++) {
        ui_add_menu_item(m_menu_list, m_main_menu_items[i].text, 
                         (int8_t)i, m_selection);
    }
}

void ui_menu_create(void)
{
    lv_obj_t *content;
    m_screen = ui_create_screen_with_header("Menu", &content);
    
    /* Create menu list */
    m_menu_list = ui_create_menu_list(content);
    
    /* Populate menu */
    refresh_menu_list();
}

void ui_menu_show(void)
{
    if (m_screen != NULL) {
        m_selection = 0;
        refresh_menu_list();
        lv_scr_load(m_screen);
    }
}

void ui_menu_show_submenu(uint8_t submenu_id)
{
    (void)submenu_id;
    /* TODO: Implement submenus */
}

/* ==========================================================================
 * BUTTON HANDLING
 * ========================================================================== */

ScreenId_t ui_menu_handle_button(ButtonEvent_t event)
{
    int8_t old_selection = m_selection;
    
    switch (event) {
        case BTN_UP_SHORT:
        case BTN_UP_LONG:
            if (m_selection > 0) {
                m_selection--;
                ui_menu_update_selection(m_menu_list, old_selection, m_selection);
            }
            break;
            
        case BTN_DOWN_SHORT:
        case BTN_DOWN_LONG:
            if (m_selection < (int8_t)(MAIN_MENU_COUNT - 1)) {
                m_selection++;
                ui_menu_update_selection(m_menu_list, old_selection, m_selection);
            }
            break;
            
        case BTN_SELECT_SHORT:
        case BTN_RIGHT_SHORT:
            /* Return target screen for selected item */
            return m_main_menu_items[m_selection].target_screen;
            
        case BTN_LEFT_SHORT:
        case BTN_LEFT_LONG:
            return SCREEN_MAIN;
            
        default:
            break;
    }
    
    return SCREEN_MENU;  /* Stay on menu */
}

/* ==========================================================================
 * STATE FUNCTIONS
 * ========================================================================== */

int8_t ui_menu_get_selection(void)
{
    return m_selection;
}

bool ui_menu_is_locked(void)
{
    return m_locked;
}

void ui_menu_lock(void)
{
    m_locked = true;
}

void ui_menu_unlock(void)
{
    m_locked = false;
}
