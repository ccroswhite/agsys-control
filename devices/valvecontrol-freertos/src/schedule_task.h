/**
 * @file schedule_task.h
 * @brief Schedule task for Valve Controller
 * 
 * Manages time-based irrigation schedules using RTC and FRAM storage.
 */

#ifndef SCHEDULE_TASK_H
#define SCHEDULE_TASK_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of schedule entries */
#define MAX_SCHEDULES   16

/* Schedule entry structure */
typedef struct {
    uint8_t enabled;
    uint8_t actuator_uid[8];    /* Actuator UID (not CAN address) */
    uint8_t days_of_week;       /* Bitmask: bit 0 = Sunday, bit 6 = Saturday */
    uint8_t start_hour;
    uint8_t start_minute;
    uint16_t duration_minutes;
    uint8_t flags;              /* Skip if wet, etc. */
} schedule_entry_t;

/**
 * @brief Schedule task function
 */
void schedule_task(void *pvParameters);

/**
 * @brief Initialize schedule task resources
 */
bool schedule_task_init(void);

/**
 * @brief Load schedules from FRAM
 */
void schedule_load(void);

/**
 * @brief Save schedules to FRAM
 */
void schedule_save(void);

/**
 * @brief Update schedule entry
 */
void schedule_update(uint8_t index, const schedule_entry_t *entry);

/**
 * @brief Get schedule entry
 */
const schedule_entry_t* schedule_get(uint8_t index);

/**
 * @brief Get current RTC time
 */
uint32_t schedule_get_rtc_time(void);

/**
 * @brief Set RTC time
 */
void schedule_set_rtc_time(uint32_t unix_time);

#endif /* SCHEDULE_TASK_H */
