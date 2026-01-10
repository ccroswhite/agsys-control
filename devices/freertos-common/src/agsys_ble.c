/**
 * @file agsys_ble.c
 * @brief BLE service implementation with PIN authentication
 */

#include "agsys_ble.h"
#include "agsys_debug.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "nrf_sdh_ble.h"
#include <string.h>

/* Static variables */
static uint8_t m_uuid_base[] = AGSYS_BLE_UUID_BASE;
static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;

typedef struct __attribute__((packed)) {
    uint8_t uid[8];
    uint8_t device_type;
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t fw_patch;
} device_info_t;

static device_info_t m_device_info;

/* Forward declarations */
static uint32_t config_service_init(agsys_ble_ctx_t *ctx);
static uint32_t data_service_init(agsys_ble_ctx_t *ctx);
static void on_write(agsys_ble_ctx_t *ctx, const ble_gatts_evt_write_t *p_write);
static void update_pin_auth_char(agsys_ble_ctx_t *ctx);


/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

agsys_err_t agsys_ble_init(agsys_ble_ctx_t *ctx, const agsys_ble_init_t *init)
{
    uint32_t err_code;
    
    if (ctx == NULL || init == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }

    memset(ctx, 0, sizeof(agsys_ble_ctx_t));
    ctx->conn_handle = BLE_CONN_HANDLE_INVALID;
    ctx->evt_handler = init->evt_handler;
    ctx->auth_ctx = init->auth_ctx;
    ctx->device_type = init->device_type;
    ctx->notifications_enabled = false;
    
    /* Register vendor-specific UUID base */
    ble_uuid128_t base_uuid = {0};
    memcpy(base_uuid.uuid128, m_uuid_base, 16);
    err_code = sd_ble_uuid_vs_add(&base_uuid, &ctx->uuid_type);
    if (err_code != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("BLE: UUID add failed: 0x%04X", err_code);
        return AGSYS_ERR_BLE;
    }
    
    /* Populate device info from FICR */
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    uint32_t id1 = NRF_FICR->DEVICEID[1];
    m_device_info.uid[0] = (id0 >> 0) & 0xFF;
    m_device_info.uid[1] = (id0 >> 8) & 0xFF;
    m_device_info.uid[2] = (id0 >> 16) & 0xFF;
    m_device_info.uid[3] = (id0 >> 24) & 0xFF;
    m_device_info.uid[4] = (id1 >> 0) & 0xFF;
    m_device_info.uid[5] = (id1 >> 8) & 0xFF;
    m_device_info.uid[6] = (id1 >> 16) & 0xFF;
    m_device_info.uid[7] = (id1 >> 24) & 0xFF;
    m_device_info.device_type = init->device_type;
    m_device_info.fw_major = AGSYS_FW_VERSION_MAJOR;
    m_device_info.fw_minor = AGSYS_FW_VERSION_MINOR;
    m_device_info.fw_patch = AGSYS_FW_VERSION_PATCH;
    
    /* Initialize services */
    err_code = config_service_init(ctx);
    if (err_code != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("BLE: Config svc failed: 0x%04X", err_code);
        return AGSYS_ERR_BLE;
    }
    
    err_code = data_service_init(ctx);
    if (err_code != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("BLE: Data svc failed: 0x%04X", err_code);
        return AGSYS_ERR_BLE;
    }
    
    ctx->initialized = true;
    AGSYS_LOG_INFO("BLE: Initialized (type=%d)", init->device_type);
    
    return AGSYS_OK;
}


/* ==========================================================================
 * ADVERTISING
 * ========================================================================== */

agsys_err_t agsys_ble_advertising_start(agsys_ble_ctx_t *ctx)
{
    uint32_t err_code;
    
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }

    static uint8_t adv_buf[31];
    static uint8_t sr_buf[31];
    
    ble_gap_adv_data_t adv_data = {0};
    adv_data.adv_data.p_data = adv_buf;
    adv_data.adv_data.len = 0;
    adv_data.scan_rsp_data.p_data = sr_buf;
    adv_data.scan_rsp_data.len = 0;
    
    /* Flags */
    adv_buf[0] = 2;
    adv_buf[1] = BLE_GAP_AD_TYPE_FLAGS;
    adv_buf[2] = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    adv_data.adv_data.len = 3;
    
    /* 16-bit service UUID */
    adv_buf[3] = 3;
    adv_buf[4] = BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE;
    adv_buf[5] = AGSYS_BLE_UUID_CONFIG_SERVICE & 0xFF;
    adv_buf[6] = (AGSYS_BLE_UUID_CONFIG_SERVICE >> 8) & 0xFF;
    adv_data.adv_data.len = 7;
    
    ble_gap_adv_params_t adv_params = {0};
    adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.p_peer_addr = NULL;
    adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = MSEC_TO_UNITS(100, UNIT_0_625_MS);
    adv_params.duration = 0;
    adv_params.primary_phy = BLE_GAP_PHY_1MBPS;
    
    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &adv_data, &adv_params);
    if (err_code != NRF_SUCCESS) {
        AGSYS_LOG_ERROR("BLE: Adv config failed: 0x%04X", err_code);
        return AGSYS_ERR_BLE;
    }
    
    err_code = sd_ble_gap_adv_start(m_adv_handle, 1);
    if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_INVALID_STATE) {
        AGSYS_LOG_ERROR("BLE: Adv start failed: 0x%04X", err_code);
        return AGSYS_ERR_BLE;
    }

    AGSYS_LOG_INFO("BLE: Advertising started");
    return AGSYS_OK;
}

agsys_err_t agsys_ble_advertising_stop(agsys_ble_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }
    (void)sd_ble_gap_adv_stop(m_adv_handle);
    AGSYS_LOG_INFO("BLE: Advertising stopped");
    return AGSYS_OK;
}

agsys_err_t agsys_ble_disconnect(agsys_ble_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_ERR_NOT_INITIALIZED;
    }
    if (ctx->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return AGSYS_OK;
    }
    uint32_t err_code = sd_ble_gap_disconnect(ctx->conn_handle, 
                                               BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_INVALID_STATE) {
        return AGSYS_ERR_BLE;
    }
    return AGSYS_OK;
}


/* ==========================================================================
 * DATA UPDATES
 * ========================================================================== */

agsys_err_t agsys_ble_update_live_data(agsys_ble_ctx_t *ctx,
                                        const uint8_t *data,
                                        uint16_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (ctx->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return AGSYS_OK;
    }

    ble_gatts_value_t value = {
        .len = len,
        .offset = 0,
        .p_value = (uint8_t *)data
    };
    
    (void)sd_ble_gatts_value_set(ctx->conn_handle,
                                  ctx->char_handles[AGSYS_BLE_CHAR_LIVE_DATA].value_handle,
                                  &value);
    
    if (ctx->notifications_enabled) {
        ble_gatts_hvx_params_t hvx = {0};
        hvx.handle = ctx->char_handles[AGSYS_BLE_CHAR_LIVE_DATA].value_handle;
        hvx.type = BLE_GATT_HVX_NOTIFICATION;
        hvx.p_len = &len;
        hvx.p_data = data;
        (void)sd_ble_gatts_hvx(ctx->conn_handle, &hvx);
    }
    return AGSYS_OK;
}

agsys_err_t agsys_ble_update_status(agsys_ble_ctx_t *ctx,
                                     const uint8_t *data,
                                     uint16_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    ble_gatts_value_t value = {
        .len = len,
        .offset = 0,
        .p_value = (uint8_t *)data
    };
    (void)sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID,
                                  ctx->char_handles[AGSYS_BLE_CHAR_STATUS].value_handle,
                                  &value);
    return AGSYS_OK;
}

agsys_err_t agsys_ble_send_response(agsys_ble_ctx_t *ctx,
                                     const uint8_t *response,
                                     uint16_t len)
{
    if (ctx == NULL || !ctx->initialized || response == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (ctx->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return AGSYS_ERR_BLE;
    }
    ble_gatts_hvx_params_t hvx = {0};
    hvx.handle = ctx->char_handles[AGSYS_BLE_CHAR_RESPONSE].value_handle;
    hvx.type = BLE_GATT_HVX_NOTIFICATION;
    hvx.p_len = &len;
    hvx.p_data = response;
    (void)sd_ble_gatts_hvx(ctx->conn_handle, &hvx);
    return AGSYS_OK;
}

/* ==========================================================================
 * UTILITIES
 * ========================================================================== */

bool agsys_ble_is_connected(const agsys_ble_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    return ctx->conn_handle != BLE_CONN_HANDLE_INVALID;
}

bool agsys_ble_is_authenticated(const agsys_ble_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized || ctx->auth_ctx == NULL) {
        return false;
    }
    return agsys_ble_auth_is_authenticated(ctx->auth_ctx);
}

agsys_err_t agsys_ble_get_rssi(agsys_ble_ctx_t *ctx, int8_t *rssi)
{
    if (ctx == NULL || !ctx->initialized || rssi == NULL) {
        return AGSYS_ERR_INVALID_PARAM;
    }
    if (ctx->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return AGSYS_ERR_BLE;
    }
    (void)sd_ble_gap_rssi_get(ctx->conn_handle, rssi, NULL);
    return AGSYS_OK;
}


/* ==========================================================================
 * EVENT HANDLING
 * ========================================================================== */

void agsys_ble_on_ble_evt(agsys_ble_ctx_t *ctx, const ble_evt_t *ble_evt)
{
    if (ctx == NULL || !ctx->initialized || ble_evt == NULL) {
        return;
    }

    switch (ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            ctx->conn_handle = ble_evt->evt.gap_evt.conn_handle;
            if (ctx->auth_ctx != NULL) {
                agsys_ble_auth_clear(ctx->auth_ctx);
            }
            update_pin_auth_char(ctx);
            if (ctx->evt_handler != NULL) {
                agsys_ble_evt_t evt = {
                    .type = AGSYS_BLE_EVT_CONNECTED,
                    .conn_handle = ctx->conn_handle,
                };
                ctx->evt_handler(&evt);
            }
            AGSYS_LOG_INFO("BLE: Connected");
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            ctx->conn_handle = BLE_CONN_HANDLE_INVALID;
            ctx->notifications_enabled = false;
            if (ctx->auth_ctx != NULL) {
                agsys_ble_auth_clear(ctx->auth_ctx);
            }
            if (ctx->evt_handler != NULL) {
                agsys_ble_evt_t evt = {
                    .type = AGSYS_BLE_EVT_DISCONNECTED,
                    .conn_handle = BLE_CONN_HANDLE_INVALID,
                };
                ctx->evt_handler(&evt);
            }
            AGSYS_LOG_INFO("BLE: Disconnected");
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(ctx, &ble_evt->evt.gatts_evt.params.write);
            break;
        
        default:
            break;
    }
}


/* ==========================================================================
 * SERVICE INITIALIZATION
 * ========================================================================== */

static uint32_t config_service_init(agsys_ble_ctx_t *ctx)
{
    uint32_t err_code;
    ble_uuid_t service_uuid;
    
    service_uuid.type = ctx->uuid_type;
    service_uuid.uuid = AGSYS_BLE_UUID_CONFIG_SERVICE;
    
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                         &service_uuid,
                                         &ctx->config_service_handle);
    if (err_code != NRF_SUCCESS) return err_code;
    
    /* Device Info characteristic (Read) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_t attr = {0};
        ble_uuid_t char_uuid;
        ble_gatts_attr_md_t attr_md = {0};
        
        char_md.char_props.read = 1;
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        char_uuid.type = ctx->uuid_type;
        char_uuid.uuid = AGSYS_BLE_UUID_CHAR_DEVICE_INFO;
        
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.init_len = sizeof(device_info_t);
        attr.max_len = sizeof(device_info_t);
        attr.p_value = (uint8_t *)&m_device_info;
        
        err_code = sd_ble_gatts_characteristic_add(ctx->config_service_handle,
                                                    &char_md, &attr,
                                                    &ctx->char_handles[AGSYS_BLE_CHAR_DEVICE_INFO]);
        if (err_code != NRF_SUCCESS) return err_code;
    }
    
    /* PIN Auth characteristic (Read/Write) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_t attr = {0};
        ble_uuid_t char_uuid;
        ble_gatts_attr_md_t attr_md = {0};
        
        char_md.char_props.read = 1;
        char_md.char_props.write = 1;
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        char_uuid.type = ctx->uuid_type;
        char_uuid.uuid = AGSYS_BLE_UUID_CHAR_PIN_AUTH;
        
        uint8_t init_val = AGSYS_AUTH_NOT_AUTHENTICATED;
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.init_len = 1;
        attr.max_len = AGSYS_PIN_LENGTH;
        attr.p_value = &init_val;
        
        err_code = sd_ble_gatts_characteristic_add(ctx->config_service_handle,
                                                    &char_md, &attr,
                                                    &ctx->char_handles[AGSYS_BLE_CHAR_PIN_AUTH]);
        if (err_code != NRF_SUCCESS) return err_code;
    }
    
    /* PIN Change characteristic (Write) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_t attr = {0};
        ble_uuid_t char_uuid;
        ble_gatts_attr_md_t attr_md = {0};
        
        char_md.char_props.write = 1;
        
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        char_uuid.type = ctx->uuid_type;
        char_uuid.uuid = AGSYS_BLE_UUID_CHAR_PIN_CHANGE;
        
        uint8_t init_val = 0;
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.init_len = 1;
        attr.max_len = AGSYS_PIN_LENGTH * 2;
        attr.p_value = &init_val;
        
        err_code = sd_ble_gatts_characteristic_add(ctx->config_service_handle,
                                                    &char_md, &attr,
                                                    &ctx->char_handles[AGSYS_BLE_CHAR_PIN_CHANGE]);
        if (err_code != NRF_SUCCESS) return err_code;
    }
    
    return NRF_SUCCESS;
}


static uint32_t data_service_init(agsys_ble_ctx_t *ctx)
{
    uint32_t err_code;
    ble_uuid_t service_uuid;
    
    service_uuid.type = ctx->uuid_type;
    service_uuid.uuid = AGSYS_BLE_UUID_DATA_SERVICE;
    
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                         &service_uuid,
                                         &ctx->data_service_handle);
    if (err_code != NRF_SUCCESS) return err_code;
    
    /* Live Data characteristic (Read/Notify) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_t attr = {0};
        ble_uuid_t char_uuid;
        ble_gatts_attr_md_t attr_md = {0};
        ble_gatts_attr_md_t cccd_md = {0};
        
        char_md.char_props.read = 1;
        char_md.char_props.notify = 1;
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        char_md.p_cccd_md = &cccd_md;
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        char_uuid.type = ctx->uuid_type;
        char_uuid.uuid = AGSYS_BLE_UUID_CHAR_LIVE_DATA;
        
        uint8_t init_val[20] = {0};
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.init_len = 1;
        attr.max_len = 20;
        attr.p_value = init_val;
        
        err_code = sd_ble_gatts_characteristic_add(ctx->data_service_handle,
                                                    &char_md, &attr,
                                                    &ctx->char_handles[AGSYS_BLE_CHAR_LIVE_DATA]);
        if (err_code != NRF_SUCCESS) return err_code;
    }
    
    /* Status characteristic (Read/Notify) */
    {
        ble_gatts_char_md_t char_md = {0};
        ble_gatts_attr_t attr = {0};
        ble_uuid_t char_uuid;
        ble_gatts_attr_md_t attr_md = {0};
        ble_gatts_attr_md_t cccd_md = {0};
        
        char_md.char_props.read = 1;
        char_md.char_props.notify = 1;
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        char_md.p_cccd_md = &cccd_md;
        
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        attr_md.vloc = BLE_GATTS_VLOC_STACK;
        
        char_uuid.type = ctx->uuid_type;
        char_uuid.uuid = AGSYS_BLE_UUID_CHAR_STATUS;
        
        uint8_t init_val[8] = {0};
        attr.p_uuid = &char_uuid;
        attr.p_attr_md = &attr_md;
        attr.init_len = 1;
        attr.max_len = 8;
        attr.p_value = init_val;
        
        err_code = sd_ble_gatts_characteristic_add(ctx->data_service_handle,
                                                    &char_md, &attr,
                                                    &ctx->char_handles[AGSYS_BLE_CHAR_STATUS]);
        if (err_code != NRF_SUCCESS) return err_code;
    }
    
    return NRF_SUCCESS;
}


/* ==========================================================================
 * WRITE HANDLERS
 * ========================================================================== */

static void update_pin_auth_char(agsys_ble_ctx_t *ctx)
{
    if (ctx->auth_ctx == NULL) {
        return;
    }
    
    uint8_t status = agsys_ble_auth_get_status(ctx->auth_ctx);
    ble_gatts_value_t value = {
        .len = 1,
        .offset = 0,
        .p_value = &status
    };
    
    (void)sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID,
                                  ctx->char_handles[AGSYS_BLE_CHAR_PIN_AUTH].value_handle,
                                  &value);
}

static void on_write(agsys_ble_ctx_t *ctx, const ble_gatts_evt_write_t *p_write)
{
    /* Handle PIN Auth write */
    if (p_write->handle == ctx->char_handles[AGSYS_BLE_CHAR_PIN_AUTH].value_handle) {
        if (ctx->auth_ctx != NULL && p_write->len == AGSYS_PIN_LENGTH) {
            uint8_t result = agsys_ble_auth_verify_pin(ctx->auth_ctx, 
                                                        p_write->data, 
                                                        p_write->len);
            update_pin_auth_char(ctx);
            
            /* Notify application */
            if (ctx->evt_handler != NULL) {
                agsys_ble_evt_t evt = {
                    .conn_handle = ctx->conn_handle,
                };
                if (result == AGSYS_AUTH_AUTHENTICATED) {
                    evt.type = AGSYS_BLE_EVT_AUTHENTICATED;
                    AGSYS_LOG_INFO("BLE: PIN authenticated");
                } else {
                    evt.type = AGSYS_BLE_EVT_AUTH_FAILED;
                    AGSYS_LOG_WARNING("BLE: PIN auth failed (status=%d)", result);
                }
                ctx->evt_handler(&evt);
            }
        }
        return;
    }
    
    /* Handle PIN Change write */
    if (p_write->handle == ctx->char_handles[AGSYS_BLE_CHAR_PIN_CHANGE].value_handle) {
        if (ctx->auth_ctx != NULL && p_write->len == AGSYS_PIN_LENGTH * 2) {
            uint8_t result = agsys_ble_auth_change_pin(ctx->auth_ctx,
                                                        p_write->data,
                                                        p_write->data + AGSYS_PIN_LENGTH);
            update_pin_auth_char(ctx);
            
            if (result == AGSYS_AUTH_PIN_CHANGED) {
                AGSYS_LOG_INFO("BLE: PIN changed");
            } else {
                AGSYS_LOG_WARNING("BLE: PIN change failed (status=%d)", result);
            }
        }
        return;
    }
    
    /* Handle CCCD writes for notifications */
    if (p_write->handle == ctx->char_handles[AGSYS_BLE_CHAR_LIVE_DATA].cccd_handle ||
        p_write->handle == ctx->char_handles[AGSYS_BLE_CHAR_STATUS].cccd_handle) {
        if (p_write->len == 2) {
            uint16_t cccd_value = (p_write->data[1] << 8) | p_write->data[0];
            ctx->notifications_enabled = (cccd_value & BLE_GATT_HVX_NOTIFICATION) != 0;
            
            if (ctx->evt_handler != NULL) {
                agsys_ble_evt_t evt = {
                    .conn_handle = ctx->conn_handle,
                    .type = ctx->notifications_enabled ? 
                            AGSYS_BLE_EVT_NOTIFICATIONS_ENABLED : 
                            AGSYS_BLE_EVT_NOTIFICATIONS_DISABLED,
                };
                ctx->evt_handler(&evt);
            }
            AGSYS_LOG_DEBUG("BLE: Notifications %s", 
                           ctx->notifications_enabled ? "enabled" : "disabled");
        }
    }
}

