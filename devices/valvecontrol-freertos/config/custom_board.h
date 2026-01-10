/**
 * @file custom_board.h
 * @brief Custom board definition for Valve Controller (nRF52832)
 * 
 * Required by Nordic SDK boards.c - defines LEDs and buttons.
 */

#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

/* LEDs - accent the SDK's LED abstraction */
#define LEDS_NUMBER    3

#define LED_1          18  /* LED_3V3_PIN */
#define LED_2          19  /* LED_24V_PIN */
#define LED_3          20  /* LED_STATUS_PIN */

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST { LED_1, LED_2, LED_3 }

#define LEDS_INV_MASK  0

#define BSP_LED_0      LED_1
#define BSP_LED_1      LED_2
#define BSP_LED_2      LED_3

/* Buttons */
#define BUTTONS_NUMBER 1

#define BUTTON_1       30  /* PAIRING_BUTTON_PIN */

#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

/* No Arduino-style pin mapping */
#define RX_PIN_NUMBER  0xFFFFFFFF
#define TX_PIN_NUMBER  0xFFFFFFFF
#define CTS_PIN_NUMBER 0xFFFFFFFF
#define RTS_PIN_NUMBER 0xFFFFFFFF
#define HWFC           false

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_BOARD_H */
