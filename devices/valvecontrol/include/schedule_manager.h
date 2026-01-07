/**
 * @file schedule_manager.h
 * @brief Schedule management for valve controller
 */

#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "schedule.h"

// Initialize schedule manager
bool schedule_init(void);

// Load schedules from FRAM
bool schedule_load(void);

// Save schedules to FRAM
bool schedule_save(void);

// Add/update/remove schedule entries
bool schedule_add(ScheduleEntry* entry);
bool schedule_update(uint16_t index, ScheduleEntry* entry);
bool schedule_remove(uint16_t index);
bool schedule_clear_all(void);

// Get schedule info
uint16_t schedule_get_count(void);
ScheduleEntry* schedule_get_entry(uint16_t index);
ScheduleHeader* schedule_get_header(void);

// Check schedules against current time
// Returns index of schedule to run, or -1 if none
int16_t schedule_check_pending(void);

// Mark schedule as run (updates last run time)
void schedule_mark_run(uint16_t index);

// Validate schedule data
bool schedule_validate(void);

#endif // SCHEDULE_MANAGER_H
