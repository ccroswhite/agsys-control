# AgSys Custom Bootloader

Brick-proof bootloader for nRF52-based AgSys devices with automatic firmware rollback.

## Features

- **Automatic rollback** - Restores previous firmware if new firmware fails to boot
- **Boot count tracking** - Triggers rollback after 3 failed boot attempts
- **FRAM logging** - Records boot events and errors for diagnostics
- **Dual backup slots** - A/B firmware slots in external flash
- **Minimal footprint** - ~5KB code size, fits in 16KB allocation

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    INTERNAL FLASH (512KB)                        │
├─────────────────────────────────────────────────────────────────┤
│ 0x00000000 │ MBR (4KB)           │ Nordic - FROZEN              │
│ 0x00001000 │ SoftDevice S132     │ BLE Stack (~148KB)           │
│ 0x00026000 │ Application         │ Main firmware - UPDATABLE    │
│ 0x00070000 │ Recovery Loader     │ Stage 0.5 - FROZEN           │
│ 0x00072000 │ Bootloader          │ This code - UPDATABLE        │
│ 0x00076000 │ Bootloader Settings │ MBR settings page            │
└─────────────────────────────────────────────────────────────────┘
```

## Boot Flow

```
Power On
    │
    ▼
┌─────────────────┐
│ Init Hardware   │ (SPI, GPIO)
│ Init Logging    │
│ Init Ext Flash  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Read boot_info  │ from FRAM
│ from FRAM       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐     Valid app?     ┌─────────────────┐
│ Validate App    │────── No ─────────▶│ Rollback from   │
│ (magic, CRC)    │                    │ External Flash  │
└────────┬────────┘                    └────────┬────────┘
         │ Yes                                  │
         ▼                                      │
┌─────────────────┐                             │
│ Check boot_state│                             │
└────────┬────────┘                             │
         │                                      │
    ┌────┴────┐                                 │
    │         │                                 │
 NORMAL    OTA_PENDING                          │
    │         │                                 │
    │    boot_count++                           │
    │         │                                 │
    │    > max_attempts? ───── Yes ─────────────┘
    │         │ No
    │         │
    └────┬────┘
         │
         ▼
┌─────────────────┐
│ Jump to App     │
└─────────────────┘
```

## Files

```
bootloader/
├── include/
│   ├── bl_hal.h          # Hardware abstraction (bare-metal)
│   ├── bl_flash.h        # External flash driver
│   └── bl_log.h          # FRAM logging
├── src/
│   ├── startup.c         # Vector table, init
│   ├── main.c            # Entry point and boot logic
│   ├── bl_hal_nrf52.c    # nRF52 bare-metal HAL
│   ├── bl_crc32.c        # CRC32 calculation
│   ├── bl_flash.c        # External flash driver (W25Q series)
│   └── bl_log.c          # FRAM logging implementation
├── linker/
│   └── bootloader.ld     # Linker script (place at 0x72000)
└── Makefile
```

## Building

```bash
# Build for test hardware (8KB FRAM with 2-byte addressing)
make -C devices/bootloader

# Build for production (128KB FRAM with 3-byte addressing)
make -C devices/bootloader CFLAGS+="-DBL_FRAM_ADDR_BYTES=3"
```

## Testing on Adafruit Feather nRF52832

### Hardware Required

| Item | Part | Notes |
|------|------|-------|
| Adafruit Feather nRF52832 | [Product 3406](https://www.adafruit.com/product/3406) | Main board |
| SPI FRAM Breakout | [Product 1897](https://www.adafruit.com/product/1897) | MB85RS64V 8KB |
| SPI Flash Breakout | [Product 5634](https://www.adafruit.com/product/5634) | W25Q128 16MB |
| Breadboard | - | For connections |
| Jumper wires | - | 10+ wires |

### Wiring Diagram

```
Adafruit Feather nRF52832          FRAM Breakout (MB85RS64V)
┌─────────────────────┐            ┌─────────────────────┐
│                     │            │                     │
│  3.3V ──────────────┼────────────┼── VCC               │
│  GND ───────────────┼────────────┼── GND               │
│  SCK (P0.12) ───────┼────────────┼── SCK               │
│  MOSI (P0.13) ──────┼────────────┼── MOSI              │
│  MISO (P0.14) ──────┼────────────┼── MISO              │
│  A0 (P0.02) ────────┼────────────┼── CS                │
│                     │            │                     │
└─────────────────────┘            └─────────────────────┘

Adafruit Feather nRF52832          Flash Breakout (W25Q128)
┌─────────────────────┐            ┌─────────────────────┐
│                     │            │                     │
│  3.3V ──────────────┼────────────┼── VCC               │
│  GND ───────────────┼────────────┼── GND               │
│  SCK (P0.12) ───────┼────────────┼── CLK               │
│  MOSI (P0.13) ──────┼────────────┼── DI                │
│  MISO (P0.14) ──────┼────────────┼── DO                │
│  A1 (P0.03) ────────┼────────────┼── CS                │
│                     │            │                     │
└─────────────────────┘            └─────────────────────┘
```

### Pin Mapping for Feather

Add these defines to the Makefile or compile command:

```makefile
# Adafruit Feather nRF52832 pin configuration
CFLAGS += -DBL_PIN_SPI_SCK=12
CFLAGS += -DBL_PIN_SPI_MOSI=13
CFLAGS += -DBL_PIN_SPI_MISO=14
CFLAGS += -DBL_PIN_FRAM_CS=2      # A0
CFLAGS += -DBL_PIN_FLASH_CS=3     # A1
CFLAGS += -DBL_PIN_LED=17         # Built-in red LED
```

### Build for Feather

```bash
make -C devices/bootloader \
    CFLAGS+="-DBL_PIN_SPI_SCK=12 -DBL_PIN_SPI_MOSI=13 -DBL_PIN_SPI_MISO=14" \
    CFLAGS+="-DBL_PIN_FRAM_CS=2 -DBL_PIN_FLASH_CS=3 -DBL_PIN_LED=17"
```

### Flashing

The Feather has a built-in UF2 bootloader, but our bootloader needs to be flashed via SWD:

```bash
# Using nrfjprog (requires J-Link)
nrfjprog --program build/agsys_bootloader.hex --sectorerase --verify
nrfjprog --reset

# Using OpenOCD (requires CMSIS-DAP or J-Link)
openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \
    -c "program build/agsys_bootloader.hex verify reset exit"
```

## FRAM Log Format

The bootloader logs events to FRAM for post-mortem analysis:

| Event Type | Value | Description |
|------------|-------|-------------|
| BOOT_START | 0x01 | Bootloader started |
| BOOT_SUCCESS | 0x02 | Successfully jumped to app |
| BOOT_FAIL | 0x03 | Boot failed |
| ROLLBACK_START | 0x10 | Starting rollback |
| ROLLBACK_SUCCESS | 0x11 | Rollback completed |
| ROLLBACK_FAIL | 0x12 | Rollback failed |
| APP_INVALID | 0x20 | Application validation failed |
| APP_CRC_FAIL | 0x21 | Application CRC mismatch |
| FRAM_ERROR | 0x30 | FRAM read/write error |
| FLASH_ERROR | 0x31 | External flash error |
| NVMC_ERROR | 0x32 | Internal flash write error |
| PANIC | 0xFF | Entered panic mode |

## External Flash Layout

```
W25Q128 (16MB) / W25Q16 (2MB)
┌─────────────────────────────────────────────────────────────────┐
│ 0x000000 │ Slot A Header (4KB)  │ Firmware metadata            │
│ 0x001000 │ Slot A Firmware      │ 944KB backup                 │
│ 0x0ED000 │ Slot B Header (4KB)  │ Firmware metadata            │
│ 0x0EE000 │ Slot B Firmware      │ 944KB staging                │
│ 0x1DA000 │ Bootloader Backup    │ 16KB (for recovery loader)   │
│ 0x1DE000 │ Reserved             │ Future use                   │
└─────────────────────────────────────────────────────────────────┘
```
