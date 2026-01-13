/**
 * @file lvgl_port.c
 * @brief LVGL Display Port Implementation for ST7789
 */

#include "lvgl_port.h"
#include "st7789.h"
#include "board_config.h"
#include "lvgl.h"
#include "nrf_gpio.h"
#include "SEGGER_RTT.h"

#include "FreeRTOS.h"
#include "semphr.h"

/* ==========================================================================
 * STATIC VARIABLES
 * ========================================================================== */

/* Display buffer (partial - 40 lines at a time) */
static lv_color_t s_buf1[LVGL_HOR_RES * LVGL_BUF_LINES];

/* LVGL display object */
static lv_display_t *s_display = NULL;

/* Input device for buttons */
static lv_indev_t *s_indev_buttons = NULL;

/* State */
static bool s_initialized = false;
static bool s_sleeping = false;

/* Mutex for thread-safe LVGL access */
static SemaphoreHandle_t s_lvgl_mutex = NULL;

/* ==========================================================================
 * DISPLAY FLUSH CALLBACK
 * ========================================================================== */

/**
 * @brief Flush callback - sends rendered pixels to ST7789
 */
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t x1 = area->x1;
    uint16_t y1 = area->y1;
    uint16_t x2 = area->x2;
    uint16_t y2 = area->y2;
    
    /* Set the address window on ST7789 */
    st7789_set_addr_window(x1, y1, x2, y2);
    
    /* Calculate number of pixels */
    uint32_t size = (x2 - x1 + 1) * (y2 - y1 + 1);
    
    /* Send pixel data
     * LVGL provides RGB565 data in native byte order
     * ST7789 expects big-endian RGB565
     */
    uint16_t *color_p = (uint16_t *)px_map;
    
    /* Write pixels to display */
    st7789_write_pixels(color_p, size);
    
    /* Inform LVGL that flushing is done */
    lv_display_flush_ready(disp);
}

/* ==========================================================================
 * BUTTON INPUT CALLBACKS
 * ========================================================================== */

/* Button state tracking */
static uint32_t s_last_key = 0;
static bool s_key_pressed = false;

/**
 * @brief Read button state for LVGL input device
 */
static void button_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    
    /* Check each button and map to LVGL keys */
    if (nrf_gpio_pin_read(BUTTON_UP_PIN) == 0) {
        data->key = LV_KEY_UP;
        data->state = LV_INDEV_STATE_PRESSED;
        s_last_key = LV_KEY_UP;
        s_key_pressed = true;
    }
    else if (nrf_gpio_pin_read(BUTTON_DOWN_PIN) == 0) {
        data->key = LV_KEY_DOWN;
        data->state = LV_INDEV_STATE_PRESSED;
        s_last_key = LV_KEY_DOWN;
        s_key_pressed = true;
    }
    else if (nrf_gpio_pin_read(BUTTON_LEFT_PIN) == 0) {
        data->key = LV_KEY_LEFT;
        data->state = LV_INDEV_STATE_PRESSED;
        s_last_key = LV_KEY_LEFT;
        s_key_pressed = true;
    }
    else if (nrf_gpio_pin_read(BUTTON_RIGHT_PIN) == 0) {
        data->key = LV_KEY_RIGHT;
        data->state = LV_INDEV_STATE_PRESSED;
        s_last_key = LV_KEY_RIGHT;
        s_key_pressed = true;
    }
    else if (nrf_gpio_pin_read(BUTTON_SELECT_PIN) == 0) {
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
        s_last_key = LV_KEY_ENTER;
        s_key_pressed = true;
    }
    else {
        /* No button pressed */
        data->key = s_last_key;
        data->state = LV_INDEV_STATE_RELEASED;
        s_key_pressed = false;
    }
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool lvgl_port_init(void)
{
    if (s_initialized) {
        return true;
    }
    
    SEGGER_RTT_printf(0, "LVGL: Initializing port...\n");
    
    /* Create mutex for thread safety */
    s_lvgl_mutex = xSemaphoreCreateMutex();
    if (s_lvgl_mutex == NULL) {
        SEGGER_RTT_printf(0, "LVGL: Failed to create mutex\n");
        return false;
    }
    
    /* Initialize ST7789 display hardware */
    if (!st7789_init()) {
        SEGGER_RTT_printf(0, "LVGL: ST7789 init failed\n");
        return false;
    }
    
    /* Set landscape orientation (320x240) */
    st7789_set_rotation(1);
    
    /* Clear screen to black */
    st7789_fill_screen(ST7789_BLACK);
    
    /* Initialize LVGL library */
    lv_init();
    
    /* Create display with partial buffer */
    s_display = lv_display_create(LVGL_HOR_RES, LVGL_VER_RES);
    if (s_display == NULL) {
        SEGGER_RTT_printf(0, "LVGL: Failed to create display\n");
        return false;
    }
    
    /* Set up display buffer (single buffer mode) */
    lv_display_set_buffers(s_display, s_buf1, NULL, 
                           sizeof(s_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    /* Set flush callback */
    lv_display_set_flush_cb(s_display, disp_flush_cb);
    
    s_initialized = true;
    s_sleeping = false;
    
    SEGGER_RTT_printf(0, "LVGL: Initialized (%dx%d, buf=%d lines)\n",
                      LVGL_HOR_RES, LVGL_VER_RES, LVGL_BUF_LINES);
    
    return true;
}

bool lvgl_port_register_buttons(void)
{
    if (!s_initialized) {
        return false;
    }
    
    /* Configure button GPIOs as inputs with pull-ups */
    nrf_gpio_cfg_input(BUTTON_UP_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(BUTTON_DOWN_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(BUTTON_LEFT_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(BUTTON_RIGHT_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(BUTTON_SELECT_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* Create keypad input device */
    s_indev_buttons = lv_indev_create();
    if (s_indev_buttons == NULL) {
        SEGGER_RTT_printf(0, "LVGL: Failed to create input device\n");
        return false;
    }
    
    lv_indev_set_type(s_indev_buttons, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(s_indev_buttons, button_read_cb);
    
    SEGGER_RTT_printf(0, "LVGL: Button input registered\n");
    return true;
}

void lvgl_port_tick(uint32_t tick_ms)
{
    lv_tick_inc(tick_ms);
}

void lvgl_port_task_handler(void)
{
    if (!s_initialized || s_sleeping) {
        return;
    }
    
    /* Take mutex for thread safety */
    if (xSemaphoreTake(s_lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        lv_task_handler();
        xSemaphoreGive(s_lvgl_mutex);
    }
}

void lvgl_port_set_brightness(uint8_t percent)
{
    st7789_set_backlight(percent);
}

void lvgl_port_sleep(void)
{
    if (!s_initialized || s_sleeping) {
        return;
    }
    
    st7789_sleep();
    s_sleeping = true;
    SEGGER_RTT_printf(0, "LVGL: Display sleeping\n");
}

void lvgl_port_wake(void)
{
    if (!s_initialized || !s_sleeping) {
        return;
    }
    
    st7789_wake();
    s_sleeping = false;
    SEGGER_RTT_printf(0, "LVGL: Display awake\n");
}

bool lvgl_port_is_sleeping(void)
{
    return s_sleeping;
}
