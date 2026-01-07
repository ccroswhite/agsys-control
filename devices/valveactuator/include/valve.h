/**
 * @file valve.h
 * @brief Valve state machine for valve actuator
 */

#ifndef VALVE_H
#define VALVE_H

#include <Arduino.h>
#include "config.h"

// Valve states
typedef enum {
    VALVE_STATE_IDLE,
    VALVE_STATE_OPENING,
    VALVE_STATE_CLOSING,
    VALVE_STATE_OPEN,
    VALVE_STATE_CLOSED,
    VALVE_STATE_FAULT
} ValveState;

// Initialize valve control
void valve_init(void);

// Valve commands
void valve_open(void);
void valve_close(void);
void valve_stop(void);
void valve_emergency_close(void);

// State machine update (call from loop)
void valve_update(void);

// Get current state
ValveState valve_get_state(void);
uint8_t valve_get_status_flags(void);
uint16_t valve_get_current_ma(void);

// Position sensing
bool valve_is_open(void);
bool valve_is_closed(void);

// Clear fault state
void valve_clear_fault(void);

#endif // VALVE_H
