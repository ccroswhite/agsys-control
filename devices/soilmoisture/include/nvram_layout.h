/**
 * @file nvram_layout.h
 * @brief FRAM memory layout with protected regions for configuration persistence
 * 
 * This file defines the memory map for the external SPI FRAM.
 * Configuration data is stored in protected regions that survive firmware updates.
 * 
 * IMPORTANT: The OTA process must NEVER write to protected regions.
 * 
 * Device Identity:
 *   Device ID is read from nRF52832 FICR (factory-programmed, 64-bit).
 *   No UUID storage needed in FRAM - identity is tied to the chip.
 *   Customer/location info is managed in the backend database.
 * 
 * Memory Map (8KB FRAM):
 * ┌─────────────────────────────────────────────────────────────┐
 * │ PROTECTED REGION (0x0000 - 0x00FF) - 256 bytes              │
 * │ ├── Calibration (0x0000 - 0x003F) - 64 bytes                │
 * │ └── User Configuration (0x0040 - 0x00BF) - 128 bytes        │
 * │ └── Reserved (0x00C0 - 0x00FF) - 64 bytes                   │
 * ├─────────────────────────────────────────────────────────────┤
 * │ FIRMWARE-MANAGED REGION (0x0100 - 0x01FF) - 256 bytes       │
 * │ ├── Runtime State (0x0100 - 0x017F) - 128 bytes             │
 * │ └── Statistics (0x0180 - 0x01FF) - 128 bytes                │
 * ├─────────────────────────────────────────────────────────────┤
 * │ OTA STAGING REGION (0x0200 - 0x1BFF) - 6.5KB                │
 * │ └── Firmware chunks during OTA                              │
 * ├─────────────────────────────────────────────────────────────┤
 * │ LOG REGION (0x1C00 - 0x1FFF) - 1KB                          │
 * │ └── Circular log buffer                                     │
 * └─────────────────────────────────────────────────────────────┘
 */

#ifndef NVRAM_LAYOUT_H
#define NVRAM_LAYOUT_H

#include <stdint.h>

/* ==========================================================================
 * FRAM SIZE AND REGIONS
 * ========================================================================== */
#define NVRAM_TOTAL_SIZE            8192    // 8KB total FRAM

// Region boundaries
#define NVRAM_PROTECTED_START       0x0000
#define NVRAM_PROTECTED_SIZE        0x0100  // 256 bytes

#define NVRAM_MANAGED_START         0x0100
#define NVRAM_MANAGED_SIZE          0x0100  // 256 bytes

#define NVRAM_OTA_START             0x0200
#define NVRAM_OTA_SIZE              0x1A00  // 6.5KB for firmware staging

#define NVRAM_LOG_START             0x1C00
#define NVRAM_LOG_SIZE              0x0400  // 1KB for logs

/* ==========================================================================
 * DEVICE IDENTITY - Read from nRF52832 FICR (not stored in FRAM)
 * ========================================================================== */
// The 64-bit device ID is factory-programmed in the chip's FICR registers.
// Access via: NRF_FICR->DEVICEID[0] and NRF_FICR->DEVICEID[1]
// This eliminates the need for UUID storage in FRAM.

/* ==========================================================================
 * PROTECTED REGION - Survives all firmware updates
 * ========================================================================== */

// Calibration Block (0x0000 - 0x003F, 64 bytes)
#define NVRAM_CALIBRATION_ADDR      0x0000
#define NVRAM_CALIBRATION_SIZE      64

// Calibration block structure offsets (relative to NVRAM_CALIBRATION_ADDR)
#define CAL_MAGIC_OFFSET            0x00    // 4 bytes: 0x43414C49 ("CALI")
#define CAL_VERSION_OFFSET          0x04    // 1 byte: calibration version
#define CAL_FLAGS_OFFSET            0x05    // 1 byte: calibration flags
#define CAL_RESERVED1_OFFSET        0x06    // 2 bytes: reserved
#define CAL_MOISTURE_DRY_OFFSET     0x08    // 2 bytes: dry ADC value
#define CAL_MOISTURE_WET_OFFSET     0x0A    // 2 bytes: wet ADC value
#define CAL_MOISTURE_TEMP_COEF      0x0C    // 2 bytes: temperature coefficient (0.01 units)
#define CAL_BATTERY_OFFSET_OFFSET   0x0E    // 2 bytes: battery voltage offset (mV)
#define CAL_BATTERY_SCALE_OFFSET    0x10    // 2 bytes: battery voltage scale (0.001 units)
#define CAL_TEMP_OFFSET_OFFSET      0x12    // 2 bytes: temperature offset (0.1°C)
#define CAL_LORA_FREQ_OFFSET        0x14    // 4 bytes: LoRa frequency offset (Hz)
#define CAL_RESERVED2_OFFSET        0x18    // 36 bytes: reserved for future calibration
#define CAL_CRC_OFFSET              0x3C    // 4 bytes: CRC32 of calibration block

// User Configuration Block (0x0040 - 0x00BF, 128 bytes)
#define NVRAM_USER_CONFIG_ADDR      0x0040
#define NVRAM_USER_CONFIG_SIZE      128

// User config structure offsets (relative to NVRAM_USER_CONFIG_ADDR)
#define CFG_MAGIC_OFFSET            0x00    // 4 bytes: 0x55534552 ("USER")
#define CFG_VERSION_OFFSET          0x04    // 1 byte: config version
#define CFG_FLAGS_OFFSET            0x05    // 1 byte: config flags
#define CFG_RESERVED1_OFFSET        0x06    // 2 bytes: reserved
#define CFG_SLEEP_INTERVAL_OFFSET   0x08    // 4 bytes: sleep interval (seconds)
#define CFG_REPORT_INTERVAL_OFFSET  0x0C    // 4 bytes: report interval (seconds)
#define CFG_LOW_BATT_THRESH_OFFSET  0x10    // 2 bytes: low battery threshold (mV)
#define CFG_CRIT_BATT_THRESH_OFFSET 0x12    // 2 bytes: critical battery threshold (mV)
#define CFG_MOISTURE_LOW_OFFSET     0x14    // 1 byte: moisture low alarm (%)
#define CFG_MOISTURE_HIGH_OFFSET    0x15    // 1 byte: moisture high alarm (%)
#define CFG_LORA_TX_POWER_OFFSET    0x16    // 1 byte: LoRa TX power (dBm)
#define CFG_LORA_SF_OFFSET          0x17    // 1 byte: LoRa spreading factor
#define CFG_GATEWAY_ID_OFFSET       0x18    // 8 bytes: paired gateway device ID
#define CFG_NETWORK_KEY_OFFSET      0x20    // 16 bytes: network encryption key
#define CFG_RESERVED2_OFFSET        0x30    // 72 bytes: reserved for future config
#define CFG_CRC_OFFSET              0x7C    // 4 bytes: CRC32 of config block

// Reserved for future protected data (0x00C0 - 0x00FF, 64 bytes)
#define NVRAM_PROTECTED_RESERVED    0x00C0
#define NVRAM_PROTECTED_RESERVED_SZ 64

/* ==========================================================================
 * FIRMWARE-MANAGED REGION - May be cleared on major version changes
 * ========================================================================== */

// Runtime State Block (0x0100 - 0x017F, 128 bytes)
#define NVRAM_RUNTIME_STATE_ADDR    0x0100
#define NVRAM_RUNTIME_STATE_SIZE    128

// Runtime state structure offsets (relative to NVRAM_RUNTIME_STATE_ADDR)
#define STATE_MAGIC_OFFSET          0x00    // 4 bytes: 0x52554E54 ("RUNT")
#define STATE_VERSION_OFFSET        0x04    // 1 byte: state version
#define STATE_FLAGS_OFFSET          0x05    // 1 byte: state flags
#define STATE_BOOT_COUNT_OFFSET     0x06    // 4 bytes: boot counter
#define STATE_LAST_BOOT_OFFSET      0x0A    // 4 bytes: last boot timestamp
#define STATE_LAST_REPORT_OFFSET    0x0E    // 4 bytes: last successful report timestamp
#define STATE_LAST_ACK_SEQ_OFFSET   0x12    // 2 bytes: last acknowledged sequence
#define STATE_PENDING_LOGS_OFFSET   0x14    // 2 bytes: number of pending log entries
#define STATE_FW_VERSION_OFFSET     0x16    // 4 bytes: current firmware version (for migration)
#define STATE_PREV_FW_VERSION       0x1A    // 4 bytes: previous firmware version
#define STATE_OTA_STATUS_OFFSET     0x1E    // 1 byte: OTA status
#define STATE_OTA_PROGRESS_OFFSET   0x1F    // 1 byte: OTA progress (%)
#define STATE_OTA_ANNOUNCE_ID       0x20    // 4 bytes: current OTA announce ID
#define STATE_OTA_CHUNKS_RECV       0x24    // 2 bytes: chunks received
#define STATE_OTA_TOTAL_CHUNKS      0x26    // 2 bytes: total chunks
#define STATE_RESERVED_OFFSET       0x28    // 84 bytes: reserved
#define STATE_CRC_OFFSET            0x7C    // 4 bytes: CRC32 of state block

// Statistics Block (0x0180 - 0x01FF, 128 bytes)
#define NVRAM_STATS_ADDR            0x0180
#define NVRAM_STATS_SIZE            128

// Statistics structure offsets (relative to NVRAM_STATS_ADDR)
#define STATS_MAGIC_OFFSET          0x00    // 4 bytes: 0x53544154 ("STAT")
#define STATS_VERSION_OFFSET        0x04    // 1 byte: stats version
#define STATS_RESERVED1_OFFSET      0x05    // 3 bytes: reserved
#define STATS_TX_SUCCESS_OFFSET     0x08    // 4 bytes: successful transmissions
#define STATS_TX_FAIL_OFFSET        0x0C    // 4 bytes: failed transmissions
#define STATS_RX_SUCCESS_OFFSET     0x10    // 4 bytes: successful receptions
#define STATS_RX_FAIL_OFFSET        0x14    // 4 bytes: failed receptions
#define STATS_OTA_SUCCESS_OFFSET    0x18    // 2 bytes: successful OTA updates
#define STATS_OTA_FAIL_OFFSET       0x1A    // 2 bytes: failed OTA updates
#define STATS_RESET_COUNT_OFFSET    0x1C    // 2 bytes: unexpected reset count
#define STATS_LOW_BATT_COUNT        0x1E    // 2 bytes: low battery events
#define STATS_MIN_BATT_MV_OFFSET    0x20    // 2 bytes: minimum battery voltage seen
#define STATS_MAX_TEMP_OFFSET       0x22    // 2 bytes: maximum temperature seen
#define STATS_MIN_TEMP_OFFSET       0x24    // 2 bytes: minimum temperature seen
#define STATS_UPTIME_HOURS_OFFSET   0x26    // 4 bytes: total uptime in hours
#define STATS_RESERVED2_OFFSET      0x2A    // 82 bytes: reserved
#define STATS_CRC_OFFSET            0x7C    // 4 bytes: CRC32 of stats block

/* ==========================================================================
 * OTA STAGING REGION - Temporary storage during firmware updates
 * ========================================================================== */

// OTA Header (first 64 bytes of OTA region)
#define NVRAM_OTA_HEADER_ADDR       0x0200
#define NVRAM_OTA_HEADER_SIZE       64

// OTA header structure offsets (relative to NVRAM_OTA_HEADER_ADDR)
#define OTA_HDR_MAGIC_OFFSET        0x00    // 4 bytes: 0x4F544148 ("OTAH")
#define OTA_HDR_ANNOUNCE_ID         0x04    // 4 bytes: announce ID
#define OTA_HDR_FW_SIZE             0x08    // 4 bytes: firmware size
#define OTA_HDR_FW_CRC              0x0C    // 4 bytes: firmware CRC32
#define OTA_HDR_TOTAL_CHUNKS        0x10    // 2 bytes: total chunks
#define OTA_HDR_CHUNK_SIZE          0x12    // 2 bytes: chunk size
#define OTA_HDR_VERSION_MAJOR       0x14    // 1 byte: target version major
#define OTA_HDR_VERSION_MINOR       0x15    // 1 byte: target version minor
#define OTA_HDR_VERSION_PATCH       0x16    // 1 byte: target version patch
#define OTA_HDR_STATUS              0x17    // 1 byte: OTA status
#define OTA_HDR_CHUNKS_BITMAP       0x18    // 32 bytes: bitmap of received chunks (256 chunks max)
#define OTA_HDR_CRC                 0x3C    // 4 bytes: header CRC32

// OTA Firmware Data (after header)
#define NVRAM_OTA_DATA_ADDR         0x0240
#define NVRAM_OTA_DATA_SIZE         (NVRAM_OTA_SIZE - 64)  // ~6.4KB

/* ==========================================================================
 * LOG REGION - Circular buffer for sensor readings
 * ========================================================================== */

// Log Header (first 16 bytes)
#define NVRAM_LOG_HEADER_ADDR       0x1C00
#define NVRAM_LOG_HEADER_SIZE       16

// Log header structure offsets
#define LOG_HDR_MAGIC_OFFSET        0x00    // 4 bytes: 0x4C4F4748 ("LOGH")
#define LOG_HDR_VERSION_OFFSET      0x04    // 1 byte: log format version
#define LOG_HDR_FLAGS_OFFSET        0x05    // 1 byte: flags
#define LOG_HDR_HEAD_OFFSET         0x06    // 2 bytes: head index
#define LOG_HDR_TAIL_OFFSET         0x08    // 2 bytes: tail index
#define LOG_HDR_COUNT_OFFSET        0x0A    // 2 bytes: entry count
#define LOG_HDR_RESERVED_OFFSET     0x0C    // 4 bytes: reserved

// Log Entries (after header)
#define NVRAM_LOG_ENTRIES_ADDR      0x1C10
#define NVRAM_LOG_ENTRY_SIZE        16      // 16 bytes per entry
#define NVRAM_LOG_MAX_ENTRIES       ((NVRAM_LOG_SIZE - NVRAM_LOG_HEADER_SIZE) / NVRAM_LOG_ENTRY_SIZE)

// Log entry structure (16 bytes each)
// Offset 0: timestamp (4 bytes)
// Offset 4: moisture_raw (2 bytes)
// Offset 6: moisture_percent (1 byte)
// Offset 7: battery_mv (2 bytes, divided by 10)
// Offset 9: temperature (2 bytes, 0.1°C)
// Offset 11: flags (1 byte)
// Offset 12: reserved (4 bytes)

/* ==========================================================================
 * MAGIC VALUES
 * ========================================================================== */
#define NVRAM_MAGIC_CALIBRATION     0x43414C49  // "CALI"
#define NVRAM_MAGIC_USER_CONFIG     0x55534552  // "USER"
#define NVRAM_MAGIC_RUNTIME         0x52554E54  // "RUNT"
#define NVRAM_MAGIC_STATS           0x53544154  // "STAT"
#define NVRAM_MAGIC_OTA_HEADER      0x4F544148  // "OTAH"
#define NVRAM_MAGIC_LOG_HEADER      0x4C4F4748  // "LOGH"

/* ==========================================================================
 * VERSION NUMBERS (for data migration)
 * ========================================================================== */
#define NVRAM_CALIBRATION_VERSION   1
#define NVRAM_USER_CONFIG_VERSION   1
#define NVRAM_RUNTIME_VERSION       1
#define NVRAM_STATS_VERSION         1
#define NVRAM_LOG_VERSION           1

/* ==========================================================================
 * CONFIGURATION FLAGS
 * ========================================================================== */

// Calibration flags
#define CAL_FLAG_FACTORY_CAL        0x01    // Factory calibration present
#define CAL_FLAG_FIELD_CAL          0x02    // Field calibration applied
#define CAL_FLAG_TEMP_COMPENSATED   0x04    // Temperature compensation enabled

// Config flags
#define CFG_FLAG_PAIRED             0x01    // Paired with controller
#define CFG_FLAG_ENCRYPTED          0x02    // Encryption enabled
#define CFG_FLAG_ALARMS_ENABLED     0x04    // Moisture alarms enabled

// State flags
#define STATE_FLAG_OTA_IN_PROGRESS  0x01    // OTA update in progress
#define STATE_FLAG_FIRST_BOOT       0x02    // First boot after OTA
#define STATE_FLAG_CONFIG_DIRTY     0x04    // Config changed, needs sync

/* ==========================================================================
 * HELPER MACROS
 * ========================================================================== */

// Check if an address is in the protected region
#define NVRAM_IS_PROTECTED(addr) \
    ((addr) >= NVRAM_PROTECTED_START && (addr) < (NVRAM_PROTECTED_START + NVRAM_PROTECTED_SIZE))

// Check if an address is in the OTA staging region
#define NVRAM_IS_OTA_REGION(addr) \
    ((addr) >= NVRAM_OTA_START && (addr) < (NVRAM_OTA_START + NVRAM_OTA_SIZE))

#endif // NVRAM_LAYOUT_H
