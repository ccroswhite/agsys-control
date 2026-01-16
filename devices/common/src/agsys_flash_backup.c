/**
 * @file agsys_flash_backup.c
 * @brief Encrypted Firmware Backup and Rollback for W25Q16 Flash
 */

#include "agsys_flash_backup.h"
#include "agsys_common.h"
#include "nrf.h"
#include "nrf_nvmc.h"
#include "SEGGER_RTT.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* Secret salt for key derivation - CHANGE FOR PRODUCTION */
static const uint8_t BACKUP_SECRET_SALT[16] = {
    0x41, 0x67, 0x53, 0x79, 0x73, 0x42, 0x61, 0x63,  /* "AgSysBac" */
    0x6B, 0x75, 0x70, 0x4B, 0x65, 0x79, 0x32, 0x36   /* "kupKey26" */
};

/* nRF52 internal flash parameters */
#define NRF52_FLASH_PAGE_SIZE       4096
#define NRF52_APP_START_ADDR        0x26000     /* After SoftDevice S132/S140 */
#define NRF52_APP_END_ADDR          0x7A000     /* Before bootloader settings */

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

static void derive_key(uint8_t key[16])
{
    uint32_t id0 = NRF_FICR->DEVICEID[0];
    uint32_t id1 = NRF_FICR->DEVICEID[1];
    
    uint8_t input[24];
    memcpy(input, BACKUP_SECRET_SALT, 16);
    input[16] = (id0 >> 0) & 0xFF;
    input[17] = (id0 >> 8) & 0xFF;
    input[18] = (id0 >> 16) & 0xFF;
    input[19] = (id0 >> 24) & 0xFF;
    input[20] = (id1 >> 0) & 0xFF;
    input[21] = (id1 >> 8) & 0xFF;
    input[22] = (id1 >> 16) & 0xFF;
    input[23] = (id1 >> 24) & 0xFF;
    
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
    
    memset(input, 0, sizeof(input));
}

/* ==========================================================================
 * ENCRYPTION (CTR MODE)
 * ========================================================================== */

static void encrypt_block(const uint8_t *key, uint32_t offset,
                          const uint8_t *plaintext, uint8_t *ciphertext,
                          size_t len)
{
    /* Generate keystream based on offset */
    for (size_t i = 0; i < len; i++) {
        uint32_t pos = offset + i;
        uint8_t keystream = key[pos % 16] ^ 
                           ((pos >> 0) & 0xFF) ^
                           ((pos >> 8) & 0xFF) ^
                           ((pos >> 16) & 0xFF);
        ciphertext[i] = plaintext[i] ^ keystream;
    }
}

static void decrypt_block(const uint8_t *key, uint32_t offset,
                          const uint8_t *ciphertext, uint8_t *plaintext,
                          size_t len)
{
    /* CTR mode: decrypt is same as encrypt */
    encrypt_block(key, offset, ciphertext, plaintext, len);
}

/* ==========================================================================
 * HEADER MANAGEMENT
 * ========================================================================== */

static bool read_header(agsys_backup_ctx_t *ctx)
{
    uint8_t buffer[256];
    
    if (!agsys_flash_read(ctx->flash, AGSYS_BACKUP_HEADER_ADDR, buffer, sizeof(buffer))) {
        return false;
    }
    
    memcpy(&ctx->header, buffer, sizeof(ctx->header));
    
    if (ctx->header.magic != AGSYS_BACKUP_MAGIC) {
        return false;
    }
    
    uint32_t stored_crc = ctx->header.header_crc;
    ctx->header.header_crc = 0;
    uint32_t calc_crc = crc32((uint8_t*)&ctx->header, sizeof(ctx->header) - 4);
    ctx->header.header_crc = stored_crc;
    
    return (stored_crc == calc_crc);
}

static bool write_header(agsys_backup_ctx_t *ctx)
{
    ctx->header.header_crc = 0;
    ctx->header.header_crc = crc32((uint8_t*)&ctx->header, sizeof(ctx->header) - 4);
    
    if (!agsys_flash_erase_sector(ctx->flash, 0)) {
        return false;
    }
    
    return agsys_flash_write(ctx->flash, AGSYS_BACKUP_HEADER_ADDR,
                             (uint8_t*)&ctx->header, sizeof(ctx->header));
}

static void init_header(agsys_backup_ctx_t *ctx)
{
    memset(&ctx->header, 0, sizeof(ctx->header));
    ctx->header.magic = AGSYS_BACKUP_MAGIC;
    ctx->header.version = 2;
    ctx->header.active_slot = 0;
    ctx->header.slot_a_status = AGSYS_BACKUP_STATUS_EMPTY;
    ctx->header.slot_b_status = AGSYS_BACKUP_STATUS_EMPTY;
}

/* ==========================================================================
 * PUBLIC API
 * ========================================================================== */

bool agsys_backup_init(agsys_backup_ctx_t *ctx, agsys_flash_ctx_t *flash)
{
    if (ctx == NULL || flash == NULL || !flash->initialized) {
        return false;
    }
    
    memset(ctx, 0, sizeof(agsys_backup_ctx_t));
    ctx->flash = flash;
    
    derive_key(ctx->key);
    
    if (!read_header(ctx)) {
        SEGGER_RTT_printf(0, "Backup: Initializing new backup storage\n");
        init_header(ctx);
        if (!write_header(ctx)) {
            return false;
        }
    }
    
    SEGGER_RTT_printf(0, "Backup: Slot A=%d, Slot B=%d, Active=%d\n",
                      ctx->header.slot_a_status, ctx->header.slot_b_status,
                      ctx->header.active_slot);
    
    ctx->initialized = true;
    return true;
}

bool agsys_backup_check_rollback(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint8_t active_status = (ctx->header.active_slot == 0) ?
                            ctx->header.slot_a_status : ctx->header.slot_b_status;
    
    if (active_status == AGSYS_BACKUP_STATUS_PENDING) {
        /* Check if validation timed out */
        TickType_t now = xTaskGetTickCount();
        TickType_t elapsed = now - ctx->header.validation_start_ms;
        
        if (elapsed >= pdMS_TO_TICKS(AGSYS_BACKUP_VALIDATION_TIMEOUT_MS)) {
            SEGGER_RTT_printf(0, "Backup: Validation timeout - triggering rollback\n");
            
            /* Mark as failed */
            if (ctx->header.active_slot == 0) {
                ctx->header.slot_a_status = AGSYS_BACKUP_STATUS_FAILED;
                memcpy(ctx->header.failed_version, ctx->header.slot_a_version, 4);
            } else {
                ctx->header.slot_b_status = AGSYS_BACKUP_STATUS_FAILED;
                memcpy(ctx->header.failed_version, ctx->header.slot_b_version, 4);
            }
            
            ctx->header.rollback_count++;
            write_header(ctx);
            
            /* Trigger rollback */
            return agsys_backup_restore(ctx);
        }
    }
    
    return false;
}

void agsys_backup_validate(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    
    uint8_t *status = (ctx->header.active_slot == 0) ?
                      &ctx->header.slot_a_status : &ctx->header.slot_b_status;
    
    if (*status == AGSYS_BACKUP_STATUS_PENDING) {
        *status = AGSYS_BACKUP_STATUS_VALID;
        ctx->header.validation_start_ms = 0;
        write_header(ctx);
        SEGGER_RTT_printf(0, "Backup: Firmware validated\n");
    }
}

bool agsys_backup_is_validation_pending(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    uint8_t status = (ctx->header.active_slot == 0) ?
                     ctx->header.slot_a_status : ctx->header.slot_b_status;
    
    return (status == AGSYS_BACKUP_STATUS_PENDING);
}

void agsys_backup_start_validation_timer(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    
    uint8_t *status = (ctx->header.active_slot == 0) ?
                      &ctx->header.slot_a_status : &ctx->header.slot_b_status;
    
    *status = AGSYS_BACKUP_STATUS_PENDING;
    ctx->header.validation_start_ms = xTaskGetTickCount();
    write_header(ctx);
    
    SEGGER_RTT_printf(0, "Backup: Validation timer started (%d ms)\n",
                      AGSYS_BACKUP_VALIDATION_TIMEOUT_MS);
}

void agsys_backup_set_expected_version(agsys_backup_ctx_t *ctx,
                                        uint8_t major, uint8_t minor, uint8_t patch)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    
    ctx->header.expected_version[0] = major;
    ctx->header.expected_version[1] = minor;
    ctx->header.expected_version[2] = patch;
    ctx->header.expected_version[3] = 0;
    write_header(ctx);
}

bool agsys_backup_was_rollback(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL) {
        return false;
    }
    return ctx->rollback_occurred;
}

bool agsys_backup_get_failed_version(agsys_backup_ctx_t *ctx,
                                      uint8_t *major, uint8_t *minor, uint8_t *patch)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (ctx->header.failed_version[0] == 0 &&
        ctx->header.failed_version[1] == 0 &&
        ctx->header.failed_version[2] == 0) {
        return false;
    }
    
    if (major) *major = ctx->header.failed_version[0];
    if (minor) *minor = ctx->header.failed_version[1];
    if (patch) *patch = ctx->header.failed_version[2];
    
    return true;
}

bool agsys_backup_check_validation_timeout(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (!agsys_backup_is_validation_pending(ctx)) {
        return false;
    }
    
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - ctx->header.validation_start_ms;
    
    if (elapsed >= pdMS_TO_TICKS(AGSYS_BACKUP_VALIDATION_TIMEOUT_MS)) {
        SEGGER_RTT_printf(0, "Backup: Validation timeout!\n");
        agsys_backup_force_rollback(ctx);
        return true;
    }
    
    return false;
}

bool agsys_backup_create(agsys_backup_ctx_t *ctx, uint32_t fw_size,
                          uint8_t major, uint8_t minor, uint8_t patch)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    if (fw_size > AGSYS_BACKUP_SLOT_SIZE) {
        SEGGER_RTT_printf(0, "Backup: Firmware too large (%lu > %lu)\n",
                          fw_size, AGSYS_BACKUP_SLOT_SIZE);
        return false;
    }
    
    /* Determine target slot (opposite of active) */
    uint8_t target_slot = (ctx->header.active_slot == 0) ? 1 : 0;
    uint32_t slot_addr = (target_slot == 0) ? AGSYS_BACKUP_SLOT_A_ADDR : 
                                               AGSYS_BACKUP_SLOT_B_ADDR;
    
    SEGGER_RTT_printf(0, "Backup: Creating backup in slot %d (%lu bytes)\n",
                      target_slot, fw_size);
    
    /* Erase target slot sectors */
    uint32_t sectors_needed = (fw_size + AGSYS_FLASH_SECTOR_SIZE - 1) / AGSYS_FLASH_SECTOR_SIZE;
    uint32_t start_sector = slot_addr / AGSYS_FLASH_SECTOR_SIZE;
    
    for (uint32_t s = 0; s < sectors_needed; s++) {
        if (!agsys_flash_erase_sector(ctx->flash, start_sector + s)) {
            return false;
        }
    }
    
    /* Read from internal flash, encrypt, write to external flash */
    uint8_t buffer[256];
    uint32_t remaining = fw_size;
    uint32_t offset = 0;
    uint32_t crc = 0xFFFFFFFF;
    
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        
        /* Read from internal flash */
        memcpy(buffer, (void*)(NRF52_APP_START_ADDR + offset), chunk);
        
        /* Update CRC */
        for (size_t i = 0; i < chunk; i++) {
            crc ^= buffer[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        
        /* Encrypt */
        uint8_t encrypted[256];
        encrypt_block(ctx->key, offset, buffer, encrypted, chunk);
        
        /* Write to external flash */
        if (!agsys_flash_write(ctx->flash, slot_addr + offset, encrypted, chunk)) {
            return false;
        }
        
        offset += chunk;
        remaining -= chunk;
    }
    
    crc = ~crc;
    
    /* Update header */
    if (target_slot == 0) {
        ctx->header.slot_a_size = fw_size;
        ctx->header.slot_a_crc = crc;
        ctx->header.slot_a_status = AGSYS_BACKUP_STATUS_VALID;
        ctx->header.slot_a_version[0] = major;
        ctx->header.slot_a_version[1] = minor;
        ctx->header.slot_a_version[2] = patch;
        ctx->header.slot_a_version[3] = 0;
    } else {
        ctx->header.slot_b_size = fw_size;
        ctx->header.slot_b_crc = crc;
        ctx->header.slot_b_status = AGSYS_BACKUP_STATUS_VALID;
        ctx->header.slot_b_version[0] = major;
        ctx->header.slot_b_version[1] = minor;
        ctx->header.slot_b_version[2] = patch;
        ctx->header.slot_b_version[3] = 0;
    }
    
    write_header(ctx);
    
    SEGGER_RTT_printf(0, "Backup: Created successfully (CRC: 0x%08X)\n", crc);
    return true;
}

bool agsys_backup_restore(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    /* Find valid backup slot (opposite of active) */
    uint8_t backup_slot = (ctx->header.active_slot == 0) ? 1 : 0;
    uint8_t backup_status = (backup_slot == 0) ? ctx->header.slot_a_status :
                                                  ctx->header.slot_b_status;
    uint32_t backup_size = (backup_slot == 0) ? ctx->header.slot_a_size :
                                                 ctx->header.slot_b_size;
    uint32_t backup_crc = (backup_slot == 0) ? ctx->header.slot_a_crc :
                                                ctx->header.slot_b_crc;
    uint32_t slot_addr = (backup_slot == 0) ? AGSYS_BACKUP_SLOT_A_ADDR :
                                               AGSYS_BACKUP_SLOT_B_ADDR;
    
    if (backup_status != AGSYS_BACKUP_STATUS_VALID) {
        SEGGER_RTT_printf(0, "Backup: No valid backup in slot %d\n", backup_slot);
        return false;
    }
    
    SEGGER_RTT_printf(0, "Backup: Restoring from slot %d (%lu bytes)\n",
                      backup_slot, backup_size);
    
    /* Read, decrypt, verify CRC, then write to internal flash */
    uint8_t buffer[256];
    uint8_t decrypted[256];
    uint32_t remaining = backup_size;
    uint32_t offset = 0;
    uint32_t crc = 0xFFFFFFFF;
    
    /* First pass: verify CRC */
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        
        if (!agsys_flash_read(ctx->flash, slot_addr + offset, buffer, chunk)) {
            return false;
        }
        
        decrypt_block(ctx->key, offset, buffer, decrypted, chunk);
        
        for (size_t i = 0; i < chunk; i++) {
            crc ^= decrypted[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        
        offset += chunk;
        remaining -= chunk;
    }
    
    crc = ~crc;
    
    if (crc != backup_crc) {
        SEGGER_RTT_printf(0, "Backup: CRC mismatch (0x%08X != 0x%08X)\n", crc, backup_crc);
        return false;
    }
    
    SEGGER_RTT_printf(0, "Backup: CRC verified, writing to internal flash...\n");
    
    /* Second pass: write to internal flash */
    remaining = backup_size;
    offset = 0;
    
    /* Erase internal flash pages */
    uint32_t pages_needed = (backup_size + NRF52_FLASH_PAGE_SIZE - 1) / NRF52_FLASH_PAGE_SIZE;
    
    for (uint32_t p = 0; p < pages_needed; p++) {
        nrf_nvmc_page_erase(NRF52_APP_START_ADDR + (p * NRF52_FLASH_PAGE_SIZE));
    }
    
    /* Write decrypted firmware */
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        
        if (!agsys_flash_read(ctx->flash, slot_addr + offset, buffer, chunk)) {
            return false;
        }
        
        decrypt_block(ctx->key, offset, buffer, decrypted, chunk);
        
        /* Write to internal flash (must be word-aligned) */
        nrf_nvmc_write_bytes(NRF52_APP_START_ADDR + offset, decrypted, chunk);
        
        offset += chunk;
        remaining -= chunk;
    }
    
    /* Update header - swap active slot */
    ctx->header.active_slot = backup_slot;
    ctx->rollback_occurred = true;
    write_header(ctx);
    
    SEGGER_RTT_printf(0, "Backup: Restore complete, resetting...\n");
    
    /* Reset device */
    NVIC_SystemReset();
    
    return true;  /* Never reached */
}

void agsys_backup_force_rollback(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    
    SEGGER_RTT_printf(0, "Backup: Forcing rollback\n");
    
    /* Mark current slot as failed */
    if (ctx->header.active_slot == 0) {
        ctx->header.slot_a_status = AGSYS_BACKUP_STATUS_FAILED;
        memcpy(ctx->header.failed_version, ctx->header.slot_a_version, 4);
    } else {
        ctx->header.slot_b_status = AGSYS_BACKUP_STATUS_FAILED;
        memcpy(ctx->header.failed_version, ctx->header.slot_b_version, 4);
    }
    
    ctx->header.rollback_count++;
    write_header(ctx);
    
    agsys_backup_restore(ctx);
}

bool agsys_backup_get_status(agsys_backup_ctx_t *ctx, agsys_backup_header_t *header)
{
    if (ctx == NULL || !ctx->initialized || header == NULL) {
        return false;
    }
    
    memcpy(header, &ctx->header, sizeof(agsys_backup_header_t));
    return true;
}

bool agsys_backup_erase_all(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    
    SEGGER_RTT_printf(0, "Backup: Erasing all backup data...\n");
    
    /* Erase first 1MB (backup region) */
    for (uint8_t block = 0; block < 16; block++) {
        if (!agsys_flash_erase_block(ctx->flash, block)) {
            return false;
        }
    }
    
    init_header(ctx);
    return write_header(ctx);
}

uint8_t agsys_backup_get_rollback_count(agsys_backup_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return 0;
    }
    return ctx->header.rollback_count;
}
