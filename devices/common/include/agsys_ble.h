/**
 * @file agsys_ble.h
 * @brief BLE service definitions for AgSys devices
 * 
 * Provides common BLE services used across all devices:
 * - Device Information Service (DIS)
 * - AgSys Configuration Service
 * - AgSys Data Service (device-specific characteristics)
 * - DFU Service (for firmware updates)
 */

#ifndef AGSYS_BLE_H
#define AGSYS_BLE_H

#include "agsys_common.h"
#include "agsys_ble_auth.h"
#include "ble.h"
#include "ble_srv_common.h"

/* ==========================================================================
 * UUID DEFINITIONS
 * ========================================================================== */

/* AgSys Base UUID: 4147-5359-5300-0000-0000-000000000000 ("AGSYS" in ASCII) */
#define AGSYS_BLE_UUID_BASE {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                              0x00, 0x53, 0x59, 0x53, 0x47, 0x41, 0x00, 0x00}

/* Service UUIDs (16-bit, combined with base) */
#define AGSYS_BLE_UUID_CONFIG_SERVICE       0x0001
#define AGSYS_BLE_UUID_DATA_SERVICE         0x0002

/* Configuration Service Characteristics */
#define AGSYS_BLE_UUID_CHAR_DEVICE_INFO     0x0100  /* R: Device info (UID, type, version) */
#define AGSYS_BLE_UUID_CHAR_DEVICE_NAME     0x0101  /* R/W: Device name */
#define AGSYS_BLE_UUID_CHAR_ZONE_ID         0x0102  /* R/W: Zone assignment */
#define AGSYS_BLE_UUID_CHAR_SECRET_SALT     0x0103  /* W: Provisioning salt */
#define AGSYS_BLE_UUID_CHAR_CONFIG_JSON     0x0104  /* R/W: JSON config blob */
#define AGSYS_BLE_UUID_CHAR_COMMAND         0x0105  /* W: Command input */
#define AGSYS_BLE_UUID_CHAR_RESPONSE        0x0106  /* R/N: Command response */

/* PIN Authentication Characteristics */
#define AGSYS_BLE_UUID_CHAR_PIN_AUTH        0x0110  /* R/W: PIN auth (write PIN, read status) */
#define AGSYS_BLE_UUID_CHAR_PIN_CHANGE      0x0111  /* W: Change PIN (old+new) */

/* Data Service Characteristics (device-specific) */
#define AGSYS_BLE_UUID_CHAR_LIVE_DATA       0x0201  /* R/N: Live sensor data */
#define AGSYS_BLE_UUID_CHAR_STATUS          0x0202  /* R/N: Device status */
#define AGSYS_BLE_UUID_CHAR_DIAGNOSTICS     0x0203  /* R: Diagnostic info */

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

#ifndef AGSYS_BLE_DEVICE_NAME_MAX_LEN
#define AGSYS_BLE_DEVICE_NAME_MAX_LEN       32
#endif

#ifndef AGSYS_BLE_CONFIG_JSON_MAX_LEN
#define AGSYS_BLE_CONFIG_JSON_MAX_LEN       512
#endif

/* ==========================================================================
 * TYPES
 * ========================================================================== */

/**
 * @brief BLE event types for application callback
 */
typedef enum {
    AGSYS_BLE_EVT_CONNECTED,
    AGSYS_BLE_EVT_DISCONNECTED,
    AGSYS_BLE_EVT_AUTHENTICATED,
    AGSYS_BLE_EVT_AUTH_FAILED,
    AGSYS_BLE_EVT_AUTH_TIMEOUT,
    AGSYS_BLE_EVT_CONFIG_CHANGED,
    AGSYS_BLE_EVT_SALT_RECEIVED,
    AGSYS_BLE_EVT_COMMAND_RECEIVED,
    AGSYS_BLE_EVT_NOTIFICATIONS_ENABLED,
    AGSYS_BLE_EVT_NOTIFICATIONS_DISABLED,
} agsys_ble_evt_type_t;

/**
 * @brief BLE event data
 */
typedef struct {
    agsys_ble_evt_type_t    type;
    uint16_t                conn_handle;
    union {
        struct {
            const uint8_t   *data;
            uint16_t        len;
        } config;
        struct {
            uint8_t         salt[16];
        } salt;
        struct {
            uint8_t         cmd_id;
            const uint8_t   *params;
            uint16_t        params_len;
        } command;
    };
} agsys_ble_evt_t;

/**
 * @brief BLE event handler callback
 */
typedef void (*agsys_ble_evt_handler_t)(const agsys_ble_evt_t *evt);

/**
 * @brief BLE initialization parameters
 */
typedef struct {
    const char              *device_name;       /* Advertised device name */
    uint8_t                 device_type;        /* Device type for advertising */
    agsys_ble_evt_handler_t evt_handler;        /* Event callback */
    agsys_ble_auth_ctx_t    *auth_ctx;          /* PIN authentication context */
    bool                    enable_dfu;         /* Enable DFU service */
} agsys_ble_init_t;

/**
 * @brief Characteristic handle indices
 */
typedef enum {
    AGSYS_BLE_CHAR_DEVICE_INFO = 0,
    AGSYS_BLE_CHAR_PIN_AUTH,
    AGSYS_BLE_CHAR_PIN_CHANGE,
    AGSYS_BLE_CHAR_LIVE_DATA,
    AGSYS_BLE_CHAR_STATUS,
    AGSYS_BLE_CHAR_COMMAND,
    AGSYS_BLE_CHAR_RESPONSE,
    AGSYS_BLE_CHAR_COUNT
} agsys_ble_char_idx_t;

/**
 * @brief BLE context
 */
typedef struct {
    uint16_t                conn_handle;
    uint16_t                config_service_handle;
    uint16_t                data_service_handle;
    ble_gatts_char_handles_t char_handles[AGSYS_BLE_CHAR_COUNT];
    agsys_ble_evt_handler_t evt_handler;
    agsys_ble_auth_ctx_t    *auth_ctx;          /* PIN authentication context */
    uint8_t                 uuid_type;          /* Vendor-specific UUID type */
    uint8_t                 device_type;        /* Device type for advertising */
    bool                    notifications_enabled;
    bool                    initialized;
} agsys_ble_ctx_t;

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize BLE stack and services
 * 
 * Initializes SoftDevice, GAP, GATT, and AgSys services.
 * 
 * @param ctx       BLE context
 * @param init      Initialization parameters
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_ble_init(agsys_ble_ctx_t *ctx, const agsys_ble_init_t *init);

/**
 * @brief Start BLE advertising
 * 
 * @param ctx       BLE context
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_ble_advertising_start(agsys_ble_ctx_t *ctx);

/**
 * @brief Stop BLE advertising
 * 
 * @param ctx       BLE context
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_ble_advertising_stop(agsys_ble_ctx_t *ctx);

/**
 * @brief Disconnect current connection
 * 
 * @param ctx       BLE context
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_ble_disconnect(agsys_ble_ctx_t *ctx);

/* ==========================================================================
 * DATA UPDATES
 * ========================================================================== */

/**
 * @brief Update live data characteristic
 * 
 * Sends notification if client has enabled them.
 * 
 * @param ctx       BLE context
 * @param data      Data to send
 * @param len       Data length
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_ble_update_live_data(agsys_ble_ctx_t *ctx,
                                        const uint8_t *data,
                                        uint16_t len);

/**
 * @brief Update status characteristic
 * 
 * @param ctx       BLE context
 * @param data      Status data
 * @param len       Data length
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_ble_update_status(agsys_ble_ctx_t *ctx,
                                     const uint8_t *data,
                                     uint16_t len);

/**
 * @brief Send command response
 * 
 * @param ctx       BLE context
 * @param response  Response data
 * @param len       Response length
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_ble_send_response(agsys_ble_ctx_t *ctx,
                                     const uint8_t *response,
                                     uint16_t len);

/* ==========================================================================
 * UTILITIES
 * ========================================================================== */

/**
 * @brief Check if a client is connected
 * 
 * @param ctx       BLE context
 * @return true if connected
 */
bool agsys_ble_is_connected(const agsys_ble_ctx_t *ctx);

/**
 * @brief Get connection RSSI
 * 
 * @param ctx       BLE context
 * @param rssi      Output: RSSI value
 * @return AGSYS_OK on success
 */
agsys_err_t agsys_ble_get_rssi(agsys_ble_ctx_t *ctx, int8_t *rssi);

/**
 * @brief BLE event handler (call from SoftDevice handler)
 * 
 * @param ctx       BLE context
 * @param ble_evt   SoftDevice BLE event
 */
void agsys_ble_on_ble_evt(agsys_ble_ctx_t *ctx, const ble_evt_t *ble_evt);

/**
 * @brief Check if current session is authenticated
 * 
 * @param ctx       BLE context
 * @return true if authenticated and not timed out
 */
bool agsys_ble_is_authenticated(const agsys_ble_ctx_t *ctx);

#endif /* AGSYS_BLE_H */
