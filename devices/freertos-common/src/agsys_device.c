/**
 * @file agsys_device.c
 * @brief Common device initialization for AgSys FreeRTOS devices
 */

#include "agsys_device.h"
#include "agsys_common.h"
#include "nrf.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* ==========================================================================
 * UID RETRIEVAL
 * ========================================================================== */

void agsys_device_get_uid(uint8_t uid[8])
{
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    uint32_t id1 = NRF_FICR->DEVICEID[1];
    
    uid[0] = (id0 >> 0) & 0xFF;
    uid[1] = (id0 >> 8) & 0xFF;
    uid[2] = (id0 >> 16) & 0xFF;
    uid[3] = (id0 >> 24) & 0xFF;
    uid[4] = (id1 >> 0) & 0xFF;
    uid[5] = (id1 >> 8) & 0xFF;
    uid[6] = (id1 >> 16) & 0xFF;
    uid[7] = (id1 >> 24) & 0xFF;
}

/* ==========================================================================
 * DEVICE INITIALIZATION
 * ========================================================================== */

bool agsys_device_init(agsys_device_ctx_t *ctx, const agsys_device_init_t *init)
{
    if (ctx == NULL || init == NULL) {
        return false;
    }
    
    /* Clear context */
    memset(ctx, 0, sizeof(agsys_device_ctx_t));
    ctx->device_type = init->device_type;
    
    /* Get device UID */
    agsys_device_get_uid(ctx->device_uid);
    SEGGER_RTT_printf(0, "Device UID: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                      ctx->device_uid[0], ctx->device_uid[1], 
                      ctx->device_uid[2], ctx->device_uid[3],
                      ctx->device_uid[4], ctx->device_uid[5], 
                      ctx->device_uid[6], ctx->device_uid[7]);
    
    /* Initialize FRAM */
    if (agsys_fram_init(&ctx->fram_ctx, init->fram_cs_pin) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "WARNING: FRAM init failed\n");
        /* Continue anyway - PIN auth will use defaults */
    }
    
    /* Initialize BLE PIN authentication */
    if (!agsys_ble_auth_init(&ctx->auth_ctx, &ctx->fram_ctx, AGSYS_FRAM_ADDR_BLE_PIN)) {
        SEGGER_RTT_printf(0, "WARNING: BLE auth init failed\n");
    }
    
    /* Initialize BLE service */
    agsys_ble_init_t ble_init = {
        .device_name = init->device_name,
        .device_type = init->device_type,
        .evt_handler = init->evt_handler,
        .auth_ctx = &ctx->auth_ctx,
        .enable_dfu = false
    };
    
    if (agsys_ble_init(&ctx->ble_ctx, &ble_init) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "WARNING: BLE service init failed\n");
        return false;
    }
    
    /* Initialize FRAM-based log storage (uses FRAM for unlimited write endurance) */
    if (agsys_log_init(&ctx->log_ctx, &ctx->fram_ctx)) {
        ctx->log_available = true;
        SEGGER_RTT_printf(0, "Log (FRAM): %lu entries, %lu pending sync\n",
                          agsys_log_get_total_count(&ctx->log_ctx),
                          agsys_log_get_unsynced_count(&ctx->log_ctx));
    } else {
        SEGGER_RTT_printf(0, "WARNING: FRAM Log init failed\n");
    }
    
    /* Initialize Flash (if CS pin provided) - used only for OTA firmware storage */
    if (init->flash_cs_pin != 0) {
        if (agsys_flash_init(&ctx->flash_ctx, init->flash_cs_pin) == AGSYS_OK) {
            ctx->flash_available = true;
            SEGGER_RTT_printf(0, "Flash: W25Q%02X detected (%lu KB)\n",
                              ctx->flash_ctx.device_id,
                              ctx->flash_ctx.capacity / 1024);
            
            /* Initialize firmware backup (flash is only for OTA, not logging) */
            if (agsys_backup_init(&ctx->backup_ctx, &ctx->flash_ctx)) {
                ctx->backup_available = true;
                SEGGER_RTT_printf(0, "Backup: Slot A=%d, Slot B=%d\n",
                                  ctx->backup_ctx.header.slot_a_status,
                                  ctx->backup_ctx.header.slot_b_status);
            } else {
                SEGGER_RTT_printf(0, "WARNING: Backup init failed\n");
            }
        } else {
            SEGGER_RTT_printf(0, "WARNING: Flash init failed (CS=%d)\n", init->flash_cs_pin);
        }
    }
    
    ctx->initialized = true;
    SEGGER_RTT_printf(0, "Device initialized: %s (type=%d)\n", 
                      init->device_name, init->device_type);
    return true;
}

/* ==========================================================================
 * BLE ADVERTISING CONTROL
 * ========================================================================== */

void agsys_device_start_advertising(agsys_device_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    agsys_ble_advertising_start(&ctx->ble_ctx);
}

void agsys_device_stop_advertising(agsys_device_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    agsys_ble_advertising_stop(&ctx->ble_ctx);
}

/* ==========================================================================
 * AUTHENTICATION STATUS
 * ========================================================================== */

bool agsys_device_is_authenticated(const agsys_device_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    return agsys_ble_is_authenticated(&ctx->ble_ctx);
}

/* ==========================================================================
 * LOGGING API
 * ========================================================================== */

bool agsys_device_log_sensor(agsys_device_ctx_t *ctx, const uint16_t *readings,
                              uint8_t count, uint16_t battery_mv)
{
    if (ctx == NULL || !ctx->log_available) {
        return false;
    }
    return agsys_log_sensor_reading(&ctx->log_ctx, ctx->device_type,
                                     readings, count, battery_mv);
}

bool agsys_device_log_meter(agsys_device_ctx_t *ctx, uint32_t flow_rate_mlpm,
                             uint32_t total_volume_ml, uint8_t alarm_flags)
{
    if (ctx == NULL || !ctx->log_available) {
        return false;
    }
    return agsys_log_meter_reading(&ctx->log_ctx, flow_rate_mlpm,
                                    total_volume_ml, alarm_flags);
}

bool agsys_device_log_valve(agsys_device_ctx_t *ctx, uint8_t valve_id,
                             uint8_t event_type, uint8_t position)
{
    if (ctx == NULL || !ctx->log_available) {
        return false;
    }
    return agsys_log_valve_event(&ctx->log_ctx, valve_id, event_type, position);
}

bool agsys_device_log_alarm(agsys_device_ctx_t *ctx, uint8_t alarm_type,
                             uint8_t severity, uint16_t code, const char *message)
{
    if (ctx == NULL || !ctx->log_available) {
        return false;
    }
    return agsys_log_alarm(&ctx->log_ctx, alarm_type, severity, code, message);
}

uint32_t agsys_device_log_pending_count(agsys_device_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->log_available) {
        return 0;
    }
    return agsys_log_get_unsynced_count(&ctx->log_ctx);
}

bool agsys_device_log_mark_synced(agsys_device_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->log_available) {
        return false;
    }
    return agsys_log_mark_synced(&ctx->log_ctx);
}

/* ==========================================================================
 * FIRMWARE BACKUP API
 * ========================================================================== */

bool agsys_device_backup_validation_pending(agsys_device_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->backup_available) {
        return false;
    }
    return agsys_backup_is_validation_pending(&ctx->backup_ctx);
}

void agsys_device_backup_validate(agsys_device_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->backup_available) {
        return;
    }
    agsys_backup_validate(&ctx->backup_ctx);
}

bool agsys_device_backup_create(agsys_device_ctx_t *ctx, uint32_t fw_size,
                                 uint8_t major, uint8_t minor, uint8_t patch)
{
    if (ctx == NULL || !ctx->backup_available) {
        return false;
    }
    return agsys_backup_create(&ctx->backup_ctx, fw_size, major, minor, patch);
}

bool agsys_device_backup_check_timeout(agsys_device_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->backup_available) {
        return false;
    }
    return agsys_backup_check_validation_timeout(&ctx->backup_ctx);
}
