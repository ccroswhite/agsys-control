/**
 * @file custom_board.h
 * @brief Custom board definition for Soil Moisture Sensor (nRF52832)
 * 
 * Required by Nordic SDK boards.c - defines LEDs and buttons.
 */

#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

/* LEDs */
#define LEDS_NUMBER    1

#define LED_1          17  /* LED_STATUS_PIN */

#define LEDS_ACTIVE_STATE 0  /* Active LOW */

#define LEDS_LIST { LED_1 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      LED_1

/* Buttons */
#define BUTTONS_NUMBER 1

#define BUTTON_1       7   /* PAIRING_BUTTON_PIN */

#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

/* No UART */
#define RX_PIN_NUMBER  0xFFFFFFFF
#define TX_PIN_NUMBER  0xFFFFFFFF
#define CTS_PIN_NUMBER 0xFFFFFFFF
#define RTS_PIN_NUMBER 0xFFFFFFFF
#define HWFC           false

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_BOARD_H */
