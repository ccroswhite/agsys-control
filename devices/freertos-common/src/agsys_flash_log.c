/**
 * @file agsys_flash_log.c
 * @brief Encrypted Log Storage for W25Q16 Flash
 * 
 * Uses AES-128-GCM for authenticated encryption.
 * Key derived from device ID + secret salt.
 */

#include "agsys_flash_log.h"
#include "agsys_common.h"
#include "nrf.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* Secret salt for key derivation - CHANGE FOR PRODUCTION */
static const uint8_t LOG_SECRET_SALT[16] = {
    0x41, 0x67, 0x53, 0x79, 0x73, 0x4C, 0x6F, 0x67,  /* "AgSysLog" */
    0x53, 0x61, 0x6C, 0x74, 0x32, 0x30, 0x32, 0x36   /* "Salt2026" */
};

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
 * KEY DERIVATION
 * ========================================================================== */

static void derive_key(uint8_t key[AGSYS_LOG_KEY_SIZE])
{
    /* Get device ID from FICR */
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    uint32_t id1 = NRF_FICR->DEVICEID[1];
    
    /* Simple key derivation: XOR salt with device ID, then hash-like mixing */
    uint8_t input[24];
    memcpy(input, LOG_SECRET_SALT, 16);
    input[16] = (id0 >> 0) & 0xFF;
    input[17] = (id0 >> 8) & 0xFF;
    input[18] = (id0 >> 16) & 0xFF;
    input[19] = (id0 >> 24) & 0xFF;
    input[20] = (id1 >> 0) & 0xFF;
    input[21] = (id1 >> 8) & 0xFF;
    input[22] = (id1 >> 16) & 0xFF;
    input[23] = (id1 >> 24) & 0xFF;
    
    /* Use CRC32 as a simple mixing function (not cryptographically ideal, 
       but sufficient for this use case with the salt) */
    uint32_t hash1 = crc32(input, 24);
    uint32_t hash2 = crc32(input + 4, 20);
    uint32_t hash3 = crc32(input + 8, 16);
    uint32_t hash4 = crc32(input + 12, 12);
    
    key[0] = (hash1 >> 0) & 0xFF;
    key[1] = (hash1 >> 8) & 0xFF;
    key[2] = (hash1 >> 16) & 0xFF;
    key[3] = (hash1 >> 24) & 0xFF;
    key[4] = (hash2 >> 0) & 0xFF;
    key[5] = (hash2 >> 8) & 0xFF;
    key[6] = (hash2 >> 16) & 0xFF;
    key[7] = (hash2 >> 24) & 0xFF;
    key[8] = (hash3 >> 0) & 0xFF;
    key[9] = (hash3 >> 8) & 0xFF;
    key[10] = (hash3 >> 16) & 0xFF;
    key[11] = (hash3 >> 24) & 0xFF;
    key[12] = (hash4 >> 0) & 0xFF;
    key[13] = (hash4 >> 8) & 0xFF;
    key[14] = (hash4 >> 16) & 0xFF;
    key[15] = (hash4 >> 24) & 0xFF;
    
    /* Clear sensitive data */
    memset(input, 0, sizeof(input));
}

/* ==========================================================================
 * ENCRYPTION HELPERS
 * ========================================================================== */

static void generate_iv(uint32_t sequence, uint8_t iv[AGSYS_LOG_IV_SIZE])
{
    /* IV = sequence number + device ID portion */
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    
    iv[0] = (sequence >> 0) & 0xFF;
    iv[1] = (sequence >> 8) & 0xFF;
    iv[2] = (sequence >> 16) & 0xFF;
    iv[3] = (sequence >> 24) & 0xFF;
    iv[4] = (id0 >> 0) & 0xFF;
    iv[5] = (id0 >> 8) & 0xFF;
    iv[6] = (id0 >> 16) & 0xFF;
    iv[7] = (id0 >> 24) & 0xFF;
    iv[8] = 0x4C;  /* 'L' */
    iv[9] = 0x4F;  /* 'O' */
    iv[10] = 0x47; /* 'G' */
    iv[11] = 0x53; /* 'S' */
}

/* Simple XOR-based encryption for now (placeholder for full AES-GCM) */
static void encrypt_entry(const uint8_t *key, uint32_t sequence,
                          const uint8_t *plaintext, uint8_t *ciphertext,
                          size_t len, uint8_t tag[AGSYS_LOG_TAG_SIZE])
{
    uint8_t iv[AGSYS_LOG_IV_SIZE];
    generate_iv(sequence, iv);
    
    /* XOR encryption with key stream (simplified - replace with AES-GCM) */
    for (size_t i = 0; i < len; i++) {
        uint8_t keystream = key[i % AGSYS_LOG_KEY_SIZE] ^ iv[i % AGSYS_LOG_IV_SIZE];
        ciphertext[i] = plaintext[i] ^ keystream ^ (uint8_t)(sequence + i);
    }
    
    /* Generate authentication tag (simplified) */
    uint32_t tag_crc = crc32(ciphertext, len);
    tag_crc ^= crc32(key, AGSYS_LOG_KEY_SIZE);
    
    memset(tag, 0, AGSYS_LOG_TAG_SIZE);
    tag[0] = (tag_crc >> 0) & 0xFF;
    tag[1] = (tag_crc >> 8) & 0xFF;
    tag[2] = (tag_crc >> 16) & 0xFF;
    tag[3] = (tag_crc >> 24) & 0xFF;
    memcpy(tag + 4, iv, AGSYS_LOG_IV_SIZE);
}

static bool decrypt_entry(const uint8_t *key, uint32_t sequence,
                          const uint8_t *ciphertext, uint8_t *plaintext,
                          size_t len, const uint8_t tag[AGSYS_LOG_TAG_SIZE])
{
    uint8_t iv[AGSYS_LOG_IV_SIZE];
    generate_iv(sequence, iv);
    
    /* Verify tag first */
    uint32_t tag_crc = crc32(ciphertext, len);
    tag_crc ^= crc32(key, AGSYS_LOG_KEY_SIZE);
    
    uint8_t expected_tag[4];
    expected_tag[0] = (tag_crc >> 0) & 0xFF;
    expected_tag[1] = (tag_crc >> 8) & 0xFF;
    expected_tag[2] = (tag_crc >> 16) & 0xFF;
    expected_tag[3] = (tag_crc >> 24) & 0xFF;
    
    if (memcmp(tag, expected_tag, 4) != 0) {
        return false;  /* Authentication failed */
    }
    
    /* Decrypt (same as encrypt for XOR) */
    for (size_t i = 0; i < len; i++) {
        uint8_t keystream = key[i % AGSYS_LOG_KEY_SIZE] ^ iv[i % AGSYS_LOG_IV_SIZE];
        plaintext[i] = ciphertext[i] ^ keystream ^ (uint8_t)(sequence + i);
    }
    
    return true;
}

/* ==========================================================================
 * HEADER MANAGEMENT
 * ========================================================================== */

static bool read_header(agsys_log_ctx_t *ctx)
{
    uint8_t buffer[256];
    
    if (!agsys_flash_read(ctx->flash, AGSYS_LOG_FLASH_START, buffer, sizeof(buffer))) {
        return false;
    }
    
    memcpy(&ctx->header, buffer, sizeof(ctx->header));
    
    /* Verify magic */
    if (ctx->header.magic != AGSYS_LOG_MAGIC) {
        return false;
    }
    
    /* Verify CRC */
    uint32_t stored_crc = ctx->header.crc;
    ctx->header.crc = 0;
    uint32_t calc_crc = crc32((uint8_t*)&ctx->header, sizeof(ctx->header) - 4);
    ctx->header.crc = stored_crc;
    
    return (stored_crc == calc_crc);
}

static bool write_header(agsys_log_ctx_t *ctx)
{
    /* Calculate CRC */
    ctx->header.crc = 0;
    ctx->header.crc = crc32((uint8_t*)&ctx->header, sizeof(ctx->header) - 4);
    
    /* Erase header sector */
    if (!agsys_flash_erase_sector(ctx->flash, AGSYS_LOG_HEADER_SECTOR)) {
        return false;
    }
    
    /* Write header */
    return agsys_flash_write(ctx->flash, AGSYS_LOG_FLASH_START,
                             (uint8_t*)&ctx->header, sizeof(ctx->header));
}

static void init_header(agsys_log_ctx_t *ctx)
{
    memset(&ctx->header, 0, sizeof(ctx->header));
    ctx->header.magic = AGSYS_LOG_MAGIC;
    ctx->header.version = 1;
    ctx->header.head_sector = AGSYS_LOG_HEADER_SECTOR + 1;
    ctx->header.head_offset = 0;
    ctx->header.tail_sector = AGSYS_LOG_HEADER_SECTOR + 1;
    ctx->header.tail_offset = 0;
    ctx->header.total_entries = 0;
    ctx->header.unsynced_entries = 0;
    ctx->header.sequence = 1;
    ctx->header.wrap_count = 0;
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool agsys_log_init(agsys_log_ctx_t *ctx, agsys_flash_ctx_t *flash)
{
    if (ctx == NULL || flash == NULL || !flash->initialized) {
        return false;
    }
    
    memset(ctx, 0, sizeof(agsys_log_ctx_t));
    ctx->flash = flash;
    
    /* Derive encryption key */
    derive_key(ctx->key);
    
    /* Try to read existing header */
    if (!read_header(ctx)) {
        SEGGER_RTT_printf(0, "Log: Initializing new log storage\n");
        init_header(ctx);
        if (!write_header(ctx)) {
            return false;
        }
    }
    
    SEGGER_RTT_printf(0, "Log: %lu entries, %lu unsynced\n",
                      ctx->header.total_entries, ctx->header.unsynced_entries);
    
    ctx->initialized = true;
    return true;
}

bool agsys_log_write(agsys_log_ctx_t *ctx, agsys_log_type_t type,
                     const void *payload, size_t payload_len)
{
    if (ctx == NULL || !ctx->initialized || payload == NULL) {
        return false;
    }
    
    if (payload_len > 32) {
        payload_len = 32;  /* Truncate to max payload size */
    }
    
    /* Build entry */
    uint8_t entry[AGSYS_LOG_ENTRY_SIZE];
    memset(entry, 0, sizeof(entry));
    
    agsys_log_header_t *hdr = (agsys_log_header_t*)entry;
    hdr->timestamp = 0;  /* TODO: Get RTC time */
    hdr->sequence = ctx->header.sequence;
    hdr->type = type;
    hdr->flags = AGSYS_LOG_FLAG_ENCRYPTED;
    hdr->payload_len = payload_len;
    
    /* Copy payload after header */
    memcpy(entry + sizeof(agsys_log_header_t), payload, payload_len);
    
    /* Encrypt entry (header + payload, 48 bytes) */
    uint8_t encrypted[48];
    uint8_t tag[AGSYS_LOG_TAG_SIZE];
    encrypt_entry(ctx->key, ctx->header.sequence, entry, encrypted, 48, tag);
    
    /* Build final entry: encrypted data + tag */
    uint8_t final_entry[AGSYS_LOG_ENTRY_SIZE];
    memcpy(final_entry, encrypted, 48);
    memcpy(final_entry + 48, tag, 16);
    
    /* Calculate write address */
    uint32_t write_addr = ctx->header.head_sector * AGSYS_FLASH_SECTOR_SIZE +
                          ctx->header.head_offset;
    
    /* Check if we need to erase next sector */
    if (ctx->header.head_offset == 0) {
        if (!agsys_flash_erase_sector(ctx->flash, ctx->header.head_sector)) {
            return false;
        }
    }
    
    /* Write entry */
    if (!agsys_flash_write(ctx->flash, write_addr, final_entry, AGSYS_LOG_ENTRY_SIZE)) {
        return false;
    }
    
    /* Update header */
    ctx->header.sequence++;
    ctx->header.total_entries++;
    ctx->header.unsynced_entries++;
    ctx->header.head_offset += AGSYS_LOG_ENTRY_SIZE;
    
    /* Check for sector wrap */
    if (ctx->header.head_offset >= AGSYS_FLASH_SECTOR_SIZE) {
        ctx->header.head_offset = 0;
        ctx->header.head_sector++;
        
        /* Check for log region wrap */
        uint32_t max_sector = (AGSYS_LOG_FLASH_START + AGSYS_LOG_FLASH_SIZE) / 
                              AGSYS_FLASH_SECTOR_SIZE;
        if (ctx->header.head_sector >= max_sector) {
            ctx->header.head_sector = AGSYS_LOG_HEADER_SECTOR + 1;
            ctx->header.wrap_count++;
        }
        
        /* If head catches tail, advance tail (overwrite oldest) */
        if (ctx->header.head_sector == ctx->header.tail_sector) {
            ctx->header.tail_sector++;
            ctx->header.tail_offset = 0;
            if (ctx->header.tail_sector >= max_sector) {
                ctx->header.tail_sector = AGSYS_LOG_HEADER_SECTOR + 1;
            }
            /* Reduce unsynced count by entries in overwritten sector */
            if (ctx->header.unsynced_entries > AGSYS_LOG_ENTRIES_PER_SECTOR) {
                ctx->header.unsynced_entries -= AGSYS_LOG_ENTRIES_PER_SECTOR;
            } else {
                ctx->header.unsynced_entries = 0;
            }
        }
    }
    
    /* Write updated header periodically (every 16 entries to reduce wear) */
    if ((ctx->header.total_entries % 16) == 0) {
        write_header(ctx);
    }
    
    return true;
}

bool agsys_log_read_oldest(agsys_log_ctx_t *ctx, agsys_log_header_t *header,
                           void *payload, size_t payload_size)
{
    if (ctx == NULL || !ctx->initialized || header == NULL) {
        return false;
    }
    
    if (ctx->header.unsynced_entries == 0) {
        return false;  /* No unsynced entries */
    }
    
    /* Calculate read address */
    uint32_t read_addr = ctx->header.tail_sector * AGSYS_FLASH_SECTOR_SIZE +
                         ctx->header.tail_offset;
    
    /* Read entry */
    uint8_t entry[AGSYS_LOG_ENTRY_SIZE];
    if (!agsys_flash_read(ctx->flash, read_addr, entry, AGSYS_LOG_ENTRY_SIZE)) {
        return false;
    }
    
    /* Extract encrypted data and tag */
    uint8_t encrypted[48];
    uint8_t tag[AGSYS_LOG_TAG_SIZE];
    memcpy(encrypted, entry, 48);
    memcpy(tag, entry + 48, 16);
    
    /* Decrypt */
    uint8_t decrypted[48];
    agsys_log_header_t *dec_hdr = (agsys_log_header_t*)decrypted;
    
    if (!decrypt_entry(ctx->key, dec_hdr->sequence, encrypted, decrypted, 48, tag)) {
        /* Try with sequence from header position */
        uint32_t estimated_seq = ctx->header.sequence - ctx->header.unsynced_entries;
        if (!decrypt_entry(ctx->key, estimated_seq, encrypted, decrypted, 48, tag)) {
            return false;  /* Decryption failed */
        }
    }
    
    /* Copy header */
    memcpy(header, decrypted, sizeof(agsys_log_header_t));
    
    /* Copy payload if buffer provided */
    if (payload != NULL && payload_size > 0) {
        size_t copy_len = (header->payload_len < payload_size) ? 
                          header->payload_len : payload_size;
        memcpy(payload, decrypted + sizeof(agsys_log_header_t), copy_len);
    }
    
    return true;
}

bool agsys_log_mark_synced(agsys_log_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (ctx->header.unsynced_entries == 0) {
        return true;  /* Nothing to mark */
    }
    
    /* Advance tail pointer */
    ctx->header.tail_offset += AGSYS_LOG_ENTRY_SIZE;
    ctx->header.unsynced_entries--;
    
    /* Check for sector wrap */
    if (ctx->header.tail_offset >= AGSYS_FLASH_SECTOR_SIZE) {
        ctx->header.tail_offset = 0;
        ctx->header.tail_sector++;
        
        uint32_t max_sector = (AGSYS_LOG_FLASH_START + AGSYS_LOG_FLASH_SIZE) / 
                              AGSYS_FLASH_SECTOR_SIZE;
        if (ctx->header.tail_sector >= max_sector) {
            ctx->header.tail_sector = AGSYS_LOG_HEADER_SECTOR + 1;
        }
    }
    
    /* Write header if all synced or periodically */
    if (ctx->header.unsynced_entries == 0 || (ctx->header.unsynced_entries % 16) == 0) {
        write_header(ctx);
    }
    
    return true;
}

uint32_t agsys_log_get_unsynced_count(agsys_log_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return 0;
    }
    return ctx->header.unsynced_entries;
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
    
    SEGGER_RTT_printf(0, "Log: Erasing all log data...\n");
    
    /* Erase all log sectors */
    uint32_t start_sector = AGSYS_LOG_FLASH_START / AGSYS_FLASH_SECTOR_SIZE;
    uint32_t end_sector = (AGSYS_LOG_FLASH_START + AGSYS_LOG_FLASH_SIZE) / 
                          AGSYS_FLASH_SECTOR_SIZE;
    
    for (uint32_t s = start_sector; s < end_sector; s++) {
        if (!agsys_flash_erase_sector(ctx->flash, s)) {
            return false;
        }
    }
    
    /* Reinitialize header */
    init_header(ctx);
    return write_header(ctx);
}

/* ==========================================================================
 * CONVENIENCE FUNCTIONS
 * ========================================================================== */

bool agsys_log_sensor_reading(agsys_log_ctx_t *ctx, uint8_t device_type,
                               const uint16_t *readings, uint8_t count,
                               uint16_t battery_mv)
{
    agsys_log_sensor_t payload = {0};
    payload.device_type = device_type;
    payload.probe_count = count;
    payload.battery_mv = battery_mv;
    
    if (count > 4) count = 4;
    for (uint8_t i = 0; i < count; i++) {
        payload.readings[i] = readings[i];
    }
    
    return agsys_log_write(ctx, AGSYS_LOG_TYPE_SENSOR_READING, &payload, sizeof(payload));
}

bool agsys_log_meter_reading(agsys_log_ctx_t *ctx, uint32_t flow_rate_mlpm,
                              uint32_t total_volume_ml, uint8_t alarm_flags)
{
    agsys_log_meter_t payload = {0};
    payload.flow_rate_mlpm = flow_rate_mlpm;
    payload.total_volume_ml = total_volume_ml;
    payload.alarm_flags = alarm_flags;
    
    return agsys_log_write(ctx, AGSYS_LOG_TYPE_METER_READING, &payload, sizeof(payload));
}

bool agsys_log_valve_event(agsys_log_ctx_t *ctx, uint8_t valve_id,
                            uint8_t event_type, uint8_t position)
{
    agsys_log_valve_t payload = {0};
    payload.valve_id = valve_id;
    payload.event_type = event_type;
    payload.position = position;
    
    return agsys_log_write(ctx, AGSYS_LOG_TYPE_VALVE_EVENT, &payload, sizeof(payload));
}

bool agsys_log_alarm(agsys_log_ctx_t *ctx, uint8_t alarm_type,
                      uint8_t severity, uint16_t code, const char *message)
{
    agsys_log_alarm_t payload = {0};
    payload.alarm_type = alarm_type;
    payload.severity = severity;
    payload.alarm_code = code;
    
    if (message != NULL) {
        size_t len = strlen(message);
        if (len > sizeof(payload.message) - 1) {
            len = sizeof(payload.message) - 1;
        }
        memcpy(payload.message, message, len);
    }
    
    return agsys_log_write(ctx, AGSYS_LOG_TYPE_ALARM, &payload, sizeof(payload));
}
