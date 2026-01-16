/**
 * @file ui_display_driver.h
 * @brief Display driver abstraction interface
 * 
 * Allows different display hardware (ST7789, ILI9341, etc.) to be used
 * with the same UI framework. Each device implements this interface.
 */

#ifndef UI_DISPLAY_DRIVER_H
#define UI_DISPLAY_DRIVER_H

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display driver operations
 */
typedef struct {
    /** Display dimensions */
    uint16_t width;
    uint16_t height;
    
    /**
     * @brief Initialize display hardware
     * @return true on success
     */
    bool (*init)(void);
    
    /**
     * @brief Flush pixels to display
     * @param area Area to update
     * @param color_p Pixel data
     * 
     * Must call lv_display_flush_ready() when complete.
     */
    void (*flush)(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p);
    
    /**
     * @brief Set backlight brightness
     * @param percent Brightness 0-100 (0=off, 100=full)
     */
    void (*set_backlight)(uint8_t percent);
    
    /**
     * @brief Enter sleep mode (low power)
     */
    void (*sleep)(void);
    
    /**
     * @brief Wake from sleep mode
     */
    void (*wake)(void);
    
} ui_display_driver_t;

/**
 * @brief Register display driver
 * 
 * Call before ui_display_init(). The driver struct must remain valid
 * for the lifetime of the application.
 * 
 * @param driver Display driver operations
 */
void ui_display_register_driver(const ui_display_driver_t *driver);

/**
 * @brief Initialize LVGL and display
 * 
 * Must call ui_display_register_driver() first.
 * 
 * @return true on success
 */
bool ui_display_init(void);

/**
 * @brief Get LVGL display handle
 * @return Display handle or NULL if not initialized
 */
lv_display_t *ui_display_get_handle(void);

/**
 * @brief Set backlight brightness
 * @param percent Brightness 0-100
 */
void ui_display_set_backlight(uint8_t percent);

/**
 * @brief Enter display sleep mode
 */
void ui_display_sleep(void);

/**
 * @brief Wake display from sleep
 */
void ui_display_wake(void);

/**
 * @brief LVGL tick handler - call from timer or task
 * @param tick_period_ms Milliseconds since last call
 */
void ui_display_tick(uint32_t tick_period_ms);

/**
 * @brief LVGL task handler - call periodically from display task
 */
void ui_display_task_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_DISPLAY_DRIVER_H */
