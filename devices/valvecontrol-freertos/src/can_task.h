/**
 * @file can_task.h
 * @brief CAN bus task for Valve Controller
 * 
 * Manages MCP2515 CAN controller to communicate with up to 64 valve actuators.
 */

#ifndef CAN_TASK_H
#define CAN_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Actuator address range */
#define ACTUATOR_ADDR_MIN   1
#define ACTUATOR_ADDR_MAX   64

/* Actuator status structure */
typedef struct {
    bool online;
    bool uid_known;
    uint8_t uid[8];
    uint8_t status_flags;
    uint16_t current_ma;
    TickType_t last_seen;
} actuator_status_t;

/* Command types for CAN task queue */
typedef enum {
    CAN_CMD_OPEN,
    CAN_CMD_CLOSE,
    CAN_CMD_STOP,
    CAN_CMD_EMERGENCY_CLOSE_ALL,
    CAN_CMD_QUERY,
    CAN_CMD_DISCOVER_ALL,
} can_cmd_type_t;

/* Command structure for queue */
typedef struct {
    can_cmd_type_t type;
    uint8_t address;        /* 0xFF for broadcast */
    uint16_t command_id;    /* For tracking responses */
} can_command_t;

/**
 * @brief CAN task function
 */
void can_task(void *pvParameters);

/**
 * @brief Initialize CAN task resources (call before scheduler starts)
 */
bool can_task_init(void);

/**
 * @brief Send command to actuator
 */
bool can_send_command(can_cmd_type_t type, uint8_t address, uint16_t command_id);

/**
 * @brief Open valve at address
 */
bool can_open_valve(uint8_t address);

/**
 * @brief Close valve at address
 */
bool can_close_valve(uint8_t address);

/**
 * @brief Stop valve at address
 */
bool can_stop_valve(uint8_t address);

/**
 * @brief Emergency close all valves
 */
void can_emergency_close_all(void);

/**
 * @brief Query status of all actuators
 */
void can_query_all(void);

/**
 * @brief Discover all actuators on bus
 */
void can_discover_all(void);

/**
 * @brief Check if actuator is online
 */
bool can_is_actuator_online(uint8_t address);

/**
 * @brief Get actuator status
 */
const actuator_status_t* can_get_actuator(uint8_t address);

/**
 * @brief Get valve state for actuator
 */
uint8_t can_get_valve_state(uint8_t address);

/**
 * @brief Get motor current for actuator
 */
uint16_t can_get_motor_current(uint8_t address);

/**
 * @brief Get count of online actuators
 */
uint8_t can_get_online_count(void);

/* ==========================================================================
 * UID-BASED OPERATIONS
 * All external interfaces (LoRa, BLE, schedules) use UID, not CAN address
 * ========================================================================== */

/**
 * @brief Look up CAN address by actuator UID
 * @return CAN address (1-64) or 0 if not found
 */
uint8_t can_lookup_address_by_uid(const uint8_t uid[8]);

/**
 * @brief Get actuator status by UID
 * @return Pointer to actuator status or NULL if not found
 */
const actuator_status_t* can_get_actuator_by_uid(const uint8_t uid[8]);

/**
 * @brief Open valve by actuator UID
 */
bool can_open_valve_by_uid(const uint8_t uid[8]);

/**
 * @brief Close valve by actuator UID
 */
bool can_close_valve_by_uid(const uint8_t uid[8]);

/**
 * @brief Stop valve by actuator UID
 */
bool can_stop_valve_by_uid(const uint8_t uid[8]);

/**
 * @brief Get valve state by actuator UID
 * @return Valve state or 0xFF if not found
 */
uint8_t can_get_valve_state_by_uid(const uint8_t uid[8]);

/**
 * @brief Compare two UIDs
 */
bool can_uid_equals(const uint8_t a[8], const uint8_t b[8]);

#endif /* CAN_TASK_H */
