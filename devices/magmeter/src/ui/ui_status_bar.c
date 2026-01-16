/**
 * @file ui_status_bar.c
 * @brief Status bar and BLE icon management
 */

#include "ui_status_bar.h"
#include "ui/ui_common.h"

/* ==========================================================================
 * STATE
 * ========================================================================== */

static lv_obj_t *m_ble_icon = NULL;
static BleUiState_t m_ble_state = BLE_UI_STATE_IDLE;
static bool m_ble_visible = true;
static uint32_t m_ble_flash_last_ms = 0;
static uint8_t m_ble_flash_count = 0;

/* ==========================================================================
 * BLE ICON
 * ========================================================================== */

void ui_status_set_ble_icon(lv_obj_t *icon)
{
    m_ble_icon = icon;
}

void ui_status_update_ble(BleUiState_t state)
{
    m_ble_state = state;
    m_ble_flash_count = 0;
    
    if (m_ble_icon == NULL) return;
    
    if (state == BLE_UI_STATE_IDLE) {
        lv_obj_add_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
        m_ble_visible = true;
        m_ble_flash_last_ms = lv_tick_get();
    }
}

BleUiState_t ui_status_get_ble(void)
{
    return m_ble_state;
}

void ui_status_tick_ble(void)
{
    if (m_ble_icon == NULL || m_ble_state == BLE_UI_STATE_IDLE) return;
    
    uint32_t now = lv_tick_get();
    uint32_t elapsed = now - m_ble_flash_last_ms;
    uint32_t flash_period_ms;
    
    switch (m_ble_state) {
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
            /* Triple flash then return to idle */
            flash_period_ms = 100;
            if (m_ble_flash_count >= 6) {
                m_ble_state = BLE_UI_STATE_IDLE;
                lv_obj_add_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
                return;
            }
            break;
            
        default:
            return;
    }
    
    if (elapsed >= flash_period_ms) {
        m_ble_flash_last_ms = now;
        m_ble_visible = !m_ble_visible;
        m_ble_flash_count++;
        
        if (m_ble_visible) {
            lv_obj_clear_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_ble_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
