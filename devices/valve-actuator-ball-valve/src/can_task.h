/**
 * @file can_task.h
 * @brief CAN bus task for Valve Actuator
 */

#ifndef CAN_TASK_H
#define CAN_TASK_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief CAN task function
 * 
 * Handles CAN bus communication with the valve controller.
 * Receives commands and sends status responses.
 * 
 * @param pvParameters Device address (uint8_t cast to void*)
 */
void can_task(void *pvParameters);

/**
 * @brief Send valve status over CAN
 * 
 * Called by valve_task when status changes.
 */
void can_send_status(void);

/**
 * @brief Send device UID over CAN
 */
void can_send_uid(void);

/**
 * @brief Send discovery response (address + UID)
 * 
 * Called in response to broadcast discovery (CAN ID 0x1F0).
 * Response is staggered by device address to avoid collisions.
 */
void can_send_discovery_response(void);

/**
 * @brief Get CAN task handle for notifications
 */
TaskHandle_t can_get_task_handle(void);

#endif /* CAN_TASK_H */
