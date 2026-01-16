/**
 * @file lora_task.h
 * @brief LoRa task for Soil Moisture Sensor
 * 
 * Handles RFM95C communication with property controller.
 */

#ifndef LORA_TASK_H
#define LORA_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief LoRa task function
 */
void lora_task(void *pvParameters);

/**
 * @brief Initialize LoRa task resources
 */
bool lora_task_init(void);

/**
 * @brief Send sensor report
 */
bool lora_send_sensor_report(const uint8_t *device_uid,
                              uint32_t probe_freqs[4],
                              uint8_t probe_moisture[4],
                              uint16_t battery_mv,
                              uint8_t flags);

/**
 * @brief Put LoRa module to sleep
 */
void lora_sleep(void);

/**
 * @brief Wake LoRa module
 */
void lora_wake(void);

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
