/**
 * @file lvgl_port.h
 * @brief LVGL Display Port for ST7789 on nRF52840
 * 
 * Provides the display driver and input device integration for LVGL.
 * Uses the ST7789 SPI driver for rendering.
 */

#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Display configuration */
#define LVGL_HOR_RES    320
#define LVGL_VER_RES    240

/* Buffer size (partial buffer for memory efficiency) */
#define LVGL_BUF_LINES  40      /* 40 lines = 320*40*2 = 25.6KB */

/**
 * @brief Initialize LVGL and display port
 * 
 * Initializes LVGL library, creates display driver, and sets up
 * the ST7789 as the rendering target.
 * 
 * @return true on success
 */
bool lvgl_port_init(void);

/**
 * @brief LVGL tick handler - call from timer or task
 * 
 * Must be called periodically (every 1-10ms) to drive LVGL timing.
 * Typically called from a FreeRTOS timer or the display task.
 * 
 * @param tick_ms Milliseconds since last call
 */
void lvgl_port_tick(uint32_t tick_ms);

/**
 * @brief LVGL task handler - call from display task
 * 
 * Processes LVGL events and renders pending updates.
 * Should be called frequently (every 5-33ms for 30-200 FPS).
 */
void lvgl_port_task_handler(void);

/**
 * @brief Register button input device
 * 
 * Registers the 5-button navigation as an LVGL input device.
 * Buttons: UP, DOWN, LEFT, RIGHT, SELECT
 * 
 * @return true on success
 */
bool lvgl_port_register_buttons(void);

/**
 * @brief Set display brightness
 * @param percent 0-100 (0 = off, 100 = full brightness)
 */
void lvgl_port_set_brightness(uint8_t percent);

/**
 * @brief Enter display sleep mode
 */
void lvgl_port_sleep(void);

/**
 * @brief Wake display from sleep
 */
void lvgl_port_wake(void);

/**
 * @brief Check if display is in sleep mode
 * @return true if sleeping
 */
bool lvgl_port_is_sleeping(void);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_PORT_H */
