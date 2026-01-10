/**
 * @file agsys_common.h
 * @brief Common definitions for AgSys FreeRTOS devices
 */

#ifndef AGSYS_COMMON_H
#define AGSYS_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#include "agsys_config.h"

/* ==========================================================================
 * VERSION
 * ========================================================================== */
#define AGSYS_COMMON_VERSION_MAJOR  1
#define AGSYS_COMMON_VERSION_MINOR  0
#define AGSYS_COMMON_VERSION_PATCH  0

/* ==========================================================================
 * DEVICE TYPES
 * ========================================================================== */
#define AGSYS_DEVICE_TYPE_SOIL_MOISTURE     0x01
#define AGSYS_DEVICE_TYPE_VALVE_CONTROLLER  0x02
#define AGSYS_DEVICE_TYPE_VALVE_ACTUATOR    0x03
#define AGSYS_DEVICE_TYPE_WATER_METER       0x04

/* ==========================================================================
 * ERROR CODES
 * ========================================================================== */
typedef enum {
    AGSYS_OK = 0,
    AGSYS_ERR_INVALID_PARAM,
    AGSYS_ERR_NO_MEMORY,
    AGSYS_ERR_TIMEOUT,
    AGSYS_ERR_BUSY,
    AGSYS_ERR_NOT_INITIALIZED,
    AGSYS_ERR_CRYPTO,
    AGSYS_ERR_SPI,
    AGSYS_ERR_BLE,
    AGSYS_ERR_LORA,
    AGSYS_ERR_FRAM,
    AGSYS_ERR_INTERNAL,
} agsys_err_t;

/* ==========================================================================
 * UTILITY MACROS
 * ========================================================================== */
#define AGSYS_ARRAY_SIZE(arr)   (sizeof(arr) / sizeof((arr)[0]))
#define AGSYS_MIN(a, b)         (((a) < (b)) ? (a) : (b))
#define AGSYS_MAX(a, b)         (((a) > (b)) ? (a) : (b))

#define AGSYS_MS_TO_TICKS(ms)   pdMS_TO_TICKS(ms)

/* ==========================================================================
 * DEVICE UID
 * ========================================================================== */

/**
 * @brief Get the unique device ID (from nRF FICR)
 * @param uid_out Buffer to store 8-byte UID
 */
static inline void agsys_get_device_uid(uint8_t uid_out[8]) {
    uint32_t *ficr = (uint32_t *)uid_out;
    ficr[0] = NRF_FICR->DEVICEID[0];
    ficr[1] = NRF_FICR->DEVICEID[1];
}

/**
 * @brief Get short device ID (first 4 bytes of UID)
 * @return 32-bit short ID
 */
static inline uint32_t agsys_get_short_id(void) {
    return NRF_FICR->DEVICEID[0];
}

#endif /* AGSYS_COMMON_H */
