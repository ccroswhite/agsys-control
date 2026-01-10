/**
 * @file lora_task.h
 * @brief LoRa task for Valve Controller
 * 
 * Handles communication with property controller via RFM95C.
 */

#ifndef LORA_TASK_H
#define LORA_TASK_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief LoRa task function
 */
void lora_task(void *pvParameters);

/**
 * @brief Initialize LoRa task resources
 */
bool lora_task_init(void);

/**
 * @brief Send status report to property controller
 */
void lora_send_status_report(void);

/**
 * @brief Request schedule update from property controller
 */
void lora_request_schedule(void);

/**
 * @brief Send valve command acknowledgment (by UID)
 */
void lora_send_valve_ack_by_uid(const uint8_t actuator_uid[8], uint16_t command_id, 
                                 uint8_t result_state, bool success, uint8_t error_code);

/**
 * @brief Send discovery response with actuator UIDs
 */
void lora_send_discovery_response(void);

#endif /* LORA_TASK_H */
