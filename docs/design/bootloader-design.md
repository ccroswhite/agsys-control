# AgSys Custom Bootloader Design

## Overview

This document describes the design of a custom bootloader for AgSys nRF52-based devices that provides automatic firmware rollback capability when OTA updates fail. The bootloader works alongside Nordic's SoftDevice BLE stack and uses FRAM for persistent state storage.

### Goals

1. **Automatic Rollback** - If new firmware fails to boot properly, automatically restore previous working firmware
2. **Brick Prevention** - Device should never be left in an unrecoverable state after failed OTA
3. **Bootloader Updateability** - Bootloader itself can be safely updated with recovery mechanism
4. **SoftDevice Compatibility** - Work within Nordic's MBR architecture
5. **Minimal Footprint** - Keep bootloader small to maximize application space
6. **Fast Boot** - Normal boots should have minimal delay
7. **10+ Year Lifetime** - Support devices deployed for a decade without physical access

### Non-Goals

- BLE DFU in bootloader (handled by application)
- Firmware encryption during OTA transfer (handled by OTA module)
- Multi-image A/B switching (we use backup/restore model)
- Recovery Loader updates (frozen after manufacturing)

> **Note:** The bootloader DOES implement AES-128-CTR decryption to restore encrypted backups during rollback. This is separate from OTA transfer encryption.

---

## Architecture Overview

The boot architecture uses a **three-stage design** to enable safe bootloader updates:

1. **Recovery Loader (Stage 0.5)** - Frozen, never updated, provides bootloader recovery
2. **Bootloader** - Updatable with safety net from Recovery Loader
3. **Application** - Updatable with rollback from Bootloader

```
┌─────────────────────────────────────────────────────────────────┐
│                    INTERNAL FLASH (512KB)                        │
├─────────────────────────────────────────────────────────────────┤
│ 0x00000000 │ MBR (4KB)           │ Nordic - FROZEN              │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00001000 │ SoftDevice S132     │ BLE Stack (~148KB)           │
│            │ (148KB)             │                              │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00026000 │ Application         │ Main firmware - UPDATABLE    │
│            │ (296KB)             │                              │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00070000 │ Recovery Loader     │ Stage 0.5 - FROZEN           │
│            │ (8KB)               │                              │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00072000 │ Bootloader          │ Our bootloader - UPDATABLE   │
│            │ (16KB)              │                              │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00076000 │ Bootloader Settings │ MBR settings page (4KB)      │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00077000 │ MBR Params          │ MBR parameters (4KB)         │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00078000 │ App Settings        │ Application data (32KB)      │
└────────────┴─────────────────────┴──────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                    EXTERNAL FLASH (W25Q16 - 2MB)                 │
│              A/B Firmware Slots - Future-Proofed                 │
├─────────────────────────────────────────────────────────────────┤
│ 0x00000000 │ Slot A Header       │ Metadata, CRC, version (4KB) │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00001000 │ Slot A Firmware     │ Encrypted backup (944KB)     │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x000ED000 │ Slot B Header       │ Metadata, CRC, version (4KB) │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x000EE000 │ Slot B Firmware     │ OTA staging area (944KB)     │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x001DA000 │ Bootloader Backup   │ Recovery source (16KB)       │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x001DE000 │ Reserved            │ Future expansion (136KB)     │
└────────────┴─────────────────────┴──────────────────────────────┘

> **Note:** A/B slots sized at 944KB to support future larger MCUs (e.g., nRF52840 
> with ~800KB usable flash). Current nRF52832 firmware (~300KB) fits with room to grow.

┌─────────────────────────────────────────────────────────────────┐
│                    FRAM (MB85RS1MT - 128KB)                      │
│              Growth-Buffered Layout with Versioning              │
├─────────────────────────────────────────────────────────────────┤
│ 0x00000    │ Layout Header (16B) │ FROZEN FOREVER - version/CRC │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00010    │ Boot Info (256B)    │ OTA state, versions, flags   │
│ 0x00110    │ [Growth] (256B)     │ Reserved for Boot Info       │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00200    │ Bootloader Info(128B│ CRC for Recovery Loader      │
│ 0x00280    │ [Growth] (128B)     │ Reserved for BL Info         │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00300    │ Device Config (1KB) │ Settings synced from cloud   │
│ 0x00700    │ [Growth] (1KB)      │ Reserved for Config          │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x00B00    │ Calibration (1KB)   │ Sensor calibration data      │
│ 0x00F00    │ [Growth] (1KB)      │ Reserved for Calibration     │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x01300    │ App Data (8KB)      │ Device-specific runtime data │
│ 0x03300    │ [Growth] (8KB)      │ Reserved for App Data        │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x05300    │ Ring Buffer Log(16KB│ Runtime logs (~128 entries)  │
│ 0x09300    │ [Growth] (16KB)     │ Reserved for Logs            │
├────────────┼─────────────────────┼──────────────────────────────┤
│ 0x0D300    │ Future Use (~76KB)  │ Unallocated for new features │
└────────────┴─────────────────────┴──────────────────────────────┘

> **Layout Versioning:** The Layout Header at 0x0000 is FROZEN FOREVER and contains
> a version number. When firmware detects a version mismatch, it runs migration code
> to move data to new locations before using it. Growth buffers between regions allow
> expanding without shifting subsequent data - minimizing layout version changes.
```

### Component Update Strategy

| Component | Size | Update Strategy | Recovery |
|-----------|------|-----------------|----------|
| MBR | 4KB | Never (Nordic) | N/A |
| SoftDevice | 148KB | Rare, with application | Application rollback |
| Application | 296KB | Normal OTA | Bootloader restores backup |
| Recovery Loader | 8KB | **NEVER** (frozen) | None - must be correct |
| Bootloader | 16KB | Rare, careful rollout | Recovery Loader restores backup |

---

## Recovery Loader (Stage 0.5)

### Purpose

The Recovery Loader is a minimal, **frozen** component that provides a safety net for bootloader updates. It runs before the bootloader and can restore a corrupted bootloader from external flash.

### Design Principles

1. **Extremely Simple** - ~200-300 lines of C, no complex logic
2. **Never Updated** - Frozen after manufacturing, bugs are permanent
3. **Single Purpose** - Only checks bootloader CRC and restores if needed
4. **No Dependencies** - Bare metal, no RTOS, no SDK

### Recovery Loader Flow

```
┌─────────────┐
│    MBR      │
└──────┬──────┘
       ▼
┌─────────────────────────────────────┐
│        RECOVERY LOADER              │
│                                     │
│  1. Read bootloader CRC from FRAM   │
│  2. Calculate actual bootloader CRC │
│  3. Compare CRCs                    │
└──────┬──────────────────────────────┘
       │
       ▼
┌─────────────────────┐     ┌─────────────────────────────┐
│ CRC Valid?          │─NO─▶│ RESTORE BOOTLOADER          │
└──────┬──────────────┘     │                             │
       │ YES                │ 1. Read from external flash │
       ▼                    │ 2. Erase bootloader region  │
┌─────────────────────┐     │ 3. Write bootloader         │
│ Jump to Bootloader  │     │ 4. Update CRC in FRAM       │
└─────────────────────┘     │ 5. Jump to bootloader       │
                            └─────────────────────────────┘
```

### Recovery Loader Code Structure

```c
// recovery_loader/main.c - ENTIRE RECOVERY LOADER (~200 lines)

#define BOOTLOADER_ADDR     0x00072000
#define BOOTLOADER_SIZE     0x4000      // 16KB
#define BOOTLOADER_CRC_FRAM 0x0030      // FRAM address for expected CRC
#define BOOTLOADER_BACKUP   0x00100000  // External flash address

void recovery_loader_main(void)
{
    // 1. Initialize minimal SPI for FRAM and external flash
    spi_init();
    
    // 2. Read expected CRC from FRAM
    uint32_t expected_crc;
    fram_read(BOOTLOADER_CRC_FRAM, &expected_crc, 4);
    
    // 3. Calculate actual bootloader CRC
    uint32_t actual_crc = crc32((void *)BOOTLOADER_ADDR, BOOTLOADER_SIZE);
    
    // 4. If mismatch, restore from backup
    if (expected_crc != actual_crc) {
        restore_bootloader_from_backup();
    }
    
    // 5. Jump to bootloader
    jump_to_address(BOOTLOADER_ADDR);
}

void restore_bootloader_from_backup(void)
{
    // Blink LED to indicate recovery in progress
    led_on();
    
    // Erase bootloader region
    for (uint32_t addr = BOOTLOADER_ADDR; addr < BOOTLOADER_ADDR + BOOTLOADER_SIZE; addr += 4096) {
        flash_erase_page(addr);
    }
    
    // Copy from external flash
    uint8_t buffer[256];
    for (uint32_t offset = 0; offset < BOOTLOADER_SIZE; offset += 256) {
        ext_flash_read(BOOTLOADER_BACKUP + offset, buffer, 256);
        flash_write(BOOTLOADER_ADDR + offset, buffer, 256);
    }
    
    // Update CRC in FRAM
    uint32_t new_crc = crc32((void *)BOOTLOADER_ADDR, BOOTLOADER_SIZE);
    fram_write(BOOTLOADER_CRC_FRAM, &new_crc, 4);
    
    led_off();
}
```

### Recovery Loader Limitations (Accepted)

Since the Recovery Loader is frozen, any bugs are permanent. Acceptable because:

1. **Code is trivial** - CRC check + flash copy, easily verified
2. **No protocol evolution** - Doesn't communicate externally
3. **Hardware interfaces stable** - SPI, NVMC don't change
4. **Workarounds possible** - Most bugs can be worked around in bootloader

| Bug Type | Workaround Available? |
|----------|----------------------|
| CRC algorithm wrong | Ship bootloader with matching "wrong" CRC |
| SPI timing issue | Adjust external flash in bootloader backup |
| Wrong jump address | **No** - caught in testing |
| Flash write bug | **No** - caught in testing |

The "no workaround" bugs are **easily caught in basic testing** before any device ships.

---

## FRAM Boot Info Structure

The bootloader and application share this structure stored in FRAM at address 0x0000:

```c
#define AGSYS_BOOT_INFO_MAGIC       0xB007B007
#define AGSYS_BOOT_INFO_VERSION     1
#define AGSYS_BOOT_INFO_FRAM_ADDR   0x0000

typedef enum {
    AGSYS_BOOT_STATE_NORMAL = 0x00,       // Normal operation
    AGSYS_BOOT_STATE_OTA_STAGED = 0x01,   // New firmware staged, not yet applied
    AGSYS_BOOT_STATE_OTA_PENDING = 0x02,  // New firmware applied, awaiting confirmation
    AGSYS_BOOT_STATE_OTA_CONFIRMED = 0x03,// New firmware confirmed working
    AGSYS_BOOT_STATE_ROLLBACK = 0x04,     // Rollback occurred
} agsys_boot_state_t;

typedef enum {
    AGSYS_BOOT_REASON_POWER_ON = 0x00,    // Normal power-on
    AGSYS_BOOT_REASON_WATCHDOG = 0x01,    // Watchdog reset
    AGSYS_BOOT_REASON_SOFT_RESET = 0x02,  // Software reset (intentional)
    AGSYS_BOOT_REASON_OTA_REBOOT = 0x03,  // Reboot after OTA apply
    AGSYS_BOOT_REASON_ROLLBACK = 0x04,    // Reboot after rollback
    AGSYS_BOOT_REASON_PANIC = 0x05,       // Hard fault / panic
} agsys_boot_reason_t;

typedef struct {
    /* Header */
    uint32_t magic;                       // AGSYS_BOOT_INFO_MAGIC
    uint8_t  version;                     // Structure version
    uint8_t  boot_state;                  // agsys_boot_state_t
    uint8_t  boot_reason;                 // agsys_boot_reason_t (set by bootloader)
    uint8_t  boot_count;                  // Boots since last confirmed state
    
    /* Current firmware info */
    uint8_t  current_version_major;
    uint8_t  current_version_minor;
    uint8_t  current_version_patch;
    uint8_t  reserved1;
    
    /* Previous firmware info (for rollback) */
    uint8_t  previous_version_major;
    uint8_t  previous_version_minor;
    uint8_t  previous_version_patch;
    uint8_t  reserved2;
    
    /* OTA tracking */
    uint8_t  staged_version_major;        // Version staged for update
    uint8_t  staged_version_minor;
    uint8_t  staged_version_patch;
    uint8_t  max_boot_attempts;           // Max boots before rollback (default: 3)
    
    /* Timestamps (tick counts, for debugging) */
    uint32_t last_ota_timestamp;          // When OTA was applied
    uint32_t last_confirm_timestamp;      // When firmware was confirmed
    
    /* Integrity */
    uint32_t crc32;                        // CRC of all above fields
} agsys_boot_info_t;  // 32 bytes total

_Static_assert(sizeof(agsys_boot_info_t) == 32, "boot_info must be 32 bytes");
```

---

## Boot Flow

### Normal Boot (No OTA Pending)

```
┌─────────────┐
│   RESET     │
└──────┬──────┘
       ▼
┌─────────────┐
│    MBR      │ Nordic Master Boot Record
└──────┬──────┘
       ▼
┌─────────────┐     ┌─────────────────────────┐
│ Bootloader  │────▶│ Read FRAM boot_info     │
└──────┬──────┘     └─────────────────────────┘
       ▼
┌─────────────────────────┐
│ boot_state == NORMAL?   │
└──────┬──────────────────┘
       │ YES
       ▼
┌─────────────┐
│ Jump to     │
│ Application │
└─────────────┘
```

**Time:** < 10ms added to boot

### Boot After OTA Apply (Pending Confirmation)

```
┌─────────────┐
│   RESET     │ (after OTA module applied new firmware and rebooted)
└──────┬──────┘
       ▼
┌─────────────┐
│    MBR      │
└──────┬──────┘
       ▼
┌─────────────┐     ┌─────────────────────────┐
│ Bootloader  │────▶│ Read FRAM boot_info     │
└──────┬──────┘     └─────────────────────────┘
       ▼
┌─────────────────────────┐
│ boot_state ==           │
│ OTA_PENDING?            │
└──────┬──────────────────┘
       │ YES
       ▼
┌─────────────────────────┐
│ Increment boot_count    │
│ Write to FRAM           │
└──────┬──────────────────┘
       ▼
┌─────────────────────────┐
│ boot_count >            │
│ max_boot_attempts?      │
└──────┬──────────────────┘
       │ NO
       ▼
┌─────────────────────────┐
│ Set boot_reason =       │
│ OTA_REBOOT              │
└──────┬──────────────────┘
       ▼
┌─────────────┐
│ Jump to     │
│ Application │ (new firmware gets a chance to run)
└─────────────┘
       │
       ▼
┌─────────────────────────┐
│ Application runs for    │
│ N minutes successfully  │
└──────┬──────────────────┘
       ▼
┌─────────────────────────┐
│ Application calls       │
│ agsys_boot_confirm()    │
└──────┬──────────────────┘
       ▼
┌─────────────────────────┐
│ boot_state = CONFIRMED  │
│ boot_count = 0          │
│ Backup new firmware     │
└─────────────────────────┘
```

### Boot After Failed OTA (Rollback)

```
┌─────────────┐
│   RESET     │ (watchdog, crash, or power cycle)
└──────┬──────┘
       ▼
┌─────────────┐
│    MBR      │
└──────┬──────┘
       ▼
┌─────────────┐     ┌─────────────────────────┐
│ Bootloader  │────▶│ Read FRAM boot_info     │
└──────┬──────┘     └─────────────────────────┘
       ▼
┌─────────────────────────┐
│ boot_state ==           │
│ OTA_PENDING?            │
└──────┬──────────────────┘
       │ YES
       ▼
┌─────────────────────────┐
│ Increment boot_count    │
└──────┬──────────────────┘
       ▼
┌─────────────────────────────────┐
│ boot_count > max_boot_attempts? │
└──────┬──────────────────────────┘
       │ YES (e.g., boot_count = 4, max = 3)
       ▼
┌─────────────────────────┐
│ *** ROLLBACK ***        │
│                         │
│ 1. Read backup from     │
│    external flash       │
│ 2. Verify CRC           │
│ 3. Erase app region     │
│ 4. Write backup to      │
│    internal flash       │
│ 5. Verify written data  │
└──────┬──────────────────┘
       ▼
┌─────────────────────────┐
│ Update FRAM:            │
│ - boot_state = ROLLBACK │
│ - boot_reason = ROLLBACK│
│ - boot_count = 0        │
│ - Swap current/previous │
│   versions              │
└──────┬──────────────────┘
       ▼
┌─────────────┐
│ Jump to     │
│ Application │ (restored previous firmware)
└─────────────┘
```

---

## Failure Scenarios and Recovery

### Scenario 1: New Firmware Crashes Immediately

**Situation:** OTA applied, device reboots, new firmware crashes before reaching main().

**Detection:** Watchdog fires (or repeated power cycles).

**Recovery:**
1. Boot 1: boot_count = 1, crash
2. Boot 2: boot_count = 2, crash
3. Boot 3: boot_count = 3, crash
4. Boot 4: boot_count = 4 > max(3), **ROLLBACK**

**Result:** Previous firmware restored, device operational.

### Scenario 2: New Firmware Runs But Has Bug

**Situation:** New firmware boots, runs for a while, but has a bug causing watchdog reset.

**Detection:** boot_count keeps incrementing without confirmation.

**Recovery:** Same as Scenario 1 - after max_boot_attempts, rollback occurs.

**Note:** Application should call `agsys_boot_confirm()` only after stable operation (e.g., 5 minutes uptime, successful sensor read, successful LoRa communication).

### Scenario 3: New Firmware Works Perfectly

**Situation:** OTA applied, new firmware boots and runs correctly.

**Flow:**
1. Boot 1: boot_count = 1, firmware runs
2. After 5 minutes: Application calls `agsys_boot_confirm()`
3. boot_state = CONFIRMED, boot_count = 0
4. Backup updated with new firmware

**Result:** New firmware is now the "known good" version.

### Scenario 4: Power Loss During OTA Apply

**Situation:** Power lost while OTA module is writing to internal flash.

**Detection:** Application region may be corrupted. CRC check fails.

**Recovery:**
1. Bootloader checks application CRC before jumping
2. If CRC fails and boot_state == OTA_PENDING: Rollback immediately
3. If CRC fails and boot_state == NORMAL: Attempt rollback (last resort)

### Scenario 5: Power Loss During Rollback

**Situation:** Power lost while bootloader is restoring backup.

**Detection:** Application CRC fails on next boot.

**Recovery:**
1. Bootloader detects corrupted application
2. Attempts rollback again from external flash
3. External flash backup should still be intact

**Mitigation:** Rollback is idempotent - can be safely repeated.

### Scenario 6: Corrupted Backup in External Flash

**Situation:** External flash backup is corrupted (bit rot, failed write).

**Detection:** Backup CRC check fails during rollback attempt.

**Recovery:**
1. Bootloader cannot rollback
2. If current application CRC is valid: Boot it anyway (better than nothing)
3. If current application CRC invalid: **Device is bricked** - requires manual reflash

**Mitigation:** 
- Verify backup CRC after writing
- Periodic backup integrity checks by application

### Scenario 7: FRAM Corruption

**Situation:** FRAM boot_info is corrupted.

**Detection:** Magic number or CRC invalid.

**Recovery:**
1. Bootloader initializes boot_info to defaults
2. boot_state = NORMAL
3. Boots current application (assumes it's good)

**Mitigation:** FRAM is highly reliable (no wear-out), corruption is rare.

---

## Bootloader Implementation Details

### Entry Conditions

Bootloader runs on every reset. MBR jumps to bootloader based on:
- Bootloader start address stored in MBR settings
- We configure MBR to always jump to bootloader first

### Hardware Access

Bootloader needs minimal hardware access:
- **FRAM (SPI):** Read/write boot_info
- **External Flash (SPI):** Read backup firmware
- **Internal Flash:** Write application region (via NVMC)
- **GPIO:** Optional - LED for rollback indication

**No SoftDevice calls** - bootloader runs before SoftDevice is initialized.

### SPI Configuration

Bootloader must configure SPI independently (no SDK dependencies):
```c
// Direct register access for SPI
// FRAM: SPI @ 8MHz, Mode 0
// External Flash: SPI @ 8MHz, Mode 0
// Share SPI bus, different CS pins
```

### Flash Writing

Use nRF52 NVMC (Non-Volatile Memory Controller) directly:
```c
// Erase page (4KB)
NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
NRF_NVMC->ERASEPAGE = page_address;
while (!NRF_NVMC->READY);

// Write word
NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
*(uint32_t *)address = data;
while (!NRF_NVMC->READY);

NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
```

### Size Budget

Target: **24KB** for bootloader

| Component | Estimated Size |
|-----------|----------------|
| Startup code | 1KB |
| FRAM driver (minimal) | 2KB |
| External flash driver (minimal) | 3KB |
| SPI driver (minimal) | 2KB |
| Boot logic | 4KB |
| Flash copy routine | 2KB |
| AES-128-CTR decryption | 3KB |
| SHA-256 (key derivation) | 2KB |
| CRC32 | 1KB |
| Error handling | 2KB |
| Padding/alignment | 2KB |
| **Total** | **24KB** |

> **Note:** Bootloader must implement AES-128-CTR decryption to restore encrypted backups. 
> Key derivation uses SHA-256(SECRET_SALT || DEVICE_ID)[0:16] - same as application.

### No RTOS

Bootloader runs bare-metal. No FreeRTOS, no interrupts (except NMI for flash operations).

---

## Application Integration

### OTA Module Changes

When OTA module is ready to apply new firmware:

```c
// 1. Stage firmware to external flash (already done)
// 2. Verify staged firmware CRC

// 3. Update boot_info for pending OTA
agsys_boot_info_t info;
agsys_boot_info_read(&info);
info.boot_state = AGSYS_BOOT_STATE_OTA_PENDING;
info.staged_version_major = new_major;
info.staged_version_minor = new_minor;
info.staged_version_patch = new_patch;
info.boot_count = 0;
info.last_ota_timestamp = xTaskGetTickCount();
agsys_boot_info_write(&info);

// 4. Copy staged firmware to application region
//    (This is done by application, not bootloader, for speed)
agsys_ota_apply_staged_firmware();

// 5. Reboot
NVIC_SystemReset();
```

### Confirmation Logic

Application should confirm OTA after stable operation:

```c
void app_main_loop(void)
{
    static bool ota_confirmed = false;
    static TickType_t boot_time = 0;
    
    if (boot_time == 0) {
        boot_time = xTaskGetTickCount();
    }
    
    // Confirm OTA after 5 minutes of stable operation
    if (!ota_confirmed) {
        agsys_boot_info_t info;
        agsys_boot_info_read(&info);
        
        if (info.boot_state == AGSYS_BOOT_STATE_OTA_PENDING) {
            TickType_t uptime = xTaskGetTickCount() - boot_time;
            
            // Require 5 minutes uptime AND successful operation
            if (uptime > pdMS_TO_TICKS(5 * 60 * 1000) && 
                app_is_operating_normally()) {
                
                agsys_boot_confirm();
                ota_confirmed = true;
            }
        } else {
            ota_confirmed = true;  // Not pending, nothing to confirm
        }
    }
}

bool app_is_operating_normally(void)
{
    // Device-specific checks:
    // - Sensor readings valid?
    // - LoRa communication working?
    // - No repeated errors?
    return true;
}
```

### Confirmation Criteria

What constitutes "stable operation" varies by device:

| Device | Confirmation Criteria |
|--------|----------------------|
| Soil Moisture | 5 min uptime + 1 successful sensor read + 1 successful LoRa TX |
| Valve Controller | 5 min uptime + CAN bus operational + 1 successful LoRa TX |
| Water Meter | 5 min uptime + ADC operational + display working |

### Boot Reason Reporting

Application can read boot reason for VERSION_REPORT:

```c
agsys_boot_info_t info;
agsys_boot_info_read(&info);

if (info.boot_reason == AGSYS_BOOT_REASON_ROLLBACK) {
    // Report rollback to property controller
    send_version_report(info.current_version, BOOT_REASON_ROLLBACK);
}
```

---

## Firmware Header

Every application firmware includes a header at a known offset for validation:

```c
#define AGSYS_FW_HEADER_MAGIC    0x41475359  // "AGSY"
#define AGSYS_FW_HEADER_OFFSET   0x200       // After vector table

typedef struct {
    uint32_t magic;              // AGSYS_FW_HEADER_MAGIC
    uint32_t header_version;     // Header structure version
    uint8_t  device_type;        // SOIL_MOISTURE, VALVE_CTRL, WATER_METER
    uint8_t  hw_revision_min;    // Minimum hardware revision
    uint8_t  hw_revision_max;    // Maximum hardware revision
    uint8_t  reserved1;
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;
    uint8_t  reserved2;
    uint32_t firmware_size;      // Size of firmware (excluding header)
    uint32_t firmware_crc;       // CRC32 of firmware
    uint32_t build_timestamp;    // Unix timestamp of build
    char     build_id[16];       // Git hash or build identifier
    uint32_t header_crc;         // CRC32 of header (excluding this field)
} agsys_fw_header_t;  // 48 bytes

_Static_assert(sizeof(agsys_fw_header_t) == 48, "fw_header must be 48 bytes");
```

### Header Validation

Bootloader validates header before jumping to application:

```c
bool validate_application(void)
{
    agsys_fw_header_t *header = (agsys_fw_header_t *)(APP_START + AGSYS_FW_HEADER_OFFSET);
    
    // Check magic
    if (header->magic != AGSYS_FW_HEADER_MAGIC) {
        return false;
    }
    
    // Check header CRC
    uint32_t calc_crc = crc32(header, sizeof(*header) - 4);
    if (calc_crc != header->header_crc) {
        return false;
    }
    
    // Check firmware CRC
    uint32_t fw_crc = crc32((void *)APP_START, header->firmware_size);
    if (fw_crc != header->firmware_crc) {
        return false;
    }
    
    return true;
}
```

---

## External Flash Layout

```
External Flash (2MB W25Q16):
┌─────────────────────────────────────────────────────────────────┐
│ Offset     │ Size   │ Purpose                                   │
├────────────┼────────┼───────────────────────────────────────────┤
│ 0x000000   │ 4KB    │ Backup Header (version, CRC, timestamp)   │
├────────────┼────────┼───────────────────────────────────────────┤
│ 0x001000   │ 508KB  │ Backup Firmware Image                     │
├────────────┼────────┼───────────────────────────────────────────┤
│ 0x080000   │ 4KB    │ Staged Header (version, CRC, timestamp)   │
├────────────┼────────┼───────────────────────────────────────────┤
│ 0x081000   │ 508KB  │ Staged Firmware Image (during OTA)        │
├────────────┼────────┼───────────────────────────────────────────┤
│ 0x100000   │ ~1MB   │ Application Data (logs, etc.)             │
└────────────┴────────┴───────────────────────────────────────────┘
```

### Backup Header

The bootloader uses the existing `agsys_backup_header_t` structure defined in `agsys_flash_backup.h`:

```c
// From agsys_flash_backup.h - 256 bytes total
typedef struct {
    uint32_t magic;                 // AGSYS_BACKUP_MAGIC (0x46574241 "FWBA")
    uint8_t  version;               // Header version
    uint8_t  active_slot;           // Currently active slot (0=A, 1=B)
    uint8_t  slot_a_status;         // Status of slot A
    uint8_t  slot_b_status;         // Status of slot B
    uint32_t slot_a_size;           // Firmware size in slot A
    uint32_t slot_b_size;           // Firmware size in slot B
    uint32_t slot_a_crc;            // CRC32 of slot A firmware
    uint32_t slot_b_crc;            // CRC32 of slot B firmware
    uint8_t  slot_a_version[4];     // Version in slot A (major.minor.patch.build)
    uint8_t  slot_b_version[4];     // Version in slot B
    uint8_t  expected_version[4];   // Expected version after OTA
    uint8_t  failed_version[4];     // Last version that failed validation
    uint32_t validation_start_ms;   // Tick when validation started
    uint8_t  rollback_count;        // Number of rollbacks performed
    uint8_t  reserved[207];         // Reserved for future use
    uint32_t header_crc;            // CRC32 of header
} __attribute__((packed)) agsys_backup_header_t;
```

The bootloader reads `active_slot` to determine which slot contains the backup firmware, then uses the corresponding `slot_X_size`, `slot_X_crc`, and `slot_X_version` fields.

---

## Rollback Procedure (Detailed)

```c
void perform_rollback(void)
{
    // 1. Read backup header from external flash
    agsys_backup_header_t backup_header;
    ext_flash_read(BACKUP_HEADER_ADDR, &backup_header, sizeof(backup_header));
    
    // 2. Validate backup header
    if (backup_header.magic != AGSYS_BACKUP_MAGIC) {
        // No valid backup - cannot rollback
        panic_no_backup();
        return;
    }
    
    uint32_t calc_crc = crc32(&backup_header, sizeof(backup_header) - 4);
    if (calc_crc != backup_header.header_crc) {
        panic_backup_corrupt();
        return;
    }
    
    // 3. Determine which slot has the backup
    uint8_t backup_slot = backup_header.active_slot;
    uint32_t fw_size, fw_crc;
    uint8_t *fw_version;
    uint32_t slot_addr;
    
    if (backup_slot == 0) {
        fw_size = backup_header.slot_a_size;
        fw_crc = backup_header.slot_a_crc;
        fw_version = backup_header.slot_a_version;
        slot_addr = AGSYS_BACKUP_SLOT_A_ADDR;
    } else {
        fw_size = backup_header.slot_b_size;
        fw_crc = backup_header.slot_b_crc;
        fw_version = backup_header.slot_b_version;
        slot_addr = AGSYS_BACKUP_SLOT_B_ADDR;
    }
    
    // 4. Erase application region in internal flash
    for (uint32_t addr = APP_START; addr < APP_END; addr += PAGE_SIZE) {
        flash_erase_page(addr);
    }
    
    // 5. Copy backup from external flash to internal flash (with decryption)
    uint8_t encrypted_buffer[256];
    uint8_t decrypted_buffer[256];
    uint32_t remaining = fw_size;
    uint32_t src_addr = slot_addr;
    uint32_t dst_addr = APP_START;
    
    // Initialize AES-CTR context with device-specific key
    aes_ctr_ctx_t aes_ctx;
    uint8_t key[16];
    derive_device_key(key);  // SHA-256(SECRET_SALT || DEVICE_ID)[0:16]
    aes_ctr_init(&aes_ctx, key);
    
    while (remaining > 0) {
        uint32_t chunk = (remaining > 256) ? 256 : remaining;
        ext_flash_read(src_addr, encrypted_buffer, chunk);
        
        // Decrypt chunk before writing to internal flash
        aes_ctr_decrypt(&aes_ctx, encrypted_buffer, decrypted_buffer, chunk);
        
        flash_write(dst_addr, decrypted_buffer, chunk);
        
        src_addr += chunk;
        dst_addr += chunk;
        remaining -= chunk;
        
        // Optional: Toggle LED to show progress
        led_toggle();
    }
    
    // 6. Verify written firmware
    uint32_t verify_crc = crc32((void *)APP_START, fw_size);
    if (verify_crc != fw_crc) {
        // Verification failed - try again or panic
        panic_verify_failed();
        return;
    }
    
    // 7. Update boot_info
    agsys_boot_info_t info;
    fram_read(BOOT_INFO_ADDR, &info, sizeof(info));
    
    // Swap versions
    info.previous_version_major = info.current_version_major;
    info.previous_version_minor = info.current_version_minor;
    info.previous_version_patch = info.current_version_patch;
    info.current_version_major = fw_version[0];
    info.current_version_minor = fw_version[1];
    info.current_version_patch = fw_version[2];
    
    info.boot_state = AGSYS_BOOT_STATE_ROLLBACK;
    info.boot_reason = AGSYS_BOOT_REASON_ROLLBACK;
    info.boot_count = 0;
    info.crc32 = crc32(&info, sizeof(info) - 4);
    
    fram_write(BOOT_INFO_ADDR, &info, sizeof(info));
    
    // 8. Increment rollback count in backup header
    backup_header.rollback_count++;
    backup_header.header_crc = crc32(&backup_header, sizeof(backup_header) - 4);
    ext_flash_write(BACKUP_HEADER_ADDR, &backup_header, sizeof(backup_header));
    
    // 9. Jump to restored application
    jump_to_application();
}
```

---

## LED Indication (Optional)

During rollback, bootloader can indicate status via LED:

| Pattern | Meaning |
|---------|---------|
| Solid ON | Rollback in progress |
| Fast blink | Erasing flash |
| Slow blink | Copying firmware |
| OFF | Complete, jumping to app |
| SOS pattern | Rollback failed, device bricked |

---

## Testing Strategy

### Unit Tests (Host)

- CRC32 calculation
- Boot info structure serialization
- State machine logic

### Integration Tests (Hardware)

1. **Normal boot** - Verify minimal delay added
2. **OTA success** - Apply OTA, confirm, verify backup updated
3. **OTA failure (immediate crash)** - Verify rollback after max_boot_attempts
4. **OTA failure (delayed crash)** - Verify rollback after repeated watchdog
5. **Power loss during apply** - Verify rollback on corrupted app
6. **Power loss during rollback** - Verify rollback completes on retry
7. **Corrupted backup** - Verify graceful handling
8. **Corrupted FRAM** - Verify defaults and recovery

### Stress Tests

- Repeated OTA cycles (100+)
- Power cycling during various stages
- Watchdog timing verification

---

## Security Considerations

### Firmware Signing (Future)

Currently not implemented. Future enhancement:
- Sign firmware with private key
- Bootloader verifies signature before applying
- Prevents unauthorized firmware

### Bootloader Protection

- Enable APPROTECT to prevent debugger access in production
- Bootloader region marked as write-protected

### Rollback Attacks

Risk: Attacker could force rollback to older vulnerable firmware.

Mitigation (future):
- Minimum version enforcement
- Anti-rollback counter in FRAM

---

## Implementation Phases

### Phase 1: Minimal Bootloader
- Read FRAM boot_info
- Check boot_state and boot_count
- Jump to application or panic
- No rollback yet

### Phase 2: Rollback Support
- External flash read
- Internal flash erase/write
- Full rollback procedure
- LED indication

### Phase 3: Application Integration
- agsys_boot_info library
- OTA module integration
- Confirmation logic in all devices

### Phase 4: Hardening
- Extensive testing
- Edge case handling
- Security features

---

## File Structure

```
agsys-control/devices/
├── recovery-loader/              # Stage 0.5 - FROZEN after manufacturing
│   ├── src/
│   │   ├── main.c                # Recovery loader entry (~200 lines)
│   │   ├── spi_minimal.c         # Minimal SPI driver
│   │   ├── fram_minimal.c        # FRAM read only
│   │   ├── ext_flash_minimal.c   # External flash read only
│   │   ├── int_flash.c           # Internal flash erase/write
│   │   ├── crc32.c               # CRC calculation
│   │   └── startup.s             # Startup assembly
│   ├── include/
│   │   └── recovery_loader.h     # Constants and addresses
│   ├── linker/
│   │   └── recovery_loader.ld    # Linker script (place at 0x70000)
│   └── Makefile
│
├── bootloader/                   # Main bootloader - UPDATABLE
│   ├── src/
│   │   ├── main.c                # Bootloader entry point
│   │   ├── boot_logic.c          # State machine, decision logic
│   │   ├── fram_driver.c         # FRAM SPI driver
│   │   ├── ext_flash_driver.c    # External flash driver
│   │   ├── int_flash.c           # Internal flash operations
│   │   ├── aes_ctr.c             # AES-128-CTR decryption
│   │   ├── sha256.c              # SHA-256 for key derivation
│   │   ├── crc32.c               # CRC calculation
│   │   └── startup.s             # Startup assembly
│   ├── include/
│   │   ├── boot_info.h           # Shared with application
│   │   ├── fw_header.h           # Shared with application
│   │   └── bootloader.h          # Internal definitions
│   ├── linker/
│   │   └── bootloader.ld         # Linker script (place at 0x72000)
│   └── Makefile
│
├── freertos-common/
│   ├── include/
│   │   ├── agsys_boot_info.h     # Application-side boot_info API
│   │   └── agsys_fw_header.h     # Firmware header definitions
│   └── src/
│       └── agsys_boot_info.c     # Application-side implementation
```

---

## Bootloader Update Process

Bootloader updates are rare but necessary for bug fixes. The Recovery Loader provides a safety net.

### Update Flow

```
1. Backend pushes bootloader update to property controller
2. Property controller pushes to device via LoRa (or BLE)
3. Device stages new bootloader in external flash (0x100000)
4. Device verifies staged bootloader (CRC, signature, version)
5. Device copies CURRENT bootloader to Bootloader Backup region
6. Device updates expected CRC in FRAM
7. Device erases bootloader region (0x72000-0x76000)
8. Device writes new bootloader from staging
9. Device verifies new bootloader CRC matches FRAM
10. Device reboots

On next boot:
- Recovery Loader checks bootloader CRC against FRAM
- If VALID → jump to bootloader → normal operation
- If INVALID → restore from Bootloader Backup → update FRAM CRC → jump
```

### Failure Recovery

| Failure Point | Recovery |
|---------------|----------|
| Power loss during step 5 | Old bootloader still intact, retry |
| Power loss during step 7-8 | Recovery Loader restores from backup |
| New bootloader has bug | Manual rollback via BLE or wait for fix |
| Backup corrupted | Recovery Loader can't help - device bricked |

### Staged Rollout for Bootloader Updates

Due to the critical nature, bootloader updates use **extremely cautious** rollout:

| Stage | Devices | Wait Period | Criteria to Proceed |
|-------|---------|-------------|---------------------|
| Internal | 5-10 | 2 weeks | Zero issues |
| Canary | 0.1% (~100 devices) | 2 weeks | Zero issues |
| Early | 1% (~1,000 devices) | 2 weeks | < 0.01% issues |
| Slow | 10% (~10,000 devices) | 1 month | < 0.01% issues |
| General | 100% | - | - |

**Total time:** ~3 months from internal to general availability

### Bootloader Version Tracking

```c
// Added to FRAM boot_info structure
typedef struct {
    // ... existing fields ...
    
    // Bootloader tracking (at offset 0x0030)
    uint32_t bootloader_crc;          // Expected CRC for Recovery Loader
    uint8_t  bootloader_version[4];   // Current bootloader version
    uint8_t  bootloader_prev_version[4]; // Previous bootloader version
} agsys_boot_info_extended_t;
```

---

## Resolved Questions

1. **Dual backup slots?** - No. Flash space constraints only allow one backup image. The existing `agsys_flash_backup` module already uses a single active slot model.

2. **Bootloader updates?** - **RESOLVED.** Use Recovery Loader (frozen) + Bootloader Backup in external flash. Recovery Loader restores bootloader if CRC check fails. See "Bootloader Update Process" section above.

3. **Factory reset?** - Factory reset should reset **configuration/options to defaults**, NOT revert to old firmware. Firmware version and configuration are separate concerns.
   > **TODO:** Define factory defaults for all device options/configurations.

4. **Encryption?** - Already implemented. The existing `agsys_flash_backup` module encrypts backups with **AES-128-CTR** using a device-specific key derived from `SHA-256(SECRET_SALT || DEVICE_ID)[0:16]`. The bootloader must implement the same decryption to restore backups.

---

## Appendix: Memory Map Summary

### nRF52832 (512KB Flash, 64KB RAM) - Updated with Recovery Loader

| Region | Start | End | Size | Purpose |
|--------|-------|-----|------|---------|
| MBR | 0x00000000 | 0x00001000 | 4KB | Master Boot Record - FROZEN |
| SoftDevice | 0x00001000 | 0x00026000 | 148KB | BLE Stack |
| Application | 0x00026000 | 0x00070000 | 296KB | Main Firmware - UPDATABLE |
| Recovery Loader | 0x00070000 | 0x00072000 | 8KB | Stage 0.5 - FROZEN |
| Bootloader | 0x00072000 | 0x00076000 | 16KB | Custom Bootloader - UPDATABLE |
| BL Settings | 0x00076000 | 0x00077000 | 4KB | Bootloader Settings |
| MBR Params | 0x00077000 | 0x00078000 | 4KB | MBR Parameters |
| App Data | 0x00078000 | 0x00080000 | 32KB | Application Settings |

### FRAM (128KB MB85RS1MT) - Growth-Buffered Layout

| Region | Start | End | Size | Purpose |
|--------|-------|-----|------|---------|
| Layout Header | 0x00000 | 0x00010 | 16B | **FROZEN** - magic, version, CRC |
| Boot Info | 0x00010 | 0x00110 | 256B | Boot state, versions, flags |
| [Growth Buffer] | 0x00110 | 0x00200 | 240B | Reserved for Boot Info expansion |
| Bootloader Info | 0x00200 | 0x00280 | 128B | Bootloader CRC for Recovery Loader |
| [Growth Buffer] | 0x00280 | 0x00300 | 128B | Reserved for BL Info expansion |
| Device Config | 0x00300 | 0x00700 | 1KB | Settings synced from cloud |
| [Growth Buffer] | 0x00700 | 0x00B00 | 1KB | Reserved for Config expansion |
| Calibration | 0x00B00 | 0x00F00 | 1KB | Sensor calibration data |
| [Growth Buffer] | 0x00F00 | 0x01300 | 1KB | Reserved for Calibration expansion |
| App Data | 0x01300 | 0x03300 | 8KB | Device-specific runtime data |
| [Growth Buffer] | 0x03300 | 0x05300 | 8KB | Reserved for App Data expansion |
| Ring Buffer Log | 0x05300 | 0x09300 | 16KB | Runtime logs (~128 entries @ 128B) |
| [Growth Buffer] | 0x09300 | 0x0D300 | 16KB | Reserved for Log expansion |
| Future Use | 0x0D300 | 0x20000 | ~76KB | Unallocated for new features |

**Layout Header Structure (16 bytes, FROZEN FOREVER):**
```c
typedef struct __attribute__((packed)) {
    uint32_t magic;           // 0x41475359 ("AGSY")
    uint8_t  layout_version;  // Increment on any region change
    uint8_t  device_type;     // 1=Soil, 2=Valve, 3=Meter, 4=Actuator
    uint16_t reserved1;
    uint32_t crc32;           // CRC32 of bytes 0-7
    uint32_t reserved2;
} agsys_fram_layout_header_t;
```

> **Rationale:** Growth buffers between regions allow expanding without shifting subsequent
> data, minimizing layout version changes. Layout Header at 0x0000 is FROZEN FOREVER.
> When firmware detects version mismatch, it runs migration code before using data.

### External Flash (2MB W25Q16) - A/B Slots for Future MCUs

| Region | Start | End | Size | Purpose |
|--------|-------|-----|------|---------|
| Slot A Header | 0x000000 | 0x001000 | 4KB | Backup metadata, CRC, version |
| Slot A Firmware | 0x001000 | 0x0ED000 | 944KB | Application backup (encrypted) |
| Slot B Header | 0x0ED000 | 0x0EE000 | 4KB | OTA staging metadata |
| Slot B Firmware | 0x0EE000 | 0x1DA000 | 944KB | OTA staging area |
| Bootloader Backup | 0x1DA000 | 0x1DE000 | 16KB | Bootloader backup for Recovery Loader |
| Reserved | 0x1DE000 | 0x200000 | 136KB | Future expansion |

> **Rationale:** A/B slots sized at 944KB to support future larger MCUs (e.g., nRF52840 
> with ~800KB usable flash). Current nRF52832 firmware (~300KB) fits with room to grow.
> This future-proofs the external flash layout for 10+ year device lifetime.
