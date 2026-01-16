/**
 * @file main.c
 * @brief AgSys Bootloader - Main Entry Point
 *
 * Full bootloader with:
 * - FRAM boot_info read/write
 * - Boot count tracking and automatic rollback
 * - External flash firmware restore
 * - FRAM-based logging for diagnostics
 *
 * Reuses shared data structures from freertos-common/include/
 */

#include "bl_hal.h"
#include "bl_flash.h"
#include "bl_log.h"
#include "bl_ed25519.h"
#include "agsys_memory_layout.h"
#include <string.h>

/*******************************************************************************
 * Boot Info Structure (matches agsys_memory_layout.h OTA state)
 * 
 * We define a simplified boot_info here that's compatible with the
 * application's view. The bootloader only needs a subset of fields.
 ******************************************************************************/

#define BL_BOOT_INFO_MAGIC      0xB007B007

typedef enum {
    BL_STATE_NORMAL         = 0x00,
    BL_STATE_OTA_STAGED     = 0x01,
    BL_STATE_OTA_PENDING    = 0x02,
    BL_STATE_OTA_CONFIRMED  = 0x03,
    BL_STATE_ROLLBACK       = 0x04,
} bl_boot_state_t;

typedef enum {
    BL_REASON_POWER_ON      = 0x00,
    BL_REASON_WATCHDOG      = 0x01,
    BL_REASON_SOFT_RESET    = 0x02,
    BL_REASON_OTA_REBOOT    = 0x03,
    BL_REASON_ROLLBACK      = 0x04,
    BL_REASON_PANIC         = 0x05,
} bl_boot_reason_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint8_t  boot_state;
    uint8_t  boot_reason;
    uint8_t  boot_count;
    uint8_t  current_version[3];
    uint8_t  reserved1;
    uint8_t  previous_version[3];
    uint8_t  reserved2;
    uint8_t  staged_version[3];
    uint8_t  max_boot_attempts;
    uint32_t last_ota_timestamp;
    uint32_t last_confirm_timestamp;
    uint32_t crc32;
} bl_boot_info_t;

_Static_assert(sizeof(bl_boot_info_t) == 32, "boot_info must be 32 bytes");

#define BL_DEFAULT_MAX_BOOT_ATTEMPTS    3

/*******************************************************************************
 * Boot Decision
 ******************************************************************************/

typedef enum {
    BL_DECISION_JUMP_TO_APP,
    BL_DECISION_ROLLBACK,
    BL_DECISION_PANIC,
} bl_decision_t;

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/

static bool bl_fram_read_boot_info(bl_boot_info_t *info);
static bool bl_fram_write_boot_info(const bl_boot_info_t *info);
static void bl_boot_info_init(bl_boot_info_t *info);
static bool bl_boot_info_validate(const bl_boot_info_t *info);
static void bl_boot_info_update_crc(bl_boot_info_t *info);
static bool bl_validate_app(void);
static bl_decision_t bl_make_decision(bl_boot_info_t *info);

/*******************************************************************************
 * Forward Declarations - Rollback
 ******************************************************************************/

static bool bl_perform_rollback(bl_boot_info_t *info);

/*******************************************************************************
 * Main Entry Point
 ******************************************************************************/

int main(void)
{
    bl_boot_info_t boot_info;
    bl_decision_t decision;
    bool rollback_success = false;
    
    /* Initialize hardware (SPI, GPIO) */
    bl_hal_init();
    
    /* Brief LED flash to indicate bootloader running */
    bl_led_set(true);
    bl_delay_ms(50);
    bl_led_set(false);
    
    /* Initialize logging (best effort - continue if fails) */
    bl_log_init();
    bl_log_increment_boot_count();
    bl_log_write(BL_LOG_BOOT_START, 0, 0);
    
    /* Initialize external flash */
    if (!bl_ext_flash_init()) {
        bl_log_write(BL_LOG_FLASH_ERROR, 0, 0);
        /* Continue - flash only needed for rollback */
    }
    
    /* Read boot info from FRAM */
    if (!bl_fram_read_boot_info(&boot_info)) {
        /* FRAM read failed or invalid - initialize defaults */
        bl_log_write(BL_LOG_FRAM_ERROR, 0, 0);
        bl_boot_info_init(&boot_info);
        bl_fram_write_boot_info(&boot_info);
    }
    
    /* Make boot decision */
    decision = bl_make_decision(&boot_info);
    
    switch (decision) {
        case BL_DECISION_JUMP_TO_APP:
            bl_log_write_version(BL_LOG_BOOT_SUCCESS,
                                 boot_info.current_version[0],
                                 boot_info.current_version[1],
                                 boot_info.current_version[2], 0);
            bl_jump_to_app();
            /* Never returns */
            break;
            
        case BL_DECISION_ROLLBACK:
            bl_log_write(BL_LOG_ROLLBACK_START, 0, 0);
            bl_log_increment_rollback_count();
            
            rollback_success = bl_perform_rollback(&boot_info);
            
            if (rollback_success) {
                bl_log_write_version(BL_LOG_ROLLBACK_SUCCESS,
                                     boot_info.previous_version[0],
                                     boot_info.previous_version[1],
                                     boot_info.previous_version[2], 0);
                
                /* Update boot info to reflect rollback */
                boot_info.boot_state = BL_STATE_ROLLBACK;
                boot_info.boot_reason = BL_REASON_ROLLBACK;
                boot_info.boot_count = 0;
                memcpy(boot_info.current_version, boot_info.previous_version, 3);
                bl_boot_info_update_crc(&boot_info);
                bl_fram_write_boot_info(&boot_info);
                
                /* Jump to restored firmware */
                bl_jump_to_app();
            }
            
            /* Rollback failed */
            bl_log_write(BL_LOG_ROLLBACK_FAIL, 0, 0);
            /* Fall through to panic */
            /* FALLTHROUGH */
            
        case BL_DECISION_PANIC:
        default:
            bl_log_write(BL_LOG_PANIC, 0, 0);
            bl_panic();
            /* Never returns */
            break;
    }
    
    /* Should never reach here */
    bl_panic();
}

/*******************************************************************************
 * Boot Decision Logic
 ******************************************************************************/

static bl_decision_t bl_make_decision(bl_boot_info_t *info)
{
    /* First check if application is valid */
    if (!bl_validate_app()) {
        if (info->boot_state == BL_STATE_OTA_PENDING) {
            return BL_DECISION_ROLLBACK;
        }
        return BL_DECISION_PANIC;
    }
    
    /* Application is valid - check boot state */
    switch (info->boot_state) {
        case BL_STATE_NORMAL:
        case BL_STATE_OTA_CONFIRMED:
        case BL_STATE_ROLLBACK:
            /* Normal boot */
            info->boot_reason = BL_REASON_POWER_ON;
            bl_fram_write_boot_info(info);
            return BL_DECISION_JUMP_TO_APP;
            
        case BL_STATE_OTA_PENDING:
            /* OTA pending - increment boot count */
            info->boot_count++;
            
            if (info->boot_count > info->max_boot_attempts) {
                /* Too many attempts - rollback */
                info->boot_reason = BL_REASON_ROLLBACK;
                bl_fram_write_boot_info(info);
                return BL_DECISION_ROLLBACK;
            }
            
            /* Give new firmware another chance */
            info->boot_reason = BL_REASON_OTA_REBOOT;
            bl_fram_write_boot_info(info);
            return BL_DECISION_JUMP_TO_APP;
            
        case BL_STATE_OTA_STAGED:
            /* Staged but not applied - just boot */
            info->boot_reason = BL_REASON_POWER_ON;
            bl_fram_write_boot_info(info);
            return BL_DECISION_JUMP_TO_APP;
            
        default:
            /* Unknown state - initialize and boot */
            bl_boot_info_init(info);
            bl_fram_write_boot_info(info);
            return BL_DECISION_JUMP_TO_APP;
    }
}

/*******************************************************************************
 * Application Validation
 ******************************************************************************/

#define APP_HEADER_MAGIC    0x41475359  /* "AGSY" */
#define APP_HEADER_OFFSET   0x200       /* After vector table */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t header_version;
    uint8_t  device_type;
    uint8_t  hw_revision_min;
    uint8_t  hw_revision_max;
    uint8_t  reserved1;
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;
    uint8_t  reserved2;
    uint32_t firmware_size;
    uint32_t firmware_crc;
    uint32_t build_timestamp;
    char     build_id[16];
    uint32_t header_crc;
} app_header_t;

static bool bl_validate_app(void)
{
    /* Read application header from internal flash */
    const app_header_t *header = (const app_header_t *)(BL_FLASH_APP_ADDR + APP_HEADER_OFFSET);
    
    /* Check magic */
    if (header->magic != APP_HEADER_MAGIC) {
        return false;
    }
    
    /* Check header CRC */
    uint32_t calc_crc = bl_crc32(header, sizeof(*header) - sizeof(header->header_crc));
    if (calc_crc != header->header_crc) {
        return false;
    }
    
    /* Check firmware size is reasonable */
    if (header->firmware_size == 0 || header->firmware_size > BL_FLASH_APP_SIZE) {
        return false;
    }
    
    /* Check firmware CRC */
    calc_crc = bl_crc32((const void *)BL_FLASH_APP_ADDR, header->firmware_size);
    if (calc_crc != header->firmware_crc) {
        return false;
    }
    
    return true;
}

/*******************************************************************************
 * Boot Info Functions
 ******************************************************************************/

static void bl_boot_info_init(bl_boot_info_t *info)
{
    memset(info, 0, sizeof(*info));
    info->magic = BL_BOOT_INFO_MAGIC;
    info->version = 1;
    info->boot_state = BL_STATE_NORMAL;
    info->boot_reason = BL_REASON_POWER_ON;
    info->boot_count = 0;
    info->max_boot_attempts = BL_DEFAULT_MAX_BOOT_ATTEMPTS;
    bl_boot_info_update_crc(info);
}

static bool bl_boot_info_validate(const bl_boot_info_t *info)
{
    if (info->magic != BL_BOOT_INFO_MAGIC) {
        return false;
    }
    
    uint32_t calc_crc = bl_crc32(info, sizeof(*info) - sizeof(info->crc32));
    return (calc_crc == info->crc32);
}

static void bl_boot_info_update_crc(bl_boot_info_t *info)
{
    info->crc32 = bl_crc32(info, sizeof(*info) - sizeof(info->crc32));
}

/*******************************************************************************
 * FRAM Access
 * 
 * Configurable addressing:
 * - 2-byte for MB85RS64V (8KB) test FRAM
 * - 3-byte for MB85RS1MT (128KB) production FRAM
 ******************************************************************************/

#define FRAM_CMD_WREN   0x06
#define FRAM_CMD_READ   0x03
#define FRAM_CMD_WRITE  0x02

/* Use 2-byte addressing for test FRAM (MB85RS64V 8KB) */
/* Define BL_FRAM_ADDR_BYTES=3 for production (MB85RS1MT 128KB) */
#ifndef BL_FRAM_ADDR_BYTES
#define BL_FRAM_ADDR_BYTES  2
#endif

/* Boot info address - use 0x0010 for both (within 8KB range) */
#define BL_BOOT_INFO_ADDR   0x0010

static void bl_fram_write_enable(void)
{
    uint8_t cmd = FRAM_CMD_WREN;
    bl_fram_select();
    bl_spi_transfer(&cmd, NULL, 1);
    bl_fram_deselect();
}

static bool bl_fram_read_boot_info(bl_boot_info_t *info)
{
#if BL_FRAM_ADDR_BYTES == 2
    /* 2-byte address for MB85RS64V (8KB) */
    uint8_t cmd[3] = {
        FRAM_CMD_READ,
        (uint8_t)(BL_BOOT_INFO_ADDR >> 8),
        (uint8_t)(BL_BOOT_INFO_ADDR & 0xFF),
    };
    bl_fram_select();
    bl_spi_transfer(cmd, NULL, 3);
#else
    /* 3-byte address for MB85RS1MT (128KB) */
    uint8_t cmd[4] = {
        FRAM_CMD_READ,
        (uint8_t)(AGSYS_FRAM_BOOT_INFO_ADDR >> 16),
        (uint8_t)(AGSYS_FRAM_BOOT_INFO_ADDR >> 8),
        (uint8_t)(AGSYS_FRAM_BOOT_INFO_ADDR & 0xFF),
    };
    bl_fram_select();
    bl_spi_transfer(cmd, NULL, 4);
#endif
    bl_spi_transfer(NULL, (uint8_t *)info, sizeof(*info));
    bl_fram_deselect();
    
    return bl_boot_info_validate(info);
}

static bool bl_fram_write_boot_info(const bl_boot_info_t *info)
{
    bl_fram_write_enable();
    
#if BL_FRAM_ADDR_BYTES == 2
    uint8_t cmd[3] = {
        FRAM_CMD_WRITE,
        (uint8_t)(BL_BOOT_INFO_ADDR >> 8),
        (uint8_t)(BL_BOOT_INFO_ADDR & 0xFF),
    };
    bl_fram_select();
    bl_spi_transfer(cmd, NULL, 3);
#else
    uint8_t cmd[4] = {
        FRAM_CMD_WRITE,
        (uint8_t)(AGSYS_FRAM_BOOT_INFO_ADDR >> 16),
        (uint8_t)(AGSYS_FRAM_BOOT_INFO_ADDR >> 8),
        (uint8_t)(AGSYS_FRAM_BOOT_INFO_ADDR & 0xFF),
    };
    bl_fram_select();
    bl_spi_transfer(cmd, NULL, 4);
#endif
    bl_spi_transfer((const uint8_t *)info, NULL, sizeof(*info));
    bl_fram_deselect();
    
    /* Verify write */
    bl_boot_info_t verify;
    return bl_fram_read_boot_info(&verify) && 
           (memcmp(info, &verify, sizeof(*info)) == 0);
}

/*******************************************************************************
 * Rollback Implementation
 ******************************************************************************/

static bool bl_perform_rollback(bl_boot_info_t *info)
{
    (void)info;  /* May use for version info */
    
    /* Try slot A first (primary backup) */
    if (bl_ext_flash_validate_slot(0)) {
        bl_log_write(BL_LOG_ROLLBACK_START, 0, BL_FLASH_SLOT_A_FW_ADDR);
        
        if (bl_ext_flash_restore_firmware(0)) {
            return true;
        }
        bl_log_write(BL_LOG_NVMC_ERROR, 0, 0);
    }
    
    /* Try slot B (secondary backup) */
    if (bl_ext_flash_validate_slot(1)) {
        bl_log_write(BL_LOG_ROLLBACK_START, 1, BL_FLASH_SLOT_B_FW_ADDR);
        
        if (bl_ext_flash_restore_firmware(1)) {
            return true;
        }
        bl_log_write(BL_LOG_NVMC_ERROR, 1, 0);
    }
    
    /* No valid backup found */
    bl_log_write(BL_LOG_APP_INVALID, 0xFF, 0);
    return false;
}
