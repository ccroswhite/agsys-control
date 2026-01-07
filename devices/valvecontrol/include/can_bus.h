/**
 * @file can_bus.h
 * @brief CAN bus communication module for valve controller
 */

#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <Arduino.h>
#include <mcp2515.h>
#include "config.h"

// Actuator status structure
typedef struct {
    uint8_t address;
    uint8_t status_flags;
    uint16_t current_ma;
    uint32_t last_seen;
    bool online;
} ActuatorStatus;

// Initialize CAN bus
bool canbus_init(void);

// Send valve commands
bool canbus_open_valve(uint8_t address);
bool canbus_close_valve(uint8_t address);
bool canbus_stop_valve(uint8_t address);
bool canbus_emergency_close_all(void);

// Query actuator status
bool canbus_query_status(uint8_t address);
bool canbus_query_all(void);

// Process incoming messages (call from loop)
void canbus_process(void);

// Get actuator status
ActuatorStatus* canbus_get_actuator(uint8_t address);
uint8_t canbus_get_online_count(void);

// Check if interrupt pending
bool canbus_has_message(void);

#endif // CAN_BUS_H
