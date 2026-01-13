# Session Notes - January 12, 2026 - Bootloader Implementation

## Summary

Implemented a complete custom bootloader for nRF52 devices with automatic firmware rollback support. The bootloader is ready for hardware testing once the ordered parts arrive.

## Parts Ordered

| Item | Adafruit Product | Purpose | ETA |
|------|------------------|---------|-----|
| SPI FRAM Breakout | [#1897](https://www.adafruit.com/product/1897) | MB85RS64V 8KB - boot_info storage | ~2 days |
| SPI Flash Breakout | [#5634](https://www.adafruit.com/product/5634) | W25Q128 16MB - firmware backup | ~2 days |

## Hardware Available

- 2x Adafruit Feather nRF52832 boards (for testing)
- 4x 4Mbit FRAM devices (8TDFN package - no breakout board available)

## Bootloader Implementation Complete

### Features Implemented

1. **Boot info management** - Read/write boot state from FRAM
2. **Boot count tracking** - Increments on each boot attempt during OTA_PENDING state
3. **Automatic rollback** - Triggers after 3 failed boot attempts
4. **Application validation** - Checks magic number and CRC32
5. **External flash driver** - W25Q series support for firmware backup
6. **Firmware restore** - Copies backup from external flash to internal flash
7. **FRAM logging** - Ring buffer of boot events for diagnostics
8. **Configurable pins** - Compile-time configuration for different boards

### Code Size

```
   text    data     bss     dec     hex filename
   4844       0    1568    6412    190c build/agsys_bootloader.elf
```

**~5KB code** - well within 16KB allocation.

### Files Created

```
devices/bootloader/
├── README.md                    # Comprehensive docs with wiring guide
├── Makefile                     # Build system with pin overrides
├── include/
│   ├── bl_hal.h                 # Hardware abstraction layer
│   ├── bl_flash.h               # External flash API
│   └── bl_log.h                 # FRAM logging API
├── src/
│   ├── startup.c                # Vector table, .data/.bss init
│   ├── main.c                   # Boot logic, rollback, FRAM access
│   ├── bl_hal_nrf52.c           # Bare-metal GPIO, SPI, NVMC
│   ├── bl_crc32.c               # CRC32 lookup table
│   ├── bl_flash.c               # W25Q flash driver
│   └── bl_log.c                 # FRAM ring buffer logging
└── linker/
    └── bootloader.ld            # Places bootloader at 0x72000
```

### Key Design Decisions

1. **Bare-metal** - No FreeRTOS, no Nordic SDK dependencies
2. **Reuses shared headers** - `agsys_memory_layout.h` for FRAM addresses
3. **2-byte FRAM addressing** - Default for 8KB test FRAM (MB85RS64V)
4. **3-byte FRAM addressing** - Production with 128KB FRAM (MB85RS1MT)
5. **Dual backup slots** - A/B slots in external flash for redundancy

### Boot Flow

```
Power On → Init HW → Init Log → Init Flash → Read boot_info from FRAM
                                                      │
                                              ┌───────┴───────┐
                                              │               │
                                           Valid?          Invalid?
                                              │               │
                                              ▼               ▼
                                        Check boot_state   Init defaults
                                              │
                              ┌───────────────┼───────────────┐
                              │               │               │
                           NORMAL        OTA_PENDING      OTA_STAGED
                              │               │               │
                              │         boot_count++          │
                              │               │               │
                              │         > 3 attempts?         │
                              │          │        │           │
                              │         Yes       No          │
                              │          │        │           │
                              │          ▼        │           │
                              │      ROLLBACK     │           │
                              │          │        │           │
                              └──────────┴────────┴───────────┘
                                              │
                                              ▼
                                    Validate Application
                                              │
                                      ┌───────┴───────┐
                                      │               │
                                   Valid?          Invalid?
                                      │               │
                                      ▼               ▼
                                 Jump to App    Restore from Flash
                                                      │
                                              ┌───────┴───────┐
                                              │               │
                                           Success?        Failed?
                                              │               │
                                              ▼               ▼
                                         Jump to App       PANIC
                                                        (SOS LED)
```

### Wiring for Adafruit Feather nRF52832

```
Feather Pin    │ FRAM (MB85RS64V)  │ Flash (W25Q128)
───────────────┼───────────────────┼──────────────────
3.3V           │ VCC               │ VCC
GND            │ GND               │ GND
SCK (P0.12)    │ SCK               │ CLK
MOSI (P0.13)   │ MOSI              │ DI
MISO (P0.14)   │ MISO              │ DO
A0 (P0.02)     │ CS                │ -
A1 (P0.03)     │ -                 │ CS
```

### Build Commands

```bash
# Default build (test FRAM, default pins)
make -C devices/bootloader

# Build for Adafruit Feather
make -C devices/bootloader \
    CFLAGS+="-DBL_PIN_SPI_SCK=12 -DBL_PIN_SPI_MOSI=13 -DBL_PIN_SPI_MISO=14" \
    CFLAGS+="-DBL_PIN_FRAM_CS=2 -DBL_PIN_FLASH_CS=3 -DBL_PIN_LED=17"

# Build for production (128KB FRAM)
make -C devices/bootloader CFLAGS+="-DBL_FRAM_ADDR_BYTES=3"
```

### Flash Commands

```bash
# Using nrfjprog (J-Link required)
nrfjprog --program build/agsys_bootloader.hex --sectorerase --verify
nrfjprog --reset

# Using OpenOCD
openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \
    -c "program build/agsys_bootloader.hex verify reset exit"
```

## Memory Layout Reference

### Internal Flash (nRF52832 - 512KB)

| Address | Size | Content |
|---------|------|---------|
| 0x00000000 | 4KB | MBR (Nordic) |
| 0x00001000 | ~148KB | SoftDevice S132 |
| 0x00026000 | 296KB | Application |
| 0x00070000 | 8KB | Recovery Loader (frozen) |
| 0x00072000 | 16KB | Bootloader (this code) |
| 0x00076000 | 4KB | Bootloader Settings |

### FRAM Layout (MB85RS64V 8KB for testing)

| Address | Size | Content |
|---------|------|---------|
| 0x0000 | 16B | Layout Header |
| 0x0010 | 32B | Boot Info |
| 0x0100 | 528B | Boot Log (16 entries) |

### External Flash Layout (W25Q128 16MB)

| Address | Size | Content |
|---------|------|---------|
| 0x000000 | 4KB | Slot A Header |
| 0x001000 | 944KB | Slot A Firmware |
| 0x0ED000 | 4KB | Slot B Header |
| 0x0EE000 | 944KB | Slot B Firmware |
| 0x1DA000 | 16KB | Bootloader Backup |

## FRAM Log Events

| Event | Value | Description |
|-------|-------|-------------|
| BOOT_START | 0x01 | Bootloader started |
| BOOT_SUCCESS | 0x02 | Jumped to app |
| BOOT_FAIL | 0x03 | Boot failed |
| ROLLBACK_START | 0x10 | Starting rollback |
| ROLLBACK_SUCCESS | 0x11 | Rollback completed |
| ROLLBACK_FAIL | 0x12 | Rollback failed |
| APP_INVALID | 0x20 | App validation failed |
| FRAM_ERROR | 0x30 | FRAM error |
| FLASH_ERROR | 0x31 | External flash error |
| NVMC_ERROR | 0x32 | Internal flash error |
| PANIC | 0xFF | Entered panic mode |

## Next Steps (When Parts Arrive)

1. **Wire up test hardware**
   - Connect FRAM breakout to Feather (6 wires)
   - Connect Flash breakout to Feather (6 wires, shared SPI bus)

2. **Flash bootloader**
   - Build with Feather pin configuration
   - Flash via J-Link or OpenOCD

3. **Create test application**
   - Simple app with AGSY header at offset 0x200
   - Blinks LED to indicate running
   - Calls `confirm_boot()` to clear OTA_PENDING state

4. **Test rollback scenario**
   - Flash app that doesn't call `confirm_boot()`
   - Verify bootloader triggers rollback after 3 attempts
   - Check FRAM log for events

5. **Test firmware restore**
   - Write backup firmware to external flash slot A
   - Corrupt internal flash application
   - Verify bootloader restores from backup

## Related Files

- `devices/bootloader/README.md` - Full documentation
- `devices/freertos-common/include/agsys_memory_layout.h` - Shared memory layout
- `docs/design/bootloader-design.md` - Original design document

## Outstanding TODOs (from earlier session)

See `docs/SESSION_NOTES_20260112_OTA_DESIGN.md` for:
- Property Admin/Manager Deployment UX
- Breaking changes / requires_version strategy
- Long-term code migration and versioning
- Customer consent for data reporting
