# FreeRTOS Development Environment Setup

**Target:** GCC + Make on macOS for Nordic nRF52 development

---

## Prerequisites

### 1. ARM GCC Toolchain

```bash
# Install via Homebrew
brew install --cask gcc-arm-embedded

# Verify installation
arm-none-eabi-gcc --version
# Should show: arm-none-eabi-gcc (GNU Arm Embedded Toolchain) 10.3.1 or similar
```

### 2. Nordic Command Line Tools

Download from: https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download

Or via Homebrew:
```bash
brew install --cask nordic-nrf-command-line-tools

# Verify installation
nrfjprog --version
mergehex --version
```

**Includes:**
- `nrfjprog` - Flash programming via J-Link
- `mergehex` - Merge hex files (app + SoftDevice)
- `nrfutil` - DFU package creation

### 3. J-Link Software (for Nordic DK boards)

Download from: https://www.segger.com/downloads/jlink/

Or the Nordic tools installer includes it.

---

## nRF5 SDK Setup

### Download SDK 17.1.0

```bash
# Create SDK directory
mkdir -p ~/nordic
cd ~/nordic

# Download SDK (or download manually from Nordic website)
curl -O https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/sdks/nrf5/binaries/nrf5_sdk_17.1.0_ddde560.zip

# Extract
unzip nrf5_sdk_17.1.0_ddde560.zip
mv nrf5_sdk_17.1.0_ddde560 nRF5_SDK_17.1.0

# Verify
ls ~/nordic/nRF5_SDK_17.1.0/
```

### Configure Toolchain Path

Edit `~/nordic/nRF5_SDK_17.1.0/components/toolchain/gcc/Makefile.posix`:

```makefile
GNU_INSTALL_ROOT ?= /opt/homebrew/bin/
GNU_VERSION ?= 10.3.1
GNU_PREFIX ?= arm-none-eabi
```

**Note:** Check your actual GCC version with `arm-none-eabi-gcc --version` and update accordingly.

### GCC 15 Compatibility Fix

GCC 15 has stricter `-Warray-bounds` warnings that cause SDK code to fail with `-Werror`. 
Add `-Wno-array-bounds` to CFLAGS in Makefiles:

```makefile
# Change this line in Makefile:
CFLAGS += -Wall -Werror
# To:
CFLAGS += -Wall -Werror -Wno-array-bounds
```

This is needed for most SDK examples with logging enabled.

---

## Verify Setup - Build a Blinky Example

```bash
cd ~/nordic/nRF5_SDK_17.1.0/examples/peripheral/blinky/pca10056/blank/armgcc

# Build
make

# Should produce:
# _build/nrf52840_xxaa.hex
# _build/nrf52840_xxaa.out
```

**Board mappings:**
- `pca10056` = nRF52840-DK
- `pca10040` = nRF52832-DK
- `pca10100` = nRF52833-DK (closest to nRF52820)

---

## Flash to Board (when DK arrives)

```bash
# Erase and program
nrfjprog -f nrf52 --eraseall
nrfjprog -f nrf52 --program _build/nrf52840_xxaa.hex --sectorerase
nrfjprog -f nrf52 --reset

# Or use the Makefile target
make flash
```

---

## FreeRTOS Examples in SDK

Key examples to study:

### 1. Basic FreeRTOS (no BLE)
```
examples/peripheral/blinky_freertos/pca10056/blank/armgcc/
```

### 2. BLE + FreeRTOS (Heart Rate example)
```
examples/ble_peripheral/ble_app_hrs_freertos/pca10056/s140/armgcc/
```

This is the most relevant - shows SoftDevice + FreeRTOS integration.

### 3. FreeRTOS with multiple tasks
```
examples/peripheral/twi_master_using_nrf_twi_mngr/pca10056/blank/armgcc/
```

---

## SoftDevice (BLE Stack)

SoftDevice hex files are in:
```
components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex  # nRF52840
components/softdevice/s132/hex/s132_nrf52_7.2.0_softdevice.hex  # nRF52832
components/softdevice/s112/hex/s112_nrf52_7.2.0_softdevice.hex  # nRF52810/820
```

Flash SoftDevice first, then application:
```bash
# Flash SoftDevice
nrfjprog -f nrf52 --program s140_nrf52_7.2.0_softdevice.hex --sectorerase

# Flash application
nrfjprog -f nrf52 --program _build/nrf52840_xxaa.hex --sectorerase
nrfjprog -f nrf52 --reset
```

---

## Directory Structure for AgSys FreeRTOS

Proposed structure in `agsys-control/`:

```
devices/
├── common/                    # Existing Arduino common lib
├── freertos-common/           # NEW: FreeRTOS common lib
│   ├── include/
│   │   ├── agsys_spi.h       # SPI manager with mutex
│   │   ├── agsys_crypto.h    # AES-GCM wrapper
│   │   ├── agsys_protocol.h  # Message structures (shared)
│   │   ├── agsys_ble.h       # BLE service definitions
│   │   └── agsys_fram.h      # FRAM driver
│   └── src/
│       ├── agsys_spi.c
│       ├── agsys_crypto.c
│       ├── agsys_ble.c
│       └── agsys_fram.c
├── valveactuator-freertos/    # NEW: FreeRTOS port
│   ├── config/
│   │   ├── FreeRTOSConfig.h
│   │   └── sdk_config.h
│   ├── src/
│   │   ├── main.c
│   │   ├── can_task.c
│   │   ├── valve_task.c
│   │   ├── ble_task.c
│   │   └── led_task.c
│   └── Makefile
├── watermeter-freertos/       # NEW: FreeRTOS port
│   └── ...
└── ...
```

---

## Quick Reference - Make Targets

Standard targets in SDK examples:

```bash
make                    # Build
make clean              # Clean build artifacts
make flash              # Flash via J-Link
make flash_softdevice   # Flash SoftDevice
make sdk_config         # Open SDK config GUI (requires Java)
make erase              # Erase chip
```

---

## Debugging with GDB

```bash
# Start GDB server (in one terminal)
JLinkGDBServer -device nRF52840_xxAA -if SWD -speed 4000

# Connect with GDB (in another terminal)
arm-none-eabi-gdb _build/nrf52840_xxaa.out
(gdb) target remote localhost:2331
(gdb) monitor reset
(gdb) load
(gdb) break main
(gdb) continue
```

---

## Next Steps

1. Install ARM GCC toolchain
2. Install Nordic command line tools
3. Download and configure nRF5 SDK
4. Build `blinky_freertos` example (no hardware needed)
5. Read FreeRTOS book chapters 1-4 (tasks, queues, semaphores)
6. When boards arrive: flash and test

---

*Created: January 10, 2026*
