/**
 * @file bl_log.c
 * @brief FRAM-based logging implementation
 *
 * Ring buffer log stored in FRAM for boot events and errors.
 */

#include "bl_log.h"
#include "bl_hal.h"
#include <string.h>

/*******************************************************************************
 * External FRAM Functions (from main.c)
 ******************************************************************************/

extern void bl_fram_select(void);
extern void bl_fram_deselect(void);

/*******************************************************************************
 * Private Variables
 ******************************************************************************/

static bl_log_header_t log_header;
static uint32_t sequence_num = 0;
static bool log_initialized = false;

/*******************************************************************************
 * Private FRAM Access
 ******************************************************************************/

#define FRAM_CMD_WREN   0x06
#define FRAM_CMD_READ   0x03
#define FRAM_CMD_WRITE  0x02

/* Use 2-byte addressing for MB85RS64V (8KB) test FRAM */
/* For production MB85RS1MT (128KB), change to 3-byte */
#ifndef BL_FRAM_ADDR_BYTES
#define BL_FRAM_ADDR_BYTES  2
#endif

static void fram_write_enable(void)
{
    uint8_t cmd = FRAM_CMD_WREN;
    bl_fram_select();
    bl_spi_transfer(&cmd, NULL, 1);
    bl_fram_deselect();
}

static bool fram_read(uint32_t addr, uint8_t *data, size_t len)
{
#if BL_FRAM_ADDR_BYTES == 2
    uint8_t cmd[3] = {
        FRAM_CMD_READ,
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };
    bl_fram_select();
    bl_spi_transfer(cmd, NULL, 3);
#else
    uint8_t cmd[4] = {
        FRAM_CMD_READ,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };
    bl_fram_select();
    bl_spi_transfer(cmd, NULL, 4);
#endif
    bl_spi_transfer(NULL, data, len);
    bl_fram_deselect();
    return true;
}

static bool fram_write(uint32_t addr, const uint8_t *data, size_t len)
{
    fram_write_enable();
    
#if BL_FRAM_ADDR_BYTES == 2
    uint8_t cmd[3] = {
        FRAM_CMD_WRITE,
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };
    bl_fram_select();
    bl_spi_transfer(cmd, NULL, 3);
#else
    uint8_t cmd[4] = {
        FRAM_CMD_WRITE,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
    };
    bl_fram_select();
    bl_spi_transfer(cmd, NULL, 4);
#endif
    bl_spi_transfer(data, NULL, len);
    bl_fram_deselect();
    return true;
}

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

static void update_header_crc(void)
{
    log_header.crc32 = bl_crc32(&log_header, 
                                 sizeof(log_header) - sizeof(log_header.crc32));
}

static bool validate_header(void)
{
    if (log_header.magic != BL_LOG_HEADER_MAGIC) {
        return false;
    }
    
    uint32_t calc_crc = bl_crc32(&log_header, 
                                  sizeof(log_header) - sizeof(log_header.crc32));
    return (calc_crc == log_header.crc32);
}

static void init_header(void)
{
    memset(&log_header, 0, sizeof(log_header));
    log_header.magic = BL_LOG_HEADER_MAGIC;
    log_header.write_index = 0;
    log_header.entry_count = 0;
    log_header.boot_count = 0;
    log_header.rollback_count = 0;
    update_header_crc();
}

static bool save_header(void)
{
    update_header_crc();
    return fram_write(BL_LOG_FRAM_ADDR, (const uint8_t *)&log_header, sizeof(log_header));
}

static uint32_t get_entry_addr(uint16_t index)
{
    return BL_LOG_FRAM_ADDR + sizeof(bl_log_header_t) + (index * BL_LOG_ENTRY_SIZE);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

bool bl_log_init(void)
{
    /* Read header from FRAM */
    if (!fram_read(BL_LOG_FRAM_ADDR, (uint8_t *)&log_header, sizeof(log_header))) {
        return false;
    }
    
    /* Validate or initialize */
    if (!validate_header()) {
        init_header();
        if (!save_header()) {
            return false;
        }
    }
    
    /* Set sequence number to continue from last entry */
    if (log_header.entry_count > 0) {
        uint16_t last_index = (log_header.write_index == 0) ? 
                              (BL_LOG_MAX_ENTRIES - 1) : (log_header.write_index - 1);
        bl_log_entry_t last_entry;
        uint32_t addr = get_entry_addr(last_index);
        fram_read(addr, (uint8_t *)&last_entry, sizeof(last_entry));
        sequence_num = last_entry.sequence + 1;
    }
    
    log_initialized = true;
    return true;
}

void bl_log_write(bl_log_type_t type, uint32_t error_code, uint32_t error_addr)
{
    bl_log_write_version(type, 0, 0, 0, error_code);
    (void)error_addr;  /* TODO: add to entry */
}

void bl_log_write_version(bl_log_type_t type, 
                          uint8_t major, uint8_t minor, uint8_t patch,
                          uint32_t error_code)
{
    if (!log_initialized) {
        return;
    }
    
    bl_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.sequence = sequence_num++;
    entry.timestamp = 0;  /* No RTC in bootloader */
    entry.type = type;
    entry.boot_state = 0;  /* Could be passed in */
    entry.boot_count = (uint8_t)log_header.boot_count;
    entry.version[0] = major;
    entry.version[1] = minor;
    entry.version[2] = patch;
    entry.error_code = error_code;
    entry.error_addr = 0;
    
    /* Write entry */
    uint32_t addr = get_entry_addr(log_header.write_index);
    fram_write(addr, (const uint8_t *)&entry, sizeof(entry));
    
    /* Update header */
    log_header.write_index = (log_header.write_index + 1) % BL_LOG_MAX_ENTRIES;
    if (log_header.entry_count < BL_LOG_MAX_ENTRIES) {
        log_header.entry_count++;
    }
    save_header();
}

void bl_log_increment_boot_count(void)
{
    if (!log_initialized) {
        return;
    }
    
    log_header.boot_count++;
    save_header();
}

void bl_log_increment_rollback_count(void)
{
    if (!log_initialized) {
        return;
    }
    
    log_header.rollback_count++;
    save_header();
}

void bl_log_get_stats(uint32_t *boot_count, uint32_t *rollback_count, uint16_t *entry_count)
{
    if (boot_count) *boot_count = log_header.boot_count;
    if (rollback_count) *rollback_count = log_header.rollback_count;
    if (entry_count) *entry_count = log_header.entry_count;
}

bool bl_log_read_entry(uint16_t index, bl_log_entry_t *entry)
{
    if (!log_initialized || entry == NULL) {
        return false;
    }
    
    if (index >= log_header.entry_count) {
        return false;
    }
    
    /* Calculate actual index in ring buffer */
    uint16_t actual_index;
    if (log_header.entry_count < BL_LOG_MAX_ENTRIES) {
        actual_index = index;
    } else {
        actual_index = (log_header.write_index + index) % BL_LOG_MAX_ENTRIES;
    }
    
    uint32_t addr = get_entry_addr(actual_index);
    return fram_read(addr, (uint8_t *)entry, sizeof(*entry));
}
