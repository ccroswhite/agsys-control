/**
 * @file ui_overlay_alarm.h
 * @brief Alarm overlay for water meter
 */

#ifndef UI_OVERLAY_ALARM_H
#define UI_OVERLAY_ALARM_H

#include "ui/ui_common.h"
#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create alarm overlay objects
 * @param parent Parent screen to attach overlay to
 */
void ui_alarm_create(lv_obj_t *parent);

/**
 * @brief Show alarm overlay
 * @param type Alarm type
 * @param duration_sec Duration of alarm condition
 * @param flow_lpm Current flow rate
 * @param volume_liters Volume during alarm
 */
void ui_alarm_show(AlarmType_t type, uint32_t duration_sec,
                   float flow_lpm, float volume_liters);

/**
 * @brief Acknowledge alarm (stop flashing)
 */
void ui_alarm_acknowledge(void);

/**
 * @brief Dismiss alarm overlay
 */
void ui_alarm_dismiss(void);

/**
 * @brief Check if alarm is active
 */
bool ui_alarm_is_active(void);

/**
 * @brief Get alarm type name
 */
const char *ui_alarm_get_name(AlarmType_t type);

#ifdef __cplusplus
}
#endif

#endif /* UI_OVERLAY_ALARM_H */
