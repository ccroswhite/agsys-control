/**
 * @file agsys_ble_ota.c
 * @brief BLE OTA Service for Firmware Updates
 */

#include "agsys_ble_ota.h"
#include "agsys_ota.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* ==========================================================================
 * STATIC VARIABLES
 * ========================================================================== */

static agsys_ble_ota_t *mp_ota_svc = NULL;

/* ==========================================================================
 * PROGRESS CALLBACK
 * ========================================================================== */

static void ota_progress_callback(agsys_ota_status_t status, uint8_t progress, void *user_data)
{
    agsys_ble_ota_t *p_svc = (agsys_ble_ota_t *)user_data;
    if (p_svc == NULL) return;
    
    agsys_ble_ota_status_t ble_status;
    
    switch (status) {
        case AGSYS_OTA_STATUS_IDLE:
            ble_status = AGSYS_BLE_OTA_STATUS_IDLE;
            break;
        case AGSYS_OTA_STATUS_BACKUP_IN_PROGRESS:
        case AGSYS_OTA_STATUS_RECEIVING:
            ble_status = AGSYS_BLE_OTA_STATUS_RECEIVING;
            break;
        case AGSYS_OTA_STATUS_VERIFYING:
            ble_status = AGSYS_BLE_OTA_STATUS_VERIFYING;
            break;
        case AGSYS_OTA_STATUS_APPLYING:
            ble_status = AGSYS_BLE_OTA_STATUS_APPLYING;
            break;
        case AGSYS_OTA_STATUS_PENDING_REBOOT:
            ble_status = AGSYS_BLE_OTA_STATUS_COMPLETE;
            break;
        case AGSYS_OTA_STATUS_ERROR:
            ble_status = AGSYS_BLE_OTA_STATUS_ERROR;
            break;
        default:
            ble_status = AGSYS_BLE_OTA_STATUS_IDLE;
            break;
    }
    
    agsys_ble_ota_notify_status(p_svc, ble_status, progress, 0);
}

static void ota_complete_callback(bool success, agsys_ota_error_t error, void *user_data)
{
    agsys_ble_ota_t *p_svc = (agsys_ble_ota_t *)user_data;
    if (p_svc == NULL) return;
    
    if (success) {
        agsys_ble_ota_notify_status(p_svc, AGSYS_BLE_OTA_STATUS_COMPLETE, 100, 0);
    } else {
        agsys_ble_ota_notify_status(p_svc, AGSYS_BLE_OTA_STATUS_ERROR, 0, (uint8_t)error);
    }
}

/* ==========================================================================
 * CHARACTERISTIC HANDLERS
 * ========================================================================== */

static void handle_control_write(agsys_ble_ota_t *p_svc, const uint8_t *data, uint16_t len)
{
    if (len < 1) return;
    
    uint8_t cmd = data[0];
    
    switch (cmd) {
        case AGSYS_BLE_OTA_CMD_START:
            if (len >= 12) {
                uint32_t fw_size = (data[1]) | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                uint32_t fw_crc = (data[5]) | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
                uint8_t major = data[9];
                uint8_t minor = data[10];
                uint8_t patch = data[11];
                
                SEGGER_RTT_printf(0, "BLE OTA: Start - size=%lu, crc=0x%08lX, v%d.%d.%d\n",
                                  fw_size, fw_crc, major, minor, patch);
                
                /* Suspend other tasks */
                agsys_ota_suspend_tasks();
                
                agsys_ota_error_t err = agsys_ota_start(p_svc->ota_ctx, fw_size, fw_crc,
                                                         major, minor, patch);
                if (err == AGSYS_OTA_ERR_NONE) {
                    agsys_ble_ota_notify_status(p_svc, AGSYS_BLE_OTA_STATUS_READY, 0, 0);
                } else {
                    agsys_ble_ota_notify_status(p_svc, AGSYS_BLE_OTA_STATUS_ERROR, 0, (uint8_t)err);
                    agsys_ota_resume_tasks();
                }
            }
            break;
            
        case AGSYS_BLE_OTA_CMD_ABORT:
            SEGGER_RTT_printf(0, "BLE OTA: Abort\n");
            agsys_ota_abort(p_svc->ota_ctx);
            agsys_ota_resume_tasks();
            agsys_ble_ota_notify_status(p_svc, AGSYS_BLE_OTA_STATUS_IDLE, 0, 0);
            break;
            
        case AGSYS_BLE_OTA_CMD_FINISH:
            SEGGER_RTT_printf(0, "BLE OTA: Finish\n");
            {
                agsys_ota_error_t err = agsys_ota_finish(p_svc->ota_ctx);
                if (err != AGSYS_OTA_ERR_NONE) {
                    agsys_ble_ota_notify_status(p_svc, AGSYS_BLE_OTA_STATUS_ERROR, 0, (uint8_t)err);
                    agsys_ota_resume_tasks();
                }
                /* Success notification sent by complete callback */
            }
            break;
            
        case AGSYS_BLE_OTA_CMD_REBOOT:
            SEGGER_RTT_printf(0, "BLE OTA: Reboot\n");
            agsys_ota_reboot();
            break;
            
        case AGSYS_BLE_OTA_CMD_STATUS:
            {
                agsys_ota_status_t status = agsys_ota_get_status(p_svc->ota_ctx);
                uint8_t progress = agsys_ota_get_progress(p_svc->ota_ctx);
                agsys_ble_ota_status_t ble_status = (status == AGSYS_OTA_STATUS_IDLE) ?
                    AGSYS_BLE_OTA_STATUS_IDLE : AGSYS_BLE_OTA_STATUS_RECEIVING;
                agsys_ble_ota_notify_status(p_svc, ble_status, progress, 0);
            }
            break;
            
        default:
            SEGGER_RTT_printf(0, "BLE OTA: Unknown command 0x%02X\n", cmd);
            break;
    }
}

static void handle_data_write(agsys_ble_ota_t *p_svc, const uint8_t *data, uint16_t len)
{
    if (len < 5) return;  /* Minimum: 4-byte offset + 1 byte data */
    
    /* First 4 bytes are offset */
    uint32_t offset = (data[0]) | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    
    agsys_ota_error_t err = agsys_ota_write_chunk(p_svc->ota_ctx, offset, data + 4, len - 4);
    
    if (err != AGSYS_OTA_ERR_NONE) {
        SEGGER_RTT_printf(0, "BLE OTA: Chunk write error %d at offset %lu\n", err, offset);
        agsys_ble_ota_notify_status(p_svc, AGSYS_BLE_OTA_STATUS_ERROR, 0, (uint8_t)err);
    }
}

/* ==========================================================================
 * BLE EVENT HANDLER
 * ========================================================================== */

void agsys_ble_ota_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
    agsys_ble_ota_t *p_svc = (agsys_ble_ota_t *)p_context;
    if (p_svc == NULL) return;
    
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            p_svc->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            p_svc->conn_handle = BLE_CONN_HANDLE_INVALID;
            p_svc->notifications_enabled = false;
            
            /* Handle disconnect based on OTA state */
            {
                agsys_ota_status_t status = agsys_ota_get_status(p_svc->ota_ctx);
                
                switch (status) {
                    case AGSYS_OTA_STATUS_IDLE:
                        /* Nothing to do */
                        break;
                        
                    case AGSYS_OTA_STATUS_BACKUP_IN_PROGRESS:
                    case AGSYS_OTA_STATUS_RECEIVING:
                    case AGSYS_OTA_STATUS_VERIFYING:
                        /* Abort: firmware transfer incomplete, delete partial data */
                        SEGGER_RTT_printf(0, "BLE OTA: Disconnected during transfer (state=%d), aborting\n", status);
                        agsys_ota_abort(p_svc->ota_ctx);
                        agsys_ota_resume_tasks();
                        break;
                        
                    case AGSYS_OTA_STATUS_APPLYING:
                        /* Continue: firmware verified, flash copy in progress - BLE not needed */
                        SEGGER_RTT_printf(0, "BLE OTA: Disconnected during apply, continuing update\n");
                        break;
                        
                    case AGSYS_OTA_STATUS_PENDING_REBOOT:
                        /* Continue: firmware applied, auto-reboot after timeout */
                        SEGGER_RTT_printf(0, "BLE OTA: Disconnected after complete, auto-reboot in 60s\n");
                        /* TODO: Start auto-reboot timer if not already running */
                        break;
                        
                    case AGSYS_OTA_STATUS_PENDING_CONFIRM:
                    case AGSYS_OTA_STATUS_ERROR:
                    default:
                        /* Resume normal operation */
                        agsys_ota_resume_tasks();
                        break;
                }
            }
            break;
            
        case BLE_GATTS_EVT_WRITE:
            {
                ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;
                
                /* Control characteristic */
                if (p_evt_write->handle == p_svc->control_handles.value_handle) {
                    handle_control_write(p_svc, p_evt_write->data, p_evt_write->len);
                }
                /* Data characteristic */
                else if (p_evt_write->handle == p_svc->data_handles.value_handle) {
                    handle_data_write(p_svc, p_evt_write->data, p_evt_write->len);
                }
                /* Status CCCD */
                else if (p_evt_write->handle == p_svc->status_handles.cccd_handle) {
                    p_svc->notifications_enabled = ble_srv_is_notification_enabled(p_evt_write->data);
                    SEGGER_RTT_printf(0, "BLE OTA: Notifications %s\n",
                                      p_svc->notifications_enabled ? "enabled" : "disabled");
                }
            }
            break;
            
        default:
            break;
    }
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

static uint32_t add_control_char(agsys_ble_ota_t *p_svc)
{
    ble_gatts_char_md_t char_md = {0};
    ble_gatts_attr_t attr_char_value = {0};
    ble_uuid_t ble_uuid;
    ble_gatts_attr_md_t attr_md = {0};
    
    char_md.char_props.write = 1;
    char_md.p_char_user_desc = NULL;
    char_md.p_char_pf = NULL;
    char_md.p_user_desc_md = NULL;
    char_md.p_cccd_md = NULL;
    char_md.p_sccd_md = NULL;
    
    ble_uuid.type = BLE_UUID_TYPE_BLE;
    ble_uuid.uuid = AGSYS_BLE_OTA_UUID_CONTROL;
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 1;
    
    attr_char_value.p_uuid = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = 0;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len = 20;
    attr_char_value.p_value = NULL;
    
    return sd_ble_gatts_characteristic_add(p_svc->service_handle,
                                            &char_md,
                                            &attr_char_value,
                                            &p_svc->control_handles);
}

static uint32_t add_data_char(agsys_ble_ota_t *p_svc)
{
    ble_gatts_char_md_t char_md = {0};
    ble_gatts_attr_t attr_char_value = {0};
    ble_uuid_t ble_uuid;
    ble_gatts_attr_md_t attr_md = {0};
    
    char_md.char_props.write_wo_resp = 1;  /* Write without response for speed */
    char_md.p_char_user_desc = NULL;
    char_md.p_char_pf = NULL;
    char_md.p_user_desc_md = NULL;
    char_md.p_cccd_md = NULL;
    char_md.p_sccd_md = NULL;
    
    ble_uuid.type = BLE_UUID_TYPE_BLE;
    ble_uuid.uuid = AGSYS_BLE_OTA_UUID_DATA;
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 1;
    
    attr_char_value.p_uuid = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = 0;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len = 244;  /* Max MTU - 3 for ATT header */
    attr_char_value.p_value = NULL;
    
    return sd_ble_gatts_characteristic_add(p_svc->service_handle,
                                            &char_md,
                                            &attr_char_value,
                                            &p_svc->data_handles);
}

static uint32_t add_status_char(agsys_ble_ota_t *p_svc)
{
    ble_gatts_char_md_t char_md = {0};
    ble_gatts_attr_t attr_char_value = {0};
    ble_uuid_t ble_uuid;
    ble_gatts_attr_md_t attr_md = {0};
    ble_gatts_attr_md_t cccd_md = {0};
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    
    char_md.char_props.read = 1;
    char_md.char_props.notify = 1;
    char_md.p_char_user_desc = NULL;
    char_md.p_char_pf = NULL;
    char_md.p_user_desc_md = NULL;
    char_md.p_cccd_md = &cccd_md;
    char_md.p_sccd_md = NULL;
    
    ble_uuid.type = BLE_UUID_TYPE_BLE;
    ble_uuid.uuid = AGSYS_BLE_OTA_UUID_STATUS;
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 1;
    
    uint8_t initial_status[3] = {AGSYS_BLE_OTA_STATUS_IDLE, 0, 0};
    
    attr_char_value.p_uuid = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = sizeof(initial_status);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len = 3;
    attr_char_value.p_value = initial_status;
    
    return sd_ble_gatts_characteristic_add(p_svc->service_handle,
                                            &char_md,
                                            &attr_char_value,
                                            &p_svc->status_handles);
}

uint32_t agsys_ble_ota_init(agsys_ble_ota_t *p_ota_svc, agsys_ota_ctx_t *ota_ctx)
{
    if (p_ota_svc == NULL || ota_ctx == NULL) {
        return NRF_ERROR_NULL;
    }
    
    uint32_t err_code;
    ble_uuid_t ble_uuid;
    
    /* Initialize context */
    p_ota_svc->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_ota_svc->notifications_enabled = false;
    p_ota_svc->ota_ctx = ota_ctx;
    mp_ota_svc = p_ota_svc;
    
    /* Set up OTA callbacks */
    agsys_ota_set_progress_callback(ota_ctx, ota_progress_callback, p_ota_svc);
    agsys_ota_set_complete_callback(ota_ctx, ota_complete_callback, p_ota_svc);
    
    /* Add service */
    ble_uuid.type = BLE_UUID_TYPE_BLE;
    ble_uuid.uuid = AGSYS_BLE_OTA_UUID_SERVICE;
    
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                         &ble_uuid,
                                         &p_ota_svc->service_handle);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    /* Add characteristics */
    err_code = add_control_char(p_ota_svc);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    err_code = add_data_char(p_ota_svc);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    err_code = add_status_char(p_ota_svc);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    SEGGER_RTT_printf(0, "BLE OTA: Service initialized\n");
    return NRF_SUCCESS;
}

/* ==========================================================================
 * STATUS NOTIFICATION
 * ========================================================================== */

uint32_t agsys_ble_ota_notify_status(agsys_ble_ota_t *p_ota_svc,
                                      agsys_ble_ota_status_t status,
                                      uint8_t progress,
                                      uint8_t error_code)
{
    if (p_ota_svc == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (p_ota_svc->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }
    
    if (!p_ota_svc->notifications_enabled) {
        return NRF_SUCCESS;  /* Silently succeed if notifications not enabled */
    }
    
    uint8_t data[3] = {status, progress, error_code};
    uint16_t len = sizeof(data);
    
    ble_gatts_hvx_params_t hvx_params = {0};
    hvx_params.handle = p_ota_svc->status_handles.value_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len = &len;
    hvx_params.p_data = data;
    
    return sd_ble_gatts_hvx(p_ota_svc->conn_handle, &hvx_params);
}
