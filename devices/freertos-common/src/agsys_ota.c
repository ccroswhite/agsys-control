/**
 * @file agsys_ota.c
 * @brief Over-The-Air Firmware Update Module
 * 
 * Transport-agnostic OTA implementation. Works with both BLE and LoRa.
 * Firmware chunks are staged in external flash, then applied to internal flash.
 */

#include "agsys_ota.h"
#include "agsys_common.h"
#include "nrf.h"
#include "nrf_nvmc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* ==========================================================================
 * INTERNAL FLASH PARAMETERS
 * ========================================================================== */

#define INTERNAL_FLASH_PAGE_SIZE    4096
#define INTERNAL_FLASH_APP_START    0x26000     /* After SoftDevice */
#define INTERNAL_FLASH_APP_END      0x7E000     /* Before bootloader settings */
#define INTERNAL_FLASH_APP_SIZE     (INTERNAL_FLASH_APP_END - INTERNAL_FLASH_APP_START)

/* ==========================================================================
 * CRC32 CALCULATION
 * ========================================================================== */

static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void crc32_init_table(void)
{
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc32_init_table();
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

/* ==========================================================================
 * PROGRESS CALLBACK
 * ========================================================================== */

static agsys_ota_progress_cb_t m_progress_callback = NULL;
static void *m_progress_user_data = NULL;
static agsys_ota_complete_cb_t m_complete_callback = NULL;
static void *m_complete_user_data = NULL;

static void notify_progress(agsys_ota_ctx_t *ctx)
{
    if (m_progress_callback != NULL) {
        uint8_t progress = agsys_ota_get_progress(ctx);
        m_progress_callback(ctx->status, progress, m_progress_user_data);
    }
}

static void notify_complete(bool success, agsys_ota_error_t error)
{
    if (m_complete_callback != NULL) {
        m_complete_callback(success, error, m_complete_user_data);
        /* Give time for ACK to be sent */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool agsys_ota_init(agsys_ota_ctx_t *ctx, 
                     agsys_flash_ctx_t *flash,
                     agsys_backup_ctx_t *backup)
{
    if (ctx == NULL || flash == NULL || backup == NULL) {
        return false;
    }
    
    memset(ctx, 0, sizeof(agsys_ota_ctx_t));
    ctx->flash = flash;
    ctx->backup = backup;
    ctx->status = AGSYS_OTA_STATUS_IDLE;
    ctx->initialized = true;
    
    /* Check if we're pending confirmation from previous update */
    if (agsys_backup_is_validation_pending(backup)) {
        ctx->status = AGSYS_OTA_STATUS_PENDING_CONFIRM;
        SEGGER_RTT_printf(0, "OTA: Pending confirmation from previous update\n");
    }
    
    SEGGER_RTT_printf(0, "OTA: Initialized\n");
    return true;
}

/* ==========================================================================
 * START OTA SESSION
 * ========================================================================== */

agsys_ota_error_t agsys_ota_start(agsys_ota_ctx_t *ctx,
                                   uint32_t fw_size,
                                   uint32_t fw_crc,
                                   uint8_t major,
                                   uint8_t minor,
                                   uint8_t patch)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_OTA_ERR_NOT_STARTED;
    }
    
    if (ctx->status != AGSYS_OTA_STATUS_IDLE && 
        ctx->status != AGSYS_OTA_STATUS_PENDING_CONFIRM) {
        return AGSYS_OTA_ERR_ALREADY_IN_PROGRESS;
    }
    
    if (fw_size > AGSYS_OTA_STAGING_SIZE || fw_size > INTERNAL_FLASH_APP_SIZE) {
        SEGGER_RTT_printf(0, "OTA: Firmware too large (%lu bytes)\n", fw_size);
        return AGSYS_OTA_ERR_SIZE_MISMATCH;
    }
    
    SEGGER_RTT_printf(0, "OTA: Starting update - size=%lu, crc=0x%08lX, v%d.%d.%d\n",
                      fw_size, fw_crc, major, minor, patch);
    
    /* Store expected values */
    ctx->expected_size = fw_size;
    ctx->expected_crc = fw_crc;
    ctx->expected_version[0] = major;
    ctx->expected_version[1] = minor;
    ctx->expected_version[2] = patch;
    ctx->expected_version[3] = 0;
    
    ctx->bytes_received = 0;
    ctx->chunks_received = 0;
    ctx->staging_offset = 0;
    ctx->start_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    /* Step 1: Backup current firmware */
    ctx->status = AGSYS_OTA_STATUS_BACKUP_IN_PROGRESS;
    notify_progress(ctx);
    
    SEGGER_RTT_printf(0, "OTA: Backing up current firmware...\n");
    
    /* Get current firmware size from internal flash */
    /* For now, assume max size - in production, read from app header */
    uint32_t current_fw_size = INTERNAL_FLASH_APP_SIZE;
    
    if (!agsys_backup_create(ctx->backup, current_fw_size, 0, 0, 0)) {
        SEGGER_RTT_printf(0, "OTA: Backup failed\n");
        ctx->status = AGSYS_OTA_STATUS_ERROR;
        ctx->last_error = AGSYS_OTA_ERR_BACKUP_FAILED;
        return AGSYS_OTA_ERR_BACKUP_FAILED;
    }
    
    SEGGER_RTT_printf(0, "OTA: Backup complete\n");
    
    /* Step 2: Erase staging area */
    SEGGER_RTT_printf(0, "OTA: Erasing staging area...\n");
    
    uint32_t sectors_needed = (fw_size + AGSYS_FLASH_SECTOR_SIZE - 1) / AGSYS_FLASH_SECTOR_SIZE;
    uint32_t staging_sector_start = AGSYS_OTA_STAGING_ADDR / AGSYS_FLASH_SECTOR_SIZE;
    
    for (uint32_t i = 0; i < sectors_needed; i++) {
        if (!agsys_flash_erase_sector(ctx->flash, staging_sector_start + i)) {
            SEGGER_RTT_printf(0, "OTA: Failed to erase sector %lu\n", staging_sector_start + i);
            ctx->status = AGSYS_OTA_STATUS_ERROR;
            ctx->last_error = AGSYS_OTA_ERR_FLASH_ERASE;
            return AGSYS_OTA_ERR_FLASH_ERASE;
        }
    }
    
    SEGGER_RTT_printf(0, "OTA: Staging area ready (%lu sectors)\n", sectors_needed);
    
    /* Ready to receive chunks */
    ctx->status = AGSYS_OTA_STATUS_RECEIVING;
    notify_progress(ctx);
    
    return AGSYS_OTA_ERR_NONE;
}

/* ==========================================================================
 * WRITE FIRMWARE CHUNK
 * ========================================================================== */

agsys_ota_error_t agsys_ota_write_chunk(agsys_ota_ctx_t *ctx,
                                         uint32_t offset,
                                         const uint8_t *data,
                                         size_t len)
{
    if (ctx == NULL || !ctx->initialized || data == NULL || len == 0) {
        return AGSYS_OTA_ERR_INVALID_CHUNK;
    }
    
    if (ctx->status != AGSYS_OTA_STATUS_RECEIVING) {
        return AGSYS_OTA_ERR_NOT_STARTED;
    }
    
    /* Validate offset and length */
    if (offset + len > ctx->expected_size) {
        SEGGER_RTT_printf(0, "OTA: Chunk exceeds expected size\n");
        return AGSYS_OTA_ERR_INVALID_CHUNK;
    }
    
    /* Write to staging area in external flash */
    uint32_t flash_addr = AGSYS_OTA_STAGING_ADDR + offset;
    
    if (!agsys_flash_write(ctx->flash, flash_addr, data, len)) {
        SEGGER_RTT_printf(0, "OTA: Failed to write chunk at offset %lu\n", offset);
        ctx->last_error = AGSYS_OTA_ERR_FLASH_WRITE;
        return AGSYS_OTA_ERR_FLASH_WRITE;
    }
    
    ctx->bytes_received += len;
    ctx->chunks_received++;
    
    /* Log progress every 10% */
    uint8_t progress = agsys_ota_get_progress(ctx);
    static uint8_t last_logged_progress = 0;
    if (progress >= last_logged_progress + 10) {
        SEGGER_RTT_printf(0, "OTA: %d%% (%lu/%lu bytes)\n", 
                          progress, ctx->bytes_received, ctx->expected_size);
        last_logged_progress = progress;
        notify_progress(ctx);
    }
    
    return AGSYS_OTA_ERR_NONE;
}

/* ==========================================================================
 * FINISH OTA UPDATE
 * ========================================================================== */

agsys_ota_error_t agsys_ota_finish(agsys_ota_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return AGSYS_OTA_ERR_NOT_STARTED;
    }
    
    if (ctx->status != AGSYS_OTA_STATUS_RECEIVING) {
        return AGSYS_OTA_ERR_NOT_STARTED;
    }
    
    /* Verify we received all bytes */
    if (ctx->bytes_received != ctx->expected_size) {
        SEGGER_RTT_printf(0, "OTA: Size mismatch - received %lu, expected %lu\n",
                          ctx->bytes_received, ctx->expected_size);
        ctx->last_error = AGSYS_OTA_ERR_SIZE_MISMATCH;
        return AGSYS_OTA_ERR_SIZE_MISMATCH;
    }
    
    /* Step 1: Verify CRC of staged firmware */
    ctx->status = AGSYS_OTA_STATUS_VERIFYING;
    notify_progress(ctx);
    
    SEGGER_RTT_printf(0, "OTA: Verifying firmware CRC...\n");
    
    uint32_t calculated_crc = 0;
    uint8_t buffer[256];
    uint32_t remaining = ctx->expected_size;
    uint32_t offset = 0;
    
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        
        if (!agsys_flash_read(ctx->flash, AGSYS_OTA_STAGING_ADDR + offset, buffer, chunk)) {
            SEGGER_RTT_printf(0, "OTA: Failed to read staged firmware\n");
            ctx->last_error = AGSYS_OTA_ERR_CRC_MISMATCH;
            return AGSYS_OTA_ERR_CRC_MISMATCH;
        }
        
        calculated_crc = crc32_update(calculated_crc, buffer, chunk);
        offset += chunk;
        remaining -= chunk;
    }
    
    if (calculated_crc != ctx->expected_crc) {
        SEGGER_RTT_printf(0, "OTA: CRC mismatch - calculated 0x%08lX, expected 0x%08lX\n",
                          calculated_crc, ctx->expected_crc);
        ctx->last_error = AGSYS_OTA_ERR_CRC_MISMATCH;
        return AGSYS_OTA_ERR_CRC_MISMATCH;
    }
    
    SEGGER_RTT_printf(0, "OTA: CRC verified\n");
    
    /* Step 2: Apply to internal flash */
    ctx->status = AGSYS_OTA_STATUS_APPLYING;
    notify_progress(ctx);
    
    SEGGER_RTT_printf(0, "OTA: Applying firmware to internal flash...\n");
    
    /* Erase internal flash pages */
    uint32_t pages_needed = (ctx->expected_size + INTERNAL_FLASH_PAGE_SIZE - 1) / INTERNAL_FLASH_PAGE_SIZE;
    
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint32_t page_addr = INTERNAL_FLASH_APP_START + (i * INTERNAL_FLASH_PAGE_SIZE);
        nrf_nvmc_page_erase(page_addr);
    }
    
    /* Copy from external flash to internal flash */
    remaining = ctx->expected_size;
    offset = 0;
    
    while (remaining > 0) {
        size_t chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        
        /* Read from external flash */
        if (!agsys_flash_read(ctx->flash, AGSYS_OTA_STAGING_ADDR + offset, buffer, chunk)) {
            SEGGER_RTT_printf(0, "OTA: Failed to read from staging\n");
            ctx->last_error = AGSYS_OTA_ERR_INTERNAL_FLASH;
            return AGSYS_OTA_ERR_INTERNAL_FLASH;
        }
        
        /* Write to internal flash */
        nrf_nvmc_write_bytes(INTERNAL_FLASH_APP_START + offset, buffer, chunk);
        
        offset += chunk;
        remaining -= chunk;
    }
    
    SEGGER_RTT_printf(0, "OTA: Firmware applied successfully\n");
    
    /* Step 3: Set expected version and start validation timer */
    agsys_backup_set_expected_version(ctx->backup,
                                       ctx->expected_version[0],
                                       ctx->expected_version[1],
                                       ctx->expected_version[2]);
    agsys_backup_start_validation_timer(ctx->backup);
    
    ctx->status = AGSYS_OTA_STATUS_PENDING_REBOOT;
    notify_progress(ctx);
    
    /* Notify completion (sends ACK to BLE/LoRa) */
    notify_complete(true, AGSYS_OTA_ERR_NONE);
    
    SEGGER_RTT_printf(0, "OTA: Ready to reboot\n");
    
    return AGSYS_OTA_ERR_NONE;
}

/* ==========================================================================
 * ABORT / REBOOT / CONFIRM
 * ========================================================================== */

void agsys_ota_abort(agsys_ota_ctx_t *ctx)
{
    if (ctx == NULL) return;
    
    SEGGER_RTT_printf(0, "OTA: Aborted\n");
    
    ctx->status = AGSYS_OTA_STATUS_IDLE;
    ctx->bytes_received = 0;
    ctx->chunks_received = 0;
    ctx->last_error = AGSYS_OTA_ERR_NONE;
}

void agsys_ota_reboot(void)
{
    SEGGER_RTT_printf(0, "OTA: Rebooting...\n");
    vTaskDelay(pdMS_TO_TICKS(100));  /* Allow log to flush */
    NVIC_SystemReset();
}

void agsys_ota_confirm(agsys_ota_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) return;
    
    SEGGER_RTT_printf(0, "OTA: Firmware confirmed\n");
    
    agsys_backup_validate(ctx->backup);
    ctx->status = AGSYS_OTA_STATUS_IDLE;
}

bool agsys_ota_is_confirm_pending(agsys_ota_ctx_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) return false;
    return ctx->status == AGSYS_OTA_STATUS_PENDING_CONFIRM;
}

/* ==========================================================================
 * STATUS / PROGRESS
 * ========================================================================== */

agsys_ota_status_t agsys_ota_get_status(agsys_ota_ctx_t *ctx)
{
    if (ctx == NULL) return AGSYS_OTA_STATUS_IDLE;
    return ctx->status;
}

agsys_ota_error_t agsys_ota_get_error(agsys_ota_ctx_t *ctx)
{
    if (ctx == NULL) return AGSYS_OTA_ERR_NONE;
    return ctx->last_error;
}

uint8_t agsys_ota_get_progress(agsys_ota_ctx_t *ctx)
{
    if (ctx == NULL || ctx->expected_size == 0) return 0;
    
    uint32_t progress = (ctx->bytes_received * 100) / ctx->expected_size;
    return (progress > 100) ? 100 : (uint8_t)progress;
}

void agsys_ota_set_progress_callback(agsys_ota_ctx_t *ctx,
                                      agsys_ota_progress_cb_t callback,
                                      void *user_data)
{
    m_progress_callback = callback;
    m_progress_user_data = user_data;
}

void agsys_ota_set_complete_callback(agsys_ota_ctx_t *ctx,
                                      agsys_ota_complete_cb_t callback,
                                      void *user_data)
{
    m_complete_callback = callback;
    m_complete_user_data = user_data;
}

/* Task handles registered by device for suspend/resume */
static TaskHandle_t m_registered_tasks[8];
static uint8_t m_registered_task_count = 0;

void agsys_ota_register_task(TaskHandle_t task)
{
    if (m_registered_task_count < 8 && task != NULL) {
        m_registered_tasks[m_registered_task_count++] = task;
    }
}

void agsys_ota_suspend_tasks(void)
{
    SEGGER_RTT_printf(0, "OTA: Suspending %d tasks\n", m_registered_task_count);
    for (uint8_t i = 0; i < m_registered_task_count; i++) {
        if (m_registered_tasks[i] != NULL) {
            vTaskSuspend(m_registered_tasks[i]);
        }
    }
}

void agsys_ota_resume_tasks(void)
{
    SEGGER_RTT_printf(0, "OTA: Resuming %d tasks\n", m_registered_task_count);
    for (uint8_t i = 0; i < m_registered_task_count; i++) {
        if (m_registered_tasks[i] != NULL) {
            vTaskResume(m_registered_tasks[i]);
        }
    }
}
