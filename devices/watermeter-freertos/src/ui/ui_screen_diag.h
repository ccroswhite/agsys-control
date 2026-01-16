/**
 * @file ui_screen_diag.h
 * @brief Diagnostics screens for water meter
 */

#ifndef UI_SCREEN_DIAG_H
#define UI_SCREEN_DIAG_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create diagnostics screen objects
 */
void ui_diag_create(void);

/**
 * @brief Show diagnostics menu
 */
void ui_diag_show_menu(void);

/**
 * @brief Show LoRa diagnostics screen
 */
void ui_diag_show_lora(void);

/**
 * @brief Show ADC diagnostics screen
 */
void ui_diag_show_adc(void);

/**
 * @brief Handle button input on diagnostics screens
 * @param event Button event
 * @return Next screen to show
 */
ScreenId_t ui_diag_handle_button(ButtonEvent_t event);

/**
 * @brief Update LoRa diagnostics display
 * @param stats LoRa statistics
 */
void ui_diag_update_lora(const LoRaStats_t *stats);

/**
 * @brief Update ADC diagnostics display
 * @param values ADC values
 */
void ui_diag_update_adc(const ADCValues_t *values);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_DIAG_H */
