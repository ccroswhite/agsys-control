/**
 * @file led_task.c
 * @brief LED status task implementation
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf_gpio.h"
#include "SEGGER_RTT.h"

#include "led_task.h"
#include "valve_task.h"
#include "board_config.h"

/* External pairing mode flag and timeout */
extern bool g_pairing_mode;
extern TickType_t g_pairing_start_tick;

/* External function to properly exit pairing mode (stops BLE advertising) */
extern void exit_pairing_mode(void);

/* ==========================================================================
 * LED TASK
 * ========================================================================== */

void led_task(void *pvParameters)
{
    (void)pvParameters;

    SEGGER_RTT_printf(0, "LED task started\n");

    bool led_state = false;
    TickType_t last_toggle = 0;

    for (;;) {
        uint8_t flags = valve_get_status_flags();
        TickType_t now = xTaskGetTickCount();

        /* Valve open LED - solid when open */
        if (flags & STATUS_FLAG_OPEN) {
            nrf_gpio_pin_set(LED_VALVE_OPEN_PIN);
        } else {
            nrf_gpio_pin_clear(LED_VALVE_OPEN_PIN);
        }

        /* Status LED - blink patterns */
        if (g_pairing_mode) {
            /* Check for pairing timeout */
            if ((now - g_pairing_start_tick) >= pdMS_TO_TICKS(BLE_PAIRING_TIMEOUT_MS)) {
                exit_pairing_mode();
            } else {
                /* Very fast blink in pairing mode (100ms) */
                if (now - last_toggle >= pdMS_TO_TICKS(100)) {
                    led_state = !led_state;
                    nrf_gpio_pin_write(LED_STATUS_PIN, led_state);
                    last_toggle = now;
                }
            }
        } else if (flags & STATUS_FLAG_FAULT) {
            /* Fast blink on fault (200ms) */
            if (now - last_toggle >= pdMS_TO_TICKS(200)) {
                led_state = !led_state;
                nrf_gpio_pin_write(LED_STATUS_PIN, led_state);
                last_toggle = now;
            }
        } else if (flags & STATUS_FLAG_MOVING) {
            /* Slow blink while moving (500ms) */
            if (now - last_toggle >= pdMS_TO_TICKS(500)) {
                led_state = !led_state;
                nrf_gpio_pin_write(LED_STATUS_PIN, led_state);
                last_toggle = now;
            }
        } else {
            /* Off when idle */
            nrf_gpio_pin_clear(LED_STATUS_PIN);
            led_state = false;
        }

        /* Sleep - LED updates don't need to be fast */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
