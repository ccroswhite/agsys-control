/**
 * @file agsys_config.h
 * @brief Configuration for Valve Actuator (FreeRTOS)
 * 
 * Hardware: Nordic nRF52810 + MCP2515 CAN + Discrete H-Bridge
 */

#ifndef AGSYS_CONFIG_H
#define AGSYS_CONFIG_H

#include "sdk_config.h"  /* Must be first before any nRF SDK headers */
#include "nrf_gpio.h"

/* ==========================================================================
 * DEVICE CONFIGURATION
 * ========================================================================== */

#define AGSYS_DEVICE_TYPE           AGSYS_DEVICE_TYPE_VALVE_ACTUATOR

#define AGSYS_FW_VERSION_MAJOR      1
#define AGSYS_FW_VERSION_MINOR      0
#define AGSYS_FW_VERSION_PATCH      0

/* ==========================================================================
 * DEBUG CONFIGURATION
 * ========================================================================== */

#define AGSYS_DEBUG_ENABLED         1
#define AGSYS_USE_NRF_LOG           0
#define AGSYS_USE_RTT               1

/* ==========================================================================
 * SPI CONFIGURATION
 * ========================================================================== */

#define AGSYS_SPI_INSTANCE          0

#define AGSYS_SPI_SCK_PIN           NRF_GPIO_PIN_MAP(0, 14)
#define AGSYS_SPI_MOSI_PIN          NRF_GPIO_PIN_MAP(0, 12)
#define AGSYS_SPI_MISO_PIN          NRF_GPIO_PIN_MAP(0, 13)

#define AGSYS_SPI_DEFAULT_FREQ      NRF_SPIM_FREQ_4M

/* ==========================================================================
 * CAN BUS CONFIGURATION (MCP2515)
 * ========================================================================== */

#define AGSYS_CAN_CS_PIN            NRF_GPIO_PIN_MAP(0, 11)
#define AGSYS_CAN_INT_PIN           NRF_GPIO_PIN_MAP(0, 8)

/* CAN Message IDs */
#define CAN_ID_VALVE_OPEN           0x100
#define CAN_ID_VALVE_CLOSE          0x101
#define CAN_ID_VALVE_STOP           0x102
#define CAN_ID_VALVE_QUERY          0x103
#define CAN_ID_UID_QUERY            0x104
#define CAN_ID_DISCOVER_ALL         0x105
#define CAN_ID_EMERGENCY_CLOSE      0x1FF
#define CAN_ID_STATUS_BASE          0x200
#define CAN_ID_UID_RESPONSE_BASE    0x280

/* ==========================================================================
 * FRAM CONFIGURATION
 * ========================================================================== */

#define AGSYS_FRAM_CS_PIN           NRF_GPIO_PIN_MAP(0, 7)

/* ==========================================================================
 * H-BRIDGE CONFIGURATION
 * ========================================================================== */

#define AGSYS_HBRIDGE_A_PIN         NRF_GPIO_PIN_MAP(0, 3)
#define AGSYS_HBRIDGE_B_PIN         NRF_GPIO_PIN_MAP(0, 4)
#define AGSYS_HBRIDGE_EN_A_PIN      NRF_GPIO_PIN_MAP(0, 5)
#define AGSYS_HBRIDGE_EN_B_PIN      NRF_GPIO_PIN_MAP(0, 6)

#define AGSYS_CURRENT_SENSE_PIN     NRF_GPIO_PIN_MAP(0, 2)

/* Current thresholds */
#define AGSYS_CURRENT_OVERCURRENT_MA    2000
#define AGSYS_CURRENT_STALL_MA          1500

/* Timing */
#define AGSYS_VALVE_TIMEOUT_MS          30000
#define AGSYS_CURRENT_SAMPLE_INTERVAL_MS 10

/* ==========================================================================
 * LIMIT SWITCHES
 * ========================================================================== */

#define AGSYS_LIMIT_OPEN_PIN        NRF_GPIO_PIN_MAP(0, 9)
#define AGSYS_LIMIT_CLOSED_PIN      NRF_GPIO_PIN_MAP(0, 10)

/* ==========================================================================
 * DIP SWITCHES (Address)
 * ========================================================================== */

#define AGSYS_DIP_1_PIN             NRF_GPIO_PIN_MAP(0, 15)
#define AGSYS_DIP_2_PIN             NRF_GPIO_PIN_MAP(0, 16)
#define AGSYS_DIP_3_PIN             NRF_GPIO_PIN_MAP(0, 17)
#define AGSYS_DIP_4_PIN             NRF_GPIO_PIN_MAP(0, 18)
#define AGSYS_DIP_5_PIN             NRF_GPIO_PIN_MAP(0, 19)
#define AGSYS_DIP_6_PIN             NRF_GPIO_PIN_MAP(0, 20)
#define AGSYS_DIP_10_PIN            NRF_GPIO_PIN_MAP(0, 24)  /* CAN termination */

/* ==========================================================================
 * STATUS LEDS
 * ========================================================================== */

#define AGSYS_LED_3V3_PIN           NRF_GPIO_PIN_MAP(0, 25)
#define AGSYS_LED_24V_PIN           NRF_GPIO_PIN_MAP(0, 26)
#define AGSYS_LED_STATUS_PIN        NRF_GPIO_PIN_MAP(0, 27)
#define AGSYS_LED_VALVE_OPEN_PIN    NRF_GPIO_PIN_MAP(0, 28)

/* ==========================================================================
 * PAIRING BUTTON
 * ========================================================================== */

#define AGSYS_PAIRING_BUTTON_PIN    NRF_GPIO_PIN_MAP(0, 30)
#define AGSYS_PAIRING_HOLD_MS       2000
#define AGSYS_PAIRING_TIMEOUT_MS    300000

/* ==========================================================================
 * BLE CONFIGURATION
 * ========================================================================== */

#define AGSYS_BLE_NAME_PREFIX       "AgSys-VA-"
#define AGSYS_BLE_ADV_INTERVAL_MS   1000

#define AGSYS_BLE_MIN_CONN_INTERVAL MSEC_TO_UNITS(100, UNIT_1_25_MS)
#define AGSYS_BLE_MAX_CONN_INTERVAL MSEC_TO_UNITS(200, UNIT_1_25_MS)
#define AGSYS_BLE_SLAVE_LATENCY     0
#define AGSYS_BLE_CONN_SUP_TIMEOUT  MSEC_TO_UNITS(4000, UNIT_10_MS)

/* ==========================================================================
 * FREERTOS TASK CONFIGURATION
 * ========================================================================== */

#define AGSYS_TASK_STACK_CAN        256
#define AGSYS_TASK_STACK_VALVE      256
#define AGSYS_TASK_STACK_BLE        256
#define AGSYS_TASK_STACK_LED        128

#define AGSYS_TASK_PRIORITY_CAN     4   /* Highest - interrupt driven */
#define AGSYS_TASK_PRIORITY_VALVE   3
#define AGSYS_TASK_PRIORITY_BLE     2
#define AGSYS_TASK_PRIORITY_LED     1   /* Lowest */

#endif /* AGSYS_CONFIG_H */
