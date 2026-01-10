# AgSys FreeRTOS Common Library

Shared code for all AgSys IoT devices running on Nordic nRF5 SDK + FreeRTOS.

## Components

| Module | Description |
|--------|-------------|
| `agsys_spi` | SPI bus manager with FreeRTOS mutex protection |
| `agsys_crypto` | AES-128-GCM encryption using nRF hardware crypto |
| `agsys_protocol` | AgSys LoRa message structures and encoding |
| `agsys_ble` | BLE service definitions and handlers |
| `agsys_fram` | FRAM driver (FM25V02) for persistent storage |
| `agsys_nvram` | NVRAM abstraction for settings/calibration |
| `agsys_debug` | Debug logging macros |

## Directory Structure

```
freertos-common/
├── include/
│   ├── agsys_spi.h
│   ├── agsys_crypto.h
│   ├── agsys_protocol.h
│   ├── agsys_ble.h
│   ├── agsys_fram.h
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
