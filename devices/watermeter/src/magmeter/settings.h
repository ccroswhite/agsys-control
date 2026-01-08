/**
 * @file settings.h
 * @brief Settings manager for Mag Meter - FRAM persistence
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "ui_types.h"

// Initialize settings manager (loads from FRAM or uses defaults)
void settings_init(void);

// Get pointer to current settings
UserSettings_t* settings_get(void);

// Save settings to FRAM
void settings_save(void);

// Reset settings to defaults
void settings_reset(void);

// Get default max flow for current tier
float settings_getDefaultMaxFlow(uint8_t tier);

#endif // SETTINGS_H
