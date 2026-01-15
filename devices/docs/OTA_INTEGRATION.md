# OTA Integration Guide

This document describes how to integrate OTA (Over-The-Air) firmware update support into AgSys devices. It covers the application header embedding, bootloader interaction, and the OTA confirmation mechanism.

## Overview

The OTA system consists of three main components:

1. **Application Header** - Embedded in firmware for bootloader validation
2. **Bootloader** - Validates firmware and manages rollback
3. **OTA Confirmation** - Application confirms successful boot to prevent rollback

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      External Flash                          │
│  ┌─────────────────┐  ┌─────────────────┐                   │
│  │  Slot A (1MB)   │  │  Slot B (1MB)   │                   │
│  │  Current FW     │  │  Backup FW      │                   │
│  └─────────────────┘  └─────────────────┘                   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                         FRAM                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │  Boot Info   │  │  OTA State   │  │  Boot Log    │       │
│  │  (32 bytes)  │  │  (32 bytes)  │  │  (ring buf)  │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    Internal Flash                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │  SoftDevice  │  │  App Header  │  │  Application │       │
│  │  (0x00000)   │  │  (.app_hdr)  │  │  (.text)     │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

## Application Header

### Purpose

The application header is a 48-byte structure embedded in the firmware binary. The bootloader reads this header to:

- Verify the firmware is valid (magic number, CRC)
- Check device type compatibility
- Check hardware revision compatibility
- Log firmware version for diagnostics

### Header Structure

```c
typedef struct {
    uint32_t magic;              // 0x59534741 ("AGSY")
    uint8_t  header_version;     // Header format version (currently 1)
    uint8_t  device_type;        // Device type enum
    uint8_t  hw_revision_min;    // Minimum hardware revision
    uint8_t  hw_revision_max;    // Maximum hardware revision
    uint8_t  fw_version_major;   // Firmware major version
    uint8_t  fw_version_minor;   // Firmware minor version
    uint8_t  fw_version_patch;   // Firmware patch version
    uint8_t  fw_flags;           // Firmware flags
    uint32_t fw_size;            // Total firmware size (patched by build)
    uint32_t fw_crc32;           // Firmware CRC32 (patched by build)
    uint32_t fw_load_addr;       // Load address in flash
    uint32_t build_timestamp;    // Unix timestamp of build
    char     build_id[16];       // Git hash or build identifier
    uint32_t header_crc32;       // Header CRC32 (patched by build)
} agsys_app_header_t;
```

### Device Types

```c
#define AGSYS_DEVICE_TYPE_UNKNOWN        0
#define AGSYS_DEVICE_TYPE_SOIL_MOISTURE  1
#define AGSYS_DEVICE_TYPE_VALVE_CONTROL  2
#define AGSYS_DEVICE_TYPE_VALVE_ACTUATOR 3
#define AGSYS_DEVICE_TYPE_WATER_METER    4
```

### Firmware Flags

```c
#define AGSYS_FW_FLAG_NONE           0x00
#define AGSYS_FW_FLAG_DEVELOPMENT    0x01  // Development build
#define AGSYS_FW_FLAG_BETA           0x02  // Beta release
#define AGSYS_FW_FLAG_PRODUCTION     0x04  // Production release
#define AGSYS_FW_FLAG_REQUIRES_RESET 0x10  // Requires factory reset after install
```

## Integration Steps

### 1. Create Device-Specific app_header.c

Create `src/app_header.c` in your device directory:

```c
#include "agsys_app_header.h"

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP 0
#endif

#ifndef BUILD_ID
#define BUILD_ID "dev"
#endif

#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0
#define FW_VERSION_PATCH    0

const agsys_app_header_t __attribute__((section(".app_header"), used))
g_app_header = {
    .magic              = AGSYS_APP_HEADER_MAGIC,
    .header_version     = AGSYS_APP_HEADER_VERSION,
    .device_type        = AGSYS_DEVICE_TYPE_YOUR_DEVICE,  // Change this
    .hw_revision_min    = 0,
    .hw_revision_max    = 255,
    .fw_version_major   = FW_VERSION_MAJOR,
    .fw_version_minor   = FW_VERSION_MINOR,
    .fw_version_patch   = FW_VERSION_PATCH,
    .fw_flags           = AGSYS_FW_FLAG_DEVELOPMENT,
    .fw_size            = 0xFFFFFFFF,       // Patched by post-build
    .fw_crc32           = 0xFFFFFFFF,       // Patched by post-build
    .fw_load_addr       = 0x00026000,       // After SoftDevice
    .build_timestamp    = BUILD_TIMESTAMP,
    .build_id           = BUILD_ID,
    .header_crc32       = 0xFFFFFFFF,       // Patched by post-build
};
```

### 2. Update Linker Script

Add the `.app_header` section to your linker script (e.g., `config/device_gcc_nrf52832.ld`):

```ld
SECTIONS
{
  /* Application header for bootloader validation */
  .app_header :
  {
    . = ALIGN(4);
    KEEP(*(.app_header))
  } > FLASH
} INSERT AFTER .text;
```

### 3. Update Makefile

Add the source files to your Makefile:

```makefile
SRC_FILES += \
  $(PROJ_DIR)/src/app_header.c \
  $(COMMON_DIR)/src/agsys_app_header.c \
```

Add the patch_header target (optional, for release builds):

```makefile
.PHONY: patch_header
patch_header: default
	@echo "Patching application header..."
	@python3 $(COMMON_DIR)/scripts/patch_app_header.py $(OUTPUT_DIRECTORY)/nrf52832_xxaa.bin
	@$(OBJCOPY) -I binary -O ihex $(OUTPUT_DIRECTORY)/nrf52832_xxaa.bin $(OUTPUT_DIRECTORY)/nrf52832_xxaa.hex
```

### 4. Add OTA Confirmation Call

In your main application startup (after successful initialization), call:

```c
#include "agsys_ota.h"

void main_task(void *arg)
{
    // Initialize hardware, peripherals, etc.
    
    // Confirm successful boot to prevent rollback
    agsys_ota_confirm();
    
    // Continue with normal operation...
}
```

## Build Process

### Development Builds

For development, the standard `make` command works:

```bash
make clean && make
```

The header will have placeholder CRCs (0xFFFFFFFF), which is fine for development.

### Release Builds

For release builds, use the `release` target which patches the header:

```bash
make release
```

This will:
1. Build the firmware
2. Run `patch_app_header.py` to calculate and patch:
   - `fw_size` - Total binary size
   - `fw_crc32` - CRC32 of entire firmware
   - `header_crc32` - CRC32 of header (excluding this field)
3. Regenerate the .hex file from the patched .bin

## Bootloader Behavior

### Boot Sequence

1. Bootloader starts, reads boot info from FRAM
2. Increments boot count
3. Checks boot state:
   - **NORMAL**: Validate app header, jump to app
   - **OTA_PENDING**: Copy new firmware, validate, jump to app
   - **ROLLBACK**: Restore backup from external flash
4. If boot count exceeds 3 without confirmation, trigger rollback

### Rollback Trigger

The bootloader will automatically rollback if:

- Boot count exceeds `AGSYS_BOOT_ATTEMPT_MAX` (3)
- Application header validation fails (bad magic or CRC)
- Device type mismatch
- Hardware revision incompatibility

### Confirmation Timeout

The application must call `agsys_ota_confirm()` within 60 seconds of boot. This:

1. Resets the boot count to 0
2. Sets boot state to NORMAL
3. Updates the backup in external flash (if new firmware)

## FRAM Memory Layout

```
Address     Size    Description
0x0000      32      Boot Info (state, version, boot count, CRC)
0x0060      32      OTA State (pending version, chunk progress)
0x0100      256     Boot Log (ring buffer)
```

## External Flash Layout

```
Address     Size    Description
0x000000    1MB     Slot A - Current firmware backup
0x100000    1MB     Slot B - OTA staging area
```

## Debugging

### Verify Header in Binary

Use `xxd` to check the header is present:

```bash
xxd _build/nrf52832_xxaa.bin | grep -A3 "AGSY"
```

Or use the patch script in verbose mode:

```bash
python3 ../freertos-common/scripts/patch_app_header.py _build/nrf52832_xxaa.bin
```

### Check Header Location

Use `objdump` to verify section placement:

```bash
arm-none-eabi-objdump -h _build/nrf52832_xxaa.out | grep app_header
```

### Runtime Header Access

```c
const agsys_app_header_t *hdr = agsys_app_header_get();
printf("Version: %d.%d.%d\n", 
       hdr->fw_version_major,
       hdr->fw_version_minor,
       hdr->fw_version_patch);
```

## Files Reference

| File | Description |
|------|-------------|
| `freertos-common/include/agsys_app_header.h` | Header structure and API |
| `freertos-common/src/agsys_app_header.c` | Runtime header access |
| `freertos-common/scripts/patch_app_header.py` | Post-build CRC patching |
| `freertos-common/include/agsys_ota.h` | OTA API |
| `freertos-common/src/agsys_ota.c` | OTA implementation |
| `freertos-common/include/agsys_memory_layout.h` | FRAM/Flash addresses |
| `bootloader/src/main.c` | Bootloader entry point |

## Checklist for New Devices

- [ ] Create `src/app_header.c` with correct device type
- [ ] Add `.app_header` section to linker script
- [ ] Add source files to Makefile
- [ ] Add `agsys_ota_confirm()` call to startup code
- [ ] Test build with `make clean && make`
- [ ] Verify header with `patch_app_header.py`
- [ ] Test OTA update flow (when hardware available)
