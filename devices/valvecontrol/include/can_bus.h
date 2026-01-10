/**
 * @file can_bus.h
 * @brief CAN bus communication module for valve controller
 */

#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <Arduino.h>
#include <mcp2515.h>
#include "config.h"

// Actuator UID (8 bytes)
typedef uint8_t ActuatorUID[8];

// Actuator status structure
typedef struct {
    uint8_t address;
    uint8_t status_flags;
    uint16_t current_ma;
    uint32_t last_seen;
    bool online;
    ActuatorUID uid;
    bool uid_known;
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

// UID discovery and lookup
bool canbus_discover_all(void);
bool canbus_query_uid(uint8_t address);
uint8_t canbus_lookup_address_by_uid(const ActuatorUID uid);
ActuatorStatus* canbus_get_actuator_by_uid(const ActuatorUID uid);

// UID-based valve commands (translates UID to address)
bool canbus_open_valve_by_uid(const ActuatorUID uid);
bool canbus_close_valve_by_uid(const ActuatorUID uid);
bool canbus_stop_valve_by_uid(const ActuatorUID uid);

// Process incoming messages (call from loop)
void canbus_process(void);

// Get actuator status
ActuatorStatus* canbus_get_actuator(uint8_t address);
uint8_t canbus_get_online_count(void);
bool canbus_is_actuator_online(uint8_t address);
uint8_t canbus_get_valve_state(uint8_t address);
uint16_t canbus_get_motor_current(uint8_t address);

// Check if interrupt pending
bool canbus_has_message(void);

// UID comparison helper
bool canbus_uid_equals(const ActuatorUID a, const ActuatorUID b);

#endif // CAN_BUS_H
