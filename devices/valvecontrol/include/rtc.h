/**
 * @file rtc.h
 * @brief RV-3028 RTC module for valve controller
 */

#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// Initialize RTC
bool rtc_init(void);

// Get/Set Unix timestamp
uint32_t rtc_get_unix_time(void);
bool rtc_set_unix_time(uint32_t timestamp);

// Get time components
uint8_t rtc_get_hour(void);
uint8_t rtc_get_minute(void);
uint8_t rtc_get_day_of_week(void);  // 0=Sunday, 6=Saturday

// Get minutes from midnight (for schedule matching)
uint16_t rtc_get_minutes_from_midnight(void);

// Battery backup status
bool rtc_is_battery_low(void);

#endif // RTC_H
