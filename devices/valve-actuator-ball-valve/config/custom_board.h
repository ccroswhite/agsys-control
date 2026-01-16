/**
 * @file custom_board.h
 * @brief Custom board definition for Valve Actuator (nRF52810)
 */

#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

/* ==========================================================================
 * LEDS
 * ========================================================================== */

#define LEDS_NUMBER    4

#define LED_1          NRF_GPIO_PIN_MAP(0, 25)  /* 3.3V Power */
#define LED_2          NRF_GPIO_PIN_MAP(0, 26)  /* 24V Present */
#define LED_3          NRF_GPIO_PIN_MAP(0, 27)  /* Status */
#define LED_4          NRF_GPIO_PIN_MAP(0, 28)  /* Valve Open */

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST { LED_1, LED_2, LED_3, LED_4 }

#define LEDS_INV_MASK  0

#define BSP_LED_0      LED_1
#define BSP_LED_1      LED_2
#define BSP_LED_2      LED_3
#define BSP_LED_3      LED_4

/* ==========================================================================
 * BUTTONS
 * ========================================================================== */

#define BUTTONS_NUMBER 1

#define BUTTON_1       NRF_GPIO_PIN_MAP(0, 30)  /* Pairing button */

#define BUTTON_PULL    NRF_GPIO_PIN_NOPULL

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

/* ==========================================================================
 * SPI (for MCP2515 CAN and FRAM)
 * ========================================================================== */

#define SPI_INSTANCE   0

#define SPI_SCK_PIN    NRF_GPIO_PIN_MAP(0, 14)
#define SPI_MOSI_PIN   NRF_GPIO_PIN_MAP(0, 12)
#define SPI_MISO_PIN   NRF_GPIO_PIN_MAP(0, 13)

/* Chip selects */
#define SPI_SS_CAN_PIN   NRF_GPIO_PIN_MAP(0, 11)
#define SPI_SS_FRAM_PIN  NRF_GPIO_PIN_MAP(0, 7)

/* ==========================================================================
 * CAN (MCP2515)
 * ========================================================================== */

#define CAN_INT_PIN    NRF_GPIO_PIN_MAP(0, 8)

/* ==========================================================================
 * H-BRIDGE
 * ========================================================================== */

#define HBRIDGE_A_PIN      NRF_GPIO_PIN_MAP(0, 3)
#define HBRIDGE_B_PIN      NRF_GPIO_PIN_MAP(0, 4)
#define HBRIDGE_EN_A_PIN   NRF_GPIO_PIN_MAP(0, 5)
#define HBRIDGE_EN_B_PIN   NRF_GPIO_PIN_MAP(0, 6)

/* ==========================================================================
 * CURRENT SENSE (ADC)
 * ========================================================================== */

#define CURRENT_SENSE_PIN  NRF_GPIO_PIN_MAP(0, 2)
#define CURRENT_SENSE_AIN  NRF_SAADC_INPUT_AIN0

/* ==========================================================================
 * LIMIT SWITCHES
 * ========================================================================== */

#define LIMIT_OPEN_PIN     NRF_GPIO_PIN_MAP(0, 9)
#define LIMIT_CLOSED_PIN   NRF_GPIO_PIN_MAP(0, 10)

/* ==========================================================================
 * DIP SWITCHES
 * ========================================================================== */

#define DIP_1_PIN      NRF_GPIO_PIN_MAP(0, 15)
#define DIP_2_PIN      NRF_GPIO_PIN_MAP(0, 16)
#define DIP_3_PIN      NRF_GPIO_PIN_MAP(0, 17)
#define DIP_4_PIN      NRF_GPIO_PIN_MAP(0, 18)
#define DIP_5_PIN      NRF_GPIO_PIN_MAP(0, 19)
#define DIP_6_PIN      NRF_GPIO_PIN_MAP(0, 20)
#define DIP_10_PIN     NRF_GPIO_PIN_MAP(0, 24)  /* CAN termination */

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_BOARD_H */
