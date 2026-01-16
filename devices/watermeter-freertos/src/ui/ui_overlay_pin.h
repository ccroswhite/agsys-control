/**
 * @file ui_overlay_pin.h
 * @brief PIN entry overlay for water meter
 */

#ifndef UI_OVERLAY_PIN_H
#define UI_OVERLAY_PIN_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PIN entry result callback
 * @param success true if PIN was correct
 */
typedef void (*pin_result_cb_t)(bool success);

/**
 * @brief Create PIN overlay objects
 */
void ui_pin_create(void);

/**
 * @brief Show PIN entry overlay
 * @param correct_pin The correct PIN to match
 * @param callback Called when PIN entry completes
 */
void ui_pin_show(uint32_t correct_pin, pin_result_cb_t callback);

/**
 * @brief Hide PIN overlay
 */
void ui_pin_hide(void);

/**
 * @brief Handle button input on PIN overlay
 * @param event Button event
 * @return true if event was consumed
 */
bool ui_pin_handle_button(ButtonEvent_t event);

/**
 * @brief Check if PIN overlay is active
 */
bool ui_pin_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_OVERLAY_PIN_H */
