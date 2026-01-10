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

#endif /* LORA_TASK_H */
