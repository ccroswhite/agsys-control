/**
 * @file ui_status_bar.h
 * @brief Status bar and BLE icon management
 */

#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Update BLE status icon
 * @param state Current BLE UI state
 */
void ui_status_update_ble(BleUiState_t state);

/**
 * @brief Get current BLE UI state
 */
BleUiState_t ui_status_get_ble(void);

/**
 * @brief Tick handler for BLE icon flashing
 * 
 * Call periodically to update flash state.
 */
void ui_status_tick_ble(void);

/**
 * @brief Set BLE icon object reference
 * 
 * Called by screen modules that contain the BLE icon.
 * 
 * @param icon BLE icon object
 */
void ui_status_set_ble_icon(lv_obj_t *icon);

#ifdef __cplusplus
}
#endif

#endif /* UI_STATUS_BAR_H */
