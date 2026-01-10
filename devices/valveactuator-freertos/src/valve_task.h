/**
 * @file valve_task.h
 * @brief Valve control task for Valve Actuator
 */

#ifndef VALVE_TASK_H
#define VALVE_TASK_H

#include <stdint.h>
#include <stdbool.h>

/* Status flags for CAN reporting */
#define STATUS_FLAG_OPEN        0x01
#define STATUS_FLAG_CLOSED      0x02
#define STATUS_FLAG_MOVING      0x04
#define STATUS_FLAG_FAULT       0x08
#define STATUS_FLAG_OVERCURRENT 0x10
#define STATUS_FLAG_TIMEOUT     0x20

/* Valve states */
typedef enum {
    VALVE_STATE_IDLE,
    VALVE_STATE_OPENING,
    VALVE_STATE_CLOSING,
    VALVE_STATE_OPEN,
    VALVE_STATE_CLOSED,
    VALVE_STATE_FAULT
} valve_state_t;

/**
 * @brief Valve task function
 * 
 * Manages valve state machine, H-bridge control, and current monitoring.
 * 
 * @param pvParameters Unused
 */
void valve_task(void *pvParameters);

/**
 * @brief Request valve open (thread-safe)
 */
void valve_request_open(void);

/**
 * @brief Request valve close (thread-safe)
 */
void valve_request_close(void);

/**
 * @brief Request valve stop (thread-safe)
 */
void valve_request_stop(void);

/**
 * @brief Request emergency close (thread-safe)
 */
void valve_request_emergency_close(void);

/**
 * @brief Get current valve state
 */
valve_state_t valve_get_state(void);

/**
 * @brief Get status flags for CAN reporting
 */
uint8_t valve_get_status_flags(void);

/**
 * @brief Get last measured motor current in mA
 */
uint16_t valve_get_current_ma(void);

/**
 * @brief Check if valve is fully open
 */
bool valve_is_open(void);

/**
 * @brief Check if valve is fully closed
 */
bool valve_is_closed(void);

#endif /* VALVE_TASK_H */
