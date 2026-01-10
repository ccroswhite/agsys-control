/**
 * @file led_task.h
 * @brief LED status task for Valve Actuator
 */

#ifndef LED_TASK_H
#define LED_TASK_H

#include <stdint.h>

/**
 * @brief LED task function
 * 
 * Manages status LED patterns based on valve state.
 * 
 * @param pvParameters Unused
 */
void led_task(void *pvParameters);

#endif /* LED_TASK_H */
