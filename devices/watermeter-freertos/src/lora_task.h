/**
 * @file lora_task.h
 * @brief LoRa task interface for Water Meter
 */

#ifndef LORA_TASK_H
#define LORA_TASK_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize LoRa task resources (call before scheduler starts)
 */
void lora_task_init(void);

/**
 * @brief Start the LoRa task
 */
void lora_task_start(void);

/**
 * @brief Check if LoRa module is initialized
 * @return true if RFM95 is initialized and ready
 */
bool lora_is_initialized(void);

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
