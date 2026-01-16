/**
 * @file ui_display_driver.c
 * @brief Display driver abstraction implementation
 * 
 * Provides LVGL integration with hardware-specific display drivers.
 */

#include "ui/ui_display_driver.h"
#include "ui/ui_common.h"
#include <string.h>

/* ==========================================================================
 * STATIC VARIABLES
 * ========================================================================== */

static const ui_display_driver_t *m_driver = NULL;
static lv_display_t *m_display = NULL;

/* Draw buffer - sized for partial updates */
#define DRAW_BUF_LINES  20
static uint8_t m_draw_buf[320 * DRAW_BUF_LINES * sizeof(lv_color_t)];

/* ==========================================================================
 * LVGL CALLBACKS
 * ========================================================================== */

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    if (m_driver != NULL && m_driver->flush != NULL) {
        m_driver->flush(disp, area, color_p);
    } else {
        lv_display_flush_ready(disp);
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

void ui_display_register_driver(const ui_display_driver_t *driver)
{
    m_driver = driver;
}

bool ui_display_init(void)
{
    if (m_driver == NULL) {
        return false;
    }
    
    /* Initialize hardware */
    if (m_driver->init != NULL) {
        if (!m_driver->init()) {
            return false;
        }
    }
    
    /* Initialize LVGL */
    lv_init();
    
    /* Create display */
    m_display = lv_display_create(m_driver->width, m_driver->height);
    if (m_display == NULL) {
        return false;
    }
    
    /* Set up draw buffer */
    lv_display_set_buffers(m_display, m_draw_buf, NULL, 
                           sizeof(m_draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    /* Set flush callback */
    lv_display_set_flush_cb(m_display, flush_cb);
    
    /* Initialize common UI resources */
    ui_common_init();
    
    /* Set backlight to full */
    if (m_driver->set_backlight != NULL) {
        m_driver->set_backlight(100);
    }
    
    return true;
}

lv_display_t *ui_display_get_handle(void)
{
    return m_display;
}

void ui_display_set_backlight(uint8_t percent)
{
    if (m_driver != NULL && m_driver->set_backlight != NULL) {
        m_driver->set_backlight(percent);
    }
}

void ui_display_sleep(void)
{
    if (m_driver != NULL && m_driver->sleep != NULL) {
        m_driver->sleep();
    }
}

void ui_display_wake(void)
{
    if (m_driver != NULL && m_driver->wake != NULL) {
        m_driver->wake();
    }
}

void ui_display_tick(uint32_t tick_period_ms)
{
    lv_tick_inc(tick_period_ms);
}

void ui_display_task_handler(void)
{
    lv_task_handler();
}
