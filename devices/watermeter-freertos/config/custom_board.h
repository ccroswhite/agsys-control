/**
 * @file custom_board.h
 * @brief Board definitions for Water Meter (Mag Meter) nRF52840
 */

#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

/* No LEDs on custom board - define dummy values */
#define LEDS_NUMBER    0
#define LEDS_LIST      {}
#define LEDS_ACTIVE_STATE 0
#define BSP_LED_0      0xFFFFFFFF

/* No buttons defined via BSP - we handle them manually */
#define BUTTONS_NUMBER 0
#define BUTTONS_LIST   {}
#define BUTTONS_ACTIVE_STATE 0
#define BSP_BUTTON_0   0xFFFFFFFF

#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

/* RX and TX pins for UART (optional, for debugging) */
#define RX_PIN_NUMBER  8
#define TX_PIN_NUMBER  6
#define CTS_PIN_NUMBER 7
#define RTS_PIN_NUMBER 5
#define HWFC           false

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_BOARD_H */
