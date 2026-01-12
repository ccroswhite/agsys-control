# AgSys FreeRTOS Common Library

Shared code for all AgSys IoT devices running on Nordic nRF5 SDK + FreeRTOS.

## Components

| Module | Description |
|--------|-------------|
| `agsys_memory_layout` | **Shared memory layout** for FRAM and Flash (all devices) |
| `agsys_spi` | SPI bus manager with FreeRTOS mutex protection |
| `agsys_crypto` | AES-128-GCM encryption using nRF hardware crypto |
| `agsys_protocol` | AgSys LoRa message structures and encoding |
| `agsys_ble` | BLE service definitions and handlers |
| `agsys_fram` | FRAM driver (MB85RS1MT - 128KB) for persistent storage + logs |
| `agsys_flash` | Flash driver (W25Q16 - 2MB) for firmware backup/OTA |
| `agsys_nvram` | NVRAM abstraction for settings/calibration |
| `agsys_debug` | Debug logging macros |

## Shared Memory Layout

All devices use the same FRAM and Flash memory layout defined in `agsys_memory_layout.h`.
This ensures consistent data storage and enables safe firmware updates with layout migration.

**FRAM (MB85RS1MT - 128KB) with Growth Buffers:**
| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| Layout Header | 0x00000 | 16B | **FROZEN** - version, magic, CRC |
| Boot Info | 0x00010 | 256B + 240B growth | OTA state, versions |
| Bootloader Info | 0x00200 | 128B + 128B growth | Recovery Loader CRC |
| Device Config | 0x00300 | 1KB + 1KB growth | Settings from cloud |
| Calibration | 0x00B00 | 1KB + 1KB growth | Sensor calibration |
| App Data | 0x01300 | 8KB + 8KB growth | Device-specific data |
| Ring Buffer Log | 0x05300 | 16KB + 16KB growth | Runtime logs |
| Future Use | 0x0D300 | ~76KB | Unallocated |

**External Flash (W25Q16 - 2MB) A/B Slots:**
| Region | Address | Size | Purpose |
|--------|---------|------|---------|
| Slot A Header | 0x000000 | 4KB | Backup metadata |
| Slot A Firmware | 0x001000 | 944KB | Application backup |
| Slot B Header | 0x0ED000 | 4KB | OTA staging metadata |
| Slot B Firmware | 0x0EE000 | 944KB | OTA staging area |
| Bootloader Backup | 0x1DA000 | 16KB | Recovery source |
| Reserved | 0x1DE000 | 136KB | Future use |

## Directory Structure

```
freertos-common/
├── include/
│   ├── agsys_memory_layout.h  ← Shared memory layout (include first)
│   ├── agsys_spi.h
│   ├── agsys_crypto.h
│   ├── agsys_protocol.h
│   ├── agsys_ble.h
│   ├── agsys_fram.h
│   ├── agsys_flash.h
│   ├── agsys_nvram.h
│   └── agsys_debug.h
├── src/
│   ├── agsys_spi.c
│   ├── agsys_crypto.c
│   ├── agsys_protocol.c
│   ├── agsys_ble.c
│   ├── agsys_fram.c
│   └── agsys_nvram.c
├── config/
│   └── agsys_config_template.h
└── README.md
```

## Usage

Include in your project's Makefile:

```makefile
# Add to SRC_FILES
SRC_FILES += \
  $(AGSYS_COMMON)/src/agsys_spi.c \
  $(AGSYS_COMMON)/src/agsys_crypto.c \
  ...

# Add to INC_FOLDERS
INC_FOLDERS += \
  $(AGSYS_COMMON)/include
```

## Dependencies

- nRF5 SDK 17.1.0
- FreeRTOS 10.4.6 (included in SDK)
- SoftDevice S140/S132/S112 (for BLE)

## Configuration

Copy `config/agsys_config_template.h` to your project as `agsys_config.h` and customize.
