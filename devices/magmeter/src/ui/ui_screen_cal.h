/**
 * @file ui_screen_cal.h
 * @brief Calibration screens for water meter
 */

#ifndef UI_SCREEN_CAL_H
#define UI_SCREEN_CAL_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create calibration screen objects
 */
void ui_cal_create(void);

/**
 * @brief Show calibration menu
 */
void ui_cal_show_menu(void);

/**
 * @brief Show zero calibration screen
 */
void ui_cal_show_zero(void);

/**
 * @brief Show span calibration screen
 */
void ui_cal_show_span(void);

/**
 * @brief Show pipe size selection screen
 */
void ui_cal_show_pipe_size(void);

/**
 * @brief Show duty cycle adjustment screen
 */
void ui_cal_show_duty_cycle(void);

/**
 * @brief Show calibration view (read-only)
 */
void ui_cal_show_view(void);

/**
 * @brief Handle button input on calibration screens
 * @param event Button event
 * @return Next screen to show
 */
ScreenId_t ui_cal_handle_button(ButtonEvent_t event);

/**
 * @brief Update calibration display with current values
 * @param cal Calibration data
 */
void ui_cal_update(const CalibrationData_t *cal);

/**
 * @brief Set callback for calibration actions
 * @param zero_cb Called when zero cal requested
 * @param span_cb Called when span cal requested
 */
typedef void (*cal_action_cb_t)(void);
void ui_cal_set_callbacks(cal_action_cb_t zero_cb, cal_action_cb_t span_cb);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_CAL_H */
