/**
 * @file buttons.h
 * @brief Button handler for Mag Meter navigation
 * 
 * Handles 3-button navigation with debouncing and long-press detection.
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>
#include "ui_types.h"

// Initialize button GPIOs
void buttons_init(void);

// Poll buttons and return event (call from main loop)
ButtonEvent_t buttons_poll(void);

// Check if any button is currently pressed
bool buttons_anyPressed(void);

#endif // BUTTONS_H
