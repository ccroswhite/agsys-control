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

// Check if SELECT button is currently held (for 3-second hold detection)
bool buttons_isSelectHeld(void);

// Get how long SELECT has been held in milliseconds (0 if not held)
uint32_t buttons_getSelectHoldTime(void);

// Check for UP+DOWN combo held for BLE pairing mode
// Returns true once when combo has been held for BLE_PAIRING_COMBO_MS
bool buttons_checkPairingCombo(void);

// Reset pairing combo state (call after entering pairing mode)
void buttons_resetPairingCombo(void);

#endif // BUTTONS_H
