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

#endif /* LORA_TASK_H */
