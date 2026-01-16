/**
 * @file agsys_fram_log.c
 * @brief FRAM-based Log Storage Implementation
 * 
 * Uses FRAM for log storage to ensure unlimited write endurance over
 * the 10-year product lifetime. External flash should only be used
 * for OTA firmware storage.
 */

#include "agsys_fram_log.h"
#include "agsys_common.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* Logging macros using SEGGER RTT */
#define LOG_DEBUG(fmt, ...)   SEGGER_RTT_printf(0, "[LOG] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   SEGGER_RTT_printf(0, "[LOG ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) SEGGER_RTT_printf(0, "[LOG WARN] " fmt "\n", ##__VA_ARGS__)

/* ==========================================================================
 * CRC32 IMPLEMENTATION
 * ========================================================================== */

static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/* ==========================================================================
 * INTERNAL HELPERS
 * ========================================================================== */

static bool write_header(agsys_log_ctx_t *ctx)
{
    /* Calculate CRC (excluding the CRC field itself) */
    ctx->header.crc = crc32((const uint8_t *)&ctx->header,
                            sizeof(ctx->header) - sizeof(uint32_t));
    
    agsys_err_t err = agsys_fram_write(ctx->fram, AGSYS_LOG_FRAM_START,
                                        (const uint8_t *)&ctx->header,
                                        sizeof(ctx->header));
    return (err == AGSYS_OK);
}

static bool read_header(agsys_log_ctx_t *ctx)
{
    agsys_err_t err = agsys_fram_read(ctx->fram, AGSYS_LOG_FRAM_START,
                                       (uint8_t *)&ctx->header,
                                       sizeof(ctx->header));
    if (err != AGSYS_OK) {
        return false;
    }
    
    /* Verify CRC */
    uint32_t calc_crc = crc32((const uint8_t *)&ctx->header,
                              sizeof(ctx->header) - sizeof(uint32_t));
    return (ctx->header.crc == calc_crc);
}

static uint32_t entry_address(uint32_t index)
{
    return AGSYS_LOG_DATA_START + (index * AGSYS_LOG_ENTRY_SIZE);
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool agsys_log_init(agsys_log_ctx_t *ctx, agsys_fram_ctx_t *fram)
{
    if (ctx == NULL || fram == NULL) {
        return false;
    }
    
    memset(ctx, 0, sizeof(agsys_log_ctx_t));
    ctx->fram = fram;
    
    /* Try to read existing header */
    if (read_header(ctx) && ctx->header.magic == AGSYS_LOG_MAGIC) {
        LOG_DEBUG("FRAM Log: Loaded existing header (seq=%lu, unsynced=%lu)",
                        (unsigned long)ctx->header.sequence,
                        (unsigned long)ctx->header.unsynced_count);
        ctx->initialized = true;
        return true;
    }
    
    /* Initialize new header */
    LOG_DEBUG("FRAM Log: Initializing new log storage");
    memset(&ctx->header, 0, sizeof(ctx->header));
    ctx->header.magic = AGSYS_LOG_MAGIC;
    ctx->header.version = AGSYS_LOG_VERSION;
    ctx->header.head_index = 0;
    ctx->header.tail_index = 0;
    ctx->header.total_entries = 0;
    ctx->header.unsynced_count = 0;
    ctx->header.sequence = 1;
    ctx->header.wrap_count = 0;
    
    if (!write_header(ctx)) {
        LOG_ERROR("FRAM Log: Failed to write initial header");
        return false;
    }
    
    ctx->initialized = true;
    return true;
}

/* ==========================================================================
 * WRITE OPERATIONS
 * ========================================================================== */

bool agsys_log_write(agsys_log_ctx_t *ctx, agsys_log_type_t type,
                     const void *payload, size_t payload_len)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (payload_len > AGSYS_LOG_PAYLOAD_SIZE) {
        payload_len = AGSYS_LOG_PAYLOAD_SIZE;
    }
    
    /* Build entry */
    agsys_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.header.timestamp = 0;  /* TODO: Get RTC time */
    entry.header.sequence = ctx->header.sequence;
    entry.header.type = type;
    entry.header.flags = AGSYS_LOG_FLAG_VALID;
    entry.header.payload_len = payload_len;
    
    if (payload != NULL && payload_len > 0) {
        memcpy(entry.payload, payload, payload_len);
    }
    
    /* Calculate CRC */
    entry.crc = crc32((const uint8_t *)&entry,
                      sizeof(entry) - sizeof(entry.crc) - sizeof(entry.reserved));
    
    /* Write entry to FRAM */
    uint32_t addr = entry_address(ctx->header.head_index);
    agsys_err_t err = agsys_fram_write(ctx->fram, addr,
                                        (const uint8_t *)&entry,
                                        sizeof(entry));
    if (err != AGSYS_OK) {
        LOG_ERROR("FRAM Log: Write failed at 0x%05lX", (unsigned long)addr);
        return false;
    }
    
    /* Update header */
    ctx->header.sequence++;
    ctx->header.total_entries++;
    ctx->header.unsynced_count++;
    ctx->header.head_index++;
    
    /* Handle wrap-around */
    if (ctx->header.head_index >= AGSYS_LOG_MAX_ENTRIES) {
        ctx->header.head_index = 0;
        ctx->header.wrap_count++;
        LOG_DEBUG("FRAM Log: Wrapped (count=%lu)", 
                        (unsigned long)ctx->header.wrap_count);
    }
    
    /* If we've caught up to tail, advance tail (oldest entry overwritten) */
    if (ctx->header.head_index == ctx->header.tail_index && 
        ctx->header.total_entries > AGSYS_LOG_MAX_ENTRIES) {
        ctx->header.tail_index++;
        if (ctx->header.tail_index >= AGSYS_LOG_MAX_ENTRIES) {
            ctx->header.tail_index = 0;
        }
        if (ctx->header.unsynced_count > 0) {
            ctx->header.unsynced_count--;
        }
    }
    
    /* Write updated header */
    if (!write_header(ctx)) {
        LOG_ERROR("FRAM Log: Header update failed");
        return false;
    }
    
    return true;
}

/* ==========================================================================
 * READ OPERATIONS
 * ========================================================================== */

bool agsys_log_read_oldest(agsys_log_ctx_t *ctx, agsys_log_entry_t *entry)
{
    if (ctx == NULL || !ctx->initialized || entry == NULL) {
        return false;
    }
    
    if (ctx->header.unsynced_count == 0) {
        return false;  /* No unsynced entries */
    }
    
    /* Read entry at tail */
    uint32_t addr = entry_address(ctx->header.tail_index);
    agsys_err_t err = agsys_fram_read(ctx->fram, addr,
                                       (uint8_t *)entry,
                                       sizeof(agsys_log_entry_t));
    if (err != AGSYS_OK) {
        LOG_ERROR("FRAM Log: Read failed at 0x%05lX", (unsigned long)addr);
        return false;
    }
    
    /* Verify CRC */
    uint32_t calc_crc = crc32((const uint8_t *)entry,
                              sizeof(*entry) - sizeof(entry->crc) - sizeof(entry->reserved));
    if (entry->crc != calc_crc) {
        LOG_WARNING("FRAM Log: CRC mismatch at index %lu",
                          (unsigned long)ctx->header.tail_index);
        return false;
    }
    
    /* Verify valid flag */
    if (!(entry->header.flags & AGSYS_LOG_FLAG_VALID)) {
        LOG_WARNING("FRAM Log: Invalid entry at index %lu",
                          (unsigned long)ctx->header.tail_index);
        return false;
    }
    
    return true;
}

bool agsys_log_mark_synced(agsys_log_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (ctx->header.unsynced_count == 0) {
        return true;  /* Nothing to mark */
    }
    
    /* Read entry, set synced flag, write back */
    agsys_log_entry_t entry;
    uint32_t addr = entry_address(ctx->header.tail_index);
    
    agsys_err_t err = agsys_fram_read(ctx->fram, addr,
                                       (uint8_t *)&entry,
                                       sizeof(entry));
    if (err != AGSYS_OK) {
        return false;
    }
    
    entry.header.flags |= AGSYS_LOG_FLAG_SYNCED;
    entry.crc = crc32((const uint8_t *)&entry,
                      sizeof(entry) - sizeof(entry.crc) - sizeof(entry.reserved));
    
    err = agsys_fram_write(ctx->fram, addr,
                           (const uint8_t *)&entry,
                           sizeof(entry));
    if (err != AGSYS_OK) {
        return false;
    }
    
    /* Advance tail */
    ctx->header.tail_index++;
    if (ctx->header.tail_index >= AGSYS_LOG_MAX_ENTRIES) {
        ctx->header.tail_index = 0;
    }
    ctx->header.unsynced_count--;
    
    return write_header(ctx);
}

uint32_t agsys_log_get_unsynced_count(agsys_log_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return 0;
    }
    return ctx->header.unsynced_count;
}

uint32_t agsys_log_get_total_count(agsys_log_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return 0;
    }
    return ctx->header.total_entries;
}

bool agsys_log_erase_all(agsys_log_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* Reset header */
    ctx->header.head_index = 0;
    ctx->header.tail_index = 0;
    ctx->header.total_entries = 0;
    ctx->header.unsynced_count = 0;
    /* Keep sequence and wrap_count for debugging */
    
    LOG_DEBUG("FRAM Log: Erased all entries");
    return write_header(ctx);
}

/* ==========================================================================
 * CONVENIENCE FUNCTIONS
 * ========================================================================== */

bool agsys_log_sensor_reading(agsys_log_ctx_t *ctx, uint8_t device_type,
                               const uint16_t *readings, uint8_t count,
                               uint16_t battery_mv)
{
    agsys_log_sensor_t payload;
    memset(&payload, 0, sizeof(payload));
    
    payload.device_type = device_type;
    payload.probe_count = count;
    payload.battery_mv = battery_mv;
    
    if (count > 4) count = 4;
    for (uint8_t i = 0; i < count; i++) {
        payload.readings[i] = readings[i];
    }
    
    return agsys_log_write(ctx, AGSYS_LOG_TYPE_SENSOR_READING,
                           &payload, sizeof(payload));
}

bool agsys_log_meter_reading(agsys_log_ctx_t *ctx, uint32_t flow_rate_mlpm,
                              uint32_t total_volume_ml, uint8_t alarm_flags)
{
    agsys_log_meter_t payload;
    memset(&payload, 0, sizeof(payload));
    
    payload.flow_rate_mlpm = flow_rate_mlpm;
    payload.total_volume_ml = total_volume_ml;
    payload.alarm_flags = alarm_flags;
    
    return agsys_log_write(ctx, AGSYS_LOG_TYPE_METER_READING,
                           &payload, sizeof(payload));
}

bool agsys_log_valve_event(agsys_log_ctx_t *ctx, uint8_t valve_id,
                            uint8_t event_type, uint8_t position)
{
    agsys_log_valve_t payload;
    memset(&payload, 0, sizeof(payload));
    
    payload.valve_id = valve_id;
    payload.event_type = event_type;
    payload.position = position;
    
    return agsys_log_write(ctx, AGSYS_LOG_TYPE_VALVE_EVENT,
                           &payload, sizeof(payload));
}

bool agsys_log_alarm(agsys_log_ctx_t *ctx, uint8_t alarm_type,
                      uint8_t severity, uint16_t code, const char *message)
{
    agsys_log_alarm_t payload;
    memset(&payload, 0, sizeof(payload));
    
    payload.alarm_type = alarm_type;
    payload.severity = severity;
    payload.alarm_code = code;
    
    if (message != NULL) {
        strncpy(payload.message, message, sizeof(payload.message) - 1);
    }
    
    return agsys_log_write(ctx, AGSYS_LOG_TYPE_ALARM,
                           &payload, sizeof(payload));
}
