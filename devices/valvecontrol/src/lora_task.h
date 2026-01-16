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

/**
 * @brief Handle incoming LoRa OTA message (defined in main.c)
 * 
 * Called by lora_task when an OTA message (0x40-0x45) is received.
 * Returns response data to send back to controller.
 * 
 * @param msg_type Message type (0x40-0x45)
 * @param data Message payload
 * @param len Payload length
 * @param response Output buffer for response (at least 4 bytes)
 * @param response_len Output: response length
 * @return true if response should be sent
 */
extern bool ota_handle_lora_message(uint8_t msg_type, const uint8_t *data, size_t len,
                                     uint8_t *response, size_t *response_len);

#endif /* LORA_TASK_H */
