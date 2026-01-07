/**
 * @file hbridge.h
 * @brief H-Bridge motor control for valve actuator
 */

#ifndef HBRIDGE_H
#define HBRIDGE_H

#include <Arduino.h>
#include "config.h"

// Motor direction
typedef enum {
    MOTOR_STOP,
    MOTOR_OPEN,
    MOTOR_CLOSE,
    MOTOR_BRAKE
} MotorDirection;

// Initialize H-Bridge pins
void hbridge_init(void);

// Motor control
void hbridge_open(void);
void hbridge_close(void);
void hbridge_stop(void);
void hbridge_brake(void);

// Get current motor direction
MotorDirection hbridge_get_direction(void);

// Current sensing
uint16_t hbridge_read_current_ma(void);
bool hbridge_is_overcurrent(void);
bool hbridge_is_stalled(void);

#endif // HBRIDGE_H
