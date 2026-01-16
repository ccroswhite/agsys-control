/**
 * @file agsys_ble_ota.h
 * @brief BLE OTA Service for Firmware Updates
 * 
 * Provides a custom GATT service for receiving firmware updates over BLE.
 * Works with the agsys_ota module for actual update processing.
 * 
 * Service UUID: 0x1400 (AgSys OTA Service)
 * Characteristics:
 *   - Control (0x1401): Write - Start/Abort/Finish commands
 *   - Data (0x1402): Write No Response - Firmware chunks
 *   - Status (0x1403): Notify - Progress and status updates
 */

#ifndef AGSYS_BLE_OTA_H
#define AGSYS_BLE_OTA_H

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "agsys_ota.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * SERVICE UUIDS
 * ========================================================================== */

#define AGSYS_BLE_OTA_UUID_BASE         {0x41, 0x67, 0x53, 0x79, 0x73, 0x4F, 0x54, 0x41, \
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

#define AGSYS_BLE_OTA_UUID_SERVICE      0x1400
#define AGSYS_BLE_OTA_UUID_CONTROL      0x1401
#define AGSYS_BLE_OTA_UUID_DATA         0x1402
#define AGSYS_BLE_OTA_UUID_STATUS       0x1403

/* ==========================================================================
 * CONTROL COMMANDS
 * ========================================================================== */

typedef enum {
    AGSYS_BLE_OTA_CMD_START     = 0x01,     /**< Start OTA: [size:4][crc:4][major:1][minor:1][patch:1] */
    AGSYS_BLE_OTA_CMD_ABORT     = 0x02,     /**< Abort OTA */
    AGSYS_BLE_OTA_CMD_FINISH    = 0x03,     /**< Finish OTA (verify and apply) */
    AGSYS_BLE_OTA_CMD_REBOOT    = 0x04,     /**< Reboot device */
    AGSYS_BLE_OTA_CMD_STATUS    = 0x05,     /**< Request status */
} agsys_ble_ota_cmd_t;

/* ==========================================================================
 * STATUS NOTIFICATIONS
 * ========================================================================== */

typedef enum {
    AGSYS_BLE_OTA_STATUS_IDLE           = 0x00,
    AGSYS_BLE_OTA_STATUS_READY          = 0x01,     /**< Ready to receive chunks */
    AGSYS_BLE_OTA_STATUS_RECEIVING      = 0x02,     /**< Receiving chunks */
    AGSYS_BLE_OTA_STATUS_VERIFYING      = 0x03,     /**< Verifying firmware */
    AGSYS_BLE_OTA_STATUS_APPLYING       = 0x04,     /**< Applying to flash */
    AGSYS_BLE_OTA_STATUS_COMPLETE       = 0x05,     /**< Update complete, ready to reboot */
    AGSYS_BLE_OTA_STATUS_ERROR          = 0x80,     /**< Error occurred (error code follows) */
} agsys_ble_ota_status_t;

/* ==========================================================================
 * SERVICE CONTEXT
 * ========================================================================== */

typedef struct {
    uint16_t                service_handle;
    ble_gatts_char_handles_t control_handles;
    ble_gatts_char_handles_t data_handles;
    ble_gatts_char_handles_t status_handles;
    uint16_t                conn_handle;
    bool                    notifications_enabled;
    agsys_ota_ctx_t         *ota_ctx;
} agsys_ble_ota_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize BLE OTA service
 * @param p_ota_svc Service context
 * @param ota_ctx OTA context (must be initialized)
 * @return NRF_SUCCESS on success
 */
uint32_t agsys_ble_ota_init(agsys_ble_ota_t *p_ota_svc, agsys_ota_ctx_t *ota_ctx);

/**
 * @brief Handle BLE events
 * @param p_ble_evt BLE event
 * @param p_context Service context
 */
void agsys_ble_ota_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context);

/**
 * @brief Send status notification
 * @param p_ota_svc Service context
 * @param status Status code
 * @param progress Progress percentage (0-100)
 * @param error_code Error code (if status is ERROR)
 * @return NRF_SUCCESS on success
 */
uint32_t agsys_ble_ota_notify_status(agsys_ble_ota_t *p_ota_svc,
                                      agsys_ble_ota_status_t status,
                                      uint8_t progress,
                                      uint8_t error_code);

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_BLE_OTA_H */
