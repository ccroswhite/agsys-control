# Soil Moisture Sensor - IoT Device Firmware

Ultra-low power soil moisture sensor for agricultural monitoring using LoRa communication.

## Target Hardware

- **MCU**: Nordic nRF52832-QFAA (512 KB Flash, 64 KB RAM, QFN48 6×6mm)
  - Adafruit Feather nRF52832 for prototyping
  - Custom PCB for production
- **LoRa Module**: HOPERF RFM95C (915 MHz ISM band, SX1276 chipset)
- **NVRAM**: FM25V02 SPI FRAM (8 KB) - config, calibration, logs
- **Flash**: W25Q16 SPI NOR Flash (2 MB) - firmware backup/rollback
- **BLE**: Built-in nRF52832 (for OTA firmware updates)
- **Power**: 21700 Li-ion battery (5000 mAh), MCP1700 LDO (2.5V output)

### nRF52832 Variants

| Variant | Flash | RAM | Package | Notes |
|---------|-------|-----|---------|-------|
| **nRF52832-QFAA** | **512 KB** | **64 KB** | QFN48 (6×6 mm) | **Required** |
| nRF52832-QFAB | 256 KB | 32 KB | QFN48 (6×6 mm) | Too small |
| nRF52832-CIAA | 512 KB | 64 KB | WLCSP | Difficult to place |

The **QFAA variant is required** - the QFAB has insufficient memory for SoftDevice + bootloader + application.

## Features

- **Ultra-low power operation**: ~1.9 µA deep sleep with RTC wake
- **Configurable wake interval**: Default 2 hours
- **Local data logging**: Stores readings in NVRAM when LoRa transmission fails
- **Battery monitoring**: Automatic extended sleep when battery is critical
- **Unique device identification**: UUID stored in NVRAM
- **BLE OTA updates**: Press button to enable firmware update via mobile phone

## Wiring Diagram

### System Block Diagram

```
                                    ┌─────────────────────────────────────────────────────────┐
                                    │                      nRF52832                           │
                                    │                   (Adafruit Feather)                    │
                                    │                                                         │
     ┌──────────────┐               │  P0.23 (SCK) ────────────────────────────┐              │
     │   21700      │               │  P0.24 (MOSI) ───────────────────────────┼──┐           │
     │   Li-Ion     │               │  P0.25 (MISO) ───────────────────────────┼──┼──┐        │
     │   5000mAh    │               │                                          │  │  │        │
     └──────┬───────┘               │  P0.27 ─────────────────────┐            │  │  │        │
            │ 3.0-4.2V              │  P0.30 ─────────────────────┼──┐         │  │  │        │
            │                       │  P0.31 ─────────────────────┼──┼──┐      │  │  │        │
     ┌──────┴───────┐               │                             │  │  │      │  │  │        │
     │  MCP1700     │               │  P0.11 ──────────────────┐  │  │  │      │  │  │        │
     │  LDO 2.5V    │               │  P0.14 ──────────────────┼──┼──┼──┼──┐   │  │  │        │
     └──────┬───────┘               │  P0.15 ──────────────────┼──┼──┼──┼──┼─┐ │  │  │        │
            │ 2.5V                  │  P0.16 ──────────────────┼──┼──┼──┼──┼─┼─│──│──│──┐     │
            │                       │                          │  │  │  │  │ │ │  │  │  │     │
            ├───────────────────────┤ VDD                      │  │  │  │  │ │ │  │  │  │     │
            │                       │                          │  │  │  │  │ │ │  │  │  │     │
            │                       │  P0.28/AIN4 ─────────────┼──┼──┼──┼──┼─┼─┼──┼──┼──┼──┐  │
            │                       │                          │  │  │  │  │ │ │  │  │  │  │  │
            │                       │  P0.17 ──────────────────┼──┼──┼──┼──┼─┼─┼──┼──┼──┼──┼─┐│
            │                       │  P0.19 ──────────────────┼──┼──┼──┼──┼─┼─┼──┼──┼──┼──┼─┼│
            │                       │  P0.20 ──────────────────┼──┼──┼──┼──┼─┼─┼──┼──┼──┼──┼─┼│
            │                       │  P0.07 ──────────────────┼──┼──┼──┼──┼─┼─┼──┼──┼──┼──┼─┼│
            │                       │                          │  │  │  │  │ │ │  │  │  │  │ ││
            │                       │  GND ────────────────────┼──┼──┼──┼──┼─┼─┼──┼──┼──┼──┼─┼│
            │                       └──────────────────────────┼──┼──┼──┼──┼─┼─┼──┼──┼──┼──┼─┼│
            │                                                  │  │  │  │  │ │ │  │  │  │  │ ││
            │                                                  │  │  │  │  │ │ │  │  │  │  │ ││
            │         ┌────────────────────────────────────────┘  │  │  │  │ │ │  │  │  │  │ ││
            │         │  ┌───────────────────────────────────────┘  │  │  │ │ │  │  │  │  │ ││
            │         │  │  ┌──────────────────────────────────────┘  │  │ │ │  │  │  │  │ ││
            │         │  │  │                                         │  │ │ │  │  │  │  │ ││
            │         │  │  │  ┌──────────────────────────────────────┘  │ │ │  │  │  │  │ ││
            │         │  │  │  │  ┌──────────────────────────────────────┘ │ │  │  │  │  │ ││
            │         │  │  │  │  │                                        │ │  │  │  │  │ ││
            │        CS RST DIO0 CS  A   B                                 PWR│  │  │  │  │ ││
            │         │  │  │  │  │                                        │ │  │  │  │  │ ││
     ┌──────┴─────────┴──┴──┴──┘  │                                        │ │  │  │  │  │ ││
     │         RFM95C LoRa Module │                                        │ │  │  │  │  │ ││
     │  ┌────────────────────────────────────┐                             │ │  │  │  │  │ ││
     │  │  VCC ─────────────────────── 2.5V  │                             │ │  │  │  │  │ ││
     │  │  GND ─────────────────────── GND   │                             │ │  │  │  │  │ ││
     │  │  SCK ─────────────────────── P0.23 │ (shared SPI bus)            │ │  │  │  │  │ ││
     │  │  MOSI ────────────────────── P0.24 │ (shared SPI bus)            │ │  │  │  │  │ ││
     │  │  MISO ────────────────────── P0.25 │ (shared SPI bus)            │ │  │  │  │  │ ││
     │  │  NSS ─────────────────────── P0.27 │                             │ │  │  │  │  │ ││
     │  │  RESET ───────────────────── P0.30 │                             │ │  │  │  │  │ ││
     │  │  DIO0 ────────────────────── P0.31 │                             │ │  │  │  │  │ ││
     │  │  ANT ─────────────────────── [915MHz Antenna]                    │ │  │  │  │  │ ││
     │  └────────────────────────────────────┘                             │ │  │  │  │  │ ││
     └─────────────────────────────────────────────────────────────────────┘ │  │  │  │  │ ││
                                                                             │  │  │  │  │ ││
     ┌───────────────────────────────────────────────────────────────────────┘  │  │  │  │ ││
     │         FM25V02 FRAM (8KB)                                               │  │  │  │ ││
     │  ┌────────────────────────────────────┐                                  │  │  │  │ ││
     │  │  VCC ─────────────────────── 2.5V  │                                  │  │  │  │ ││
     │  │  GND ─────────────────────── GND   │                                  │  │  │  │ ││
     │  │  SCK ─────────────────────── P0.23 │ (shared SPI bus)                 │  │  │  │ ││
     │  │  SI ──────────────────────── P0.24 │ (shared SPI bus)                 │  │  │  │ ││
     │  │  SO ──────────────────────── P0.25 │ (shared SPI bus)                 │  │  │  │ ││
     │  │  CS ──────────────────────── P0.11 │                                  │  │  │  │ ││
     │  │  WP ──────────────────────── VCC   │                                  │  │  │  │ ││
     │  │  HOLD ────────────────────── VCC   │                                  │  │  │  │ ││
     │  └────────────────────────────────────┘                                  │  │  │  │ ││
     └──────────────────────────────────────────────────────────────────────────┘  │  │  │ ││
                                                                                   │  │  │ ││
     ┌─────────────────────────────────────────────────────────────────────────────┘  │  │ ││
     │         H-Bridge Capacitance Sensor (100 kHz AC)                                │  │ ││
     │  ┌────────────────────────────────────┐                                        │  │ ││
     │  │  VCC ─────────────────────── 2.5V  │ (when power enabled)                   │  │ ││
     │  │  GND ─────────────────────── GND   │                                        │  │ ││
     │  │                                    │                                        │  │ ││
     │  │  ┌─────────┐      ┌─────────┐      │                                        │  │ ││
     │  │  │ Q1 P-ch │      │ Q2 P-ch │      │  SSM6P15FU                             │  │ ││
     │  │  │ SSM6P15 │      │ SSM6P15 │      │                                        │  │ ││
     │  │  └────┬────┘      └────┬────┘      │                                        │  │ ││
     │  │       │                │           │                                        │  │ ││
     │  │       ├───[PROBE+]─────┤           │  Capacitive Probe                      │  │ ││
     │  │       │                │           │  (epoxy sealed)                        │  │ ││
     │  │  ┌────┴────┐      ┌────┴────┐      │                                        │  │ ││
     │  │  │ Q3 N-ch │      │ Q4 N-ch │      │  2SK2009                               │  │ ││
     │  │  │ 2SK2009 │      │ 2SK2009 │      │                                        │  │ ││
     │  │  └─────────┘      └─────────┘      │                                        │  │ ││
     │  │                                    │                                        │  │ ││
     │  │  Q1,Q3 Gate ─────────────── P0.14  │  (HBRIDGE_A, Timer2+PPI)               │  │ ││
     │  │  Q2,Q4 Gate ─────────────── P0.15  │  (HBRIDGE_B, complementary)            │  │ ││
     │  │  Envelope Det ───────────── P0.02  │  (AIN0, ADC input)                     │  │ ││
     │  │  D1,D2 BAT54S flyback diodes       │                                        │  │ ││
     │  └────────────────────────────────────┘                                        │  │ ││
     └────────────────────────────────────────────────────────────────────────────────┘  │ ││
                                                                                         │ ││
     ┌───────────────────────────────────────────────────────────────────────────────────┘ ││
     │  H-Bridge Power Enable                                                              ││
     │  ┌──────────────────────────────────────────────────────────────────────────────────┤│
     │  │  P0.16 ──[10K]──┬──[P-ch MOSFET Gate]                                            ││
     │  │                 └── H-bridge VCC switched                                        ││
     │  └──────────────────────────────────────────────────────────────────────────────────┘│
     │                                                                                      │
     ┌──────────────────────────────────────────────────────────────────────────────────────┘
     │  LEDs & Button
     │  ┌──────────────────────────────────────────────────────────────────────────────────┐
     │  │  LED_STATUS (Green)  ── P0.17 ──[330Ω]──[LED]── GND                              │
     │  │  LED_SPI (Yellow)    ── P0.19 ──[330Ω]──[LED]── GND                              │
     │  │  LED_CONN (Blue)     ── P0.20 ──[330Ω]──[LED]── GND                              │
     │  │  OTA_BTN             ── P0.07 ──[BTN]── GND  (with 10K pull-up to VCC)           │
     │  └──────────────────────────────────────────────────────────────────────────────────┘
     │
     │  Battery Voltage Divider
     │  ┌──────────────────────────────────────────────────────────────────────────────────┐
     │  │  VBAT ──[100K]──┬──[100K]── GND                                                  │
     │  │                 └────────── P0.28/AIN4 (1/2 VBAT)                                │
     │  └──────────────────────────────────────────────────────────────────────────────────┘
     └─────────────────────────────────────────────────────────────────────────────────────
```

### Connection Summary

| Signal | nRF52 Pin | Connected To | Notes |
|--------|-----------|--------------|-------|
| **SPI Bus** |
| SPI_SCK | P0.23 | RFM95C, FM25V02, W25Q16 | Shared clock |
| SPI_MOSI | P0.24 | RFM95C, FM25V02, W25Q16 | Shared data out |
| SPI_MISO | P0.25 | RFM95C, FM25V02, W25Q16 | Shared data in |
| **LoRa Module** |
| LORA_CS | P0.27 | RFM95C.NSS | Chip select (active low) |
| LORA_RST | P0.30 | RFM95C.RESET | Reset (active low) |
| LORA_DIO0 | P0.31 | RFM95C.DIO0 | TX/RX done interrupt |
| **FRAM** |
| NVRAM_CS | P0.11 | FM25V02.CS | Chip select (active low) |
| **SPI Flash** |
| FLASH_CS | P0.12 | W25Q16.CS | Chip select (active low) |
| **H-Bridge Capacitance Sensor** |
| HBRIDGE_A | P0.14 | Q1.G, Q3.G | 100kHz drive via Timer2+PPI |
| HBRIDGE_B | P0.15 | Q2.G, Q4.G | Complementary drive |
| HBRIDGE_PWR | P0.16 | Power enable | High to enable H-bridge |
| **ADC** |
| MOISTURE_ADC | P0.02/AIN0 | Envelope detector | Capacitance measurement |
| VBAT_ADC | P0.28/AIN4 | Voltage divider | Battery monitoring |
| **LEDs** |
| LED_STATUS | P0.17 | Green LED | System heartbeat |
| LED_SPI | P0.19 | Yellow LED | SPI activity |
| LED_CONN | P0.20 | Blue LED | BLE connection |
| **Button** |
| OTA_BTN | P0.07 | Tactile switch | OTA mode (active low) |

### Eagle Schematic

A detailed Eagle 7-compatible schematic is available at:
- `docs/soilmoisture_schematic.sch`

Import into Eagle 7.x via: **File → Open → soilmoisture_schematic.sch**

## Pin Assignments (Custom PCB)

See `docs/pcb_pinout.md` for complete pinout documentation.

| nRF52 Pin | GPIO | Function | Description |
|-----------|------|----------|-------------|
| P0.02 | AIN0 | ADC | Moisture envelope detector |
| P0.28 | AIN4 | ADC | Battery voltage (via 2:1 divider) |
| P0.23 | 23 | SPI_SCK | SPI clock (LoRa + FRAM) |
| P0.24 | 24 | SPI_MOSI | SPI data out |
| P0.25 | 25 | SPI_MISO | SPI data in |
| P0.27 | 27 | LORA_CS | LoRa module chip select |
| P0.30 | 30 | LORA_RST | LoRa module reset |
| P0.31 | 31 | LORA_DIO0 | LoRa interrupt (TX/RX done) |
| P0.11 | 11 | NVRAM_CS | FRAM chip select |
| P0.14 | 14 | HBRIDGE_A | H-bridge drive A (Timer/PPI) |
| P0.15 | 15 | HBRIDGE_B | H-bridge drive B (Timer/PPI) |
| P0.16 | 16 | HBRIDGE_PWR | H-bridge power enable |
| P0.17 | 17 | LED_STATUS | Green LED - system heartbeat |
| P0.19 | 19 | LED_SPI | Yellow LED - SPI activity |
| P0.20 | 20 | LED_CONN | Blue LED - BLE connection |
| P0.07 | 7 | OTA_BUTTON | OTA update button (active LOW) |

### LED Behavior

| LED | Color | Behavior |
|-----|-------|----------|
| Status | Green | Blinks once at start of each wake cycle |
| SPI | Yellow | On during SPI activity (LoRa TX, FRAM access) |
| BLE | Blue | On during BLE advertising/connection |

## Building with PlatformIO

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE extension)

### Build Commands

```bash
# Build firmware
pio run

# Build and upload
pio run --target upload

# Monitor serial output
pio device monitor

# Clean build
pio run --target clean
```

### Environments

- `adafruit_feather_nrf52832` - Adafruit Feather nRF52832 (recommended)
- `sparkfun_nrf52832_breakout` - SparkFun nRF52832 Breakout

## Operation Cycle

1. **Wake** from deep sleep (RTC triggered)
2. **Initialize** peripherals (SPI, LoRa)
3. **Read sensors** (moisture, battery)
4. **Transmit** data via LoRa to leader
5. **Wait for ACK** from leader
6. **Log locally** if transmission fails
7. **Sleep** - enter deep sleep mode

## Power Consumption

The nRF52832 is highly optimized for low power operation:
- **Operating voltage**: 2.5V (via LDO from LiPo)
- **CPU clock**: 64MHz (with automatic power management)
- **DC-DC converter**: Enabled (reduces active power by ~20%)
- **SPI clock**: 1MHz (low power)

| State                         | Current (approx) |
|------------------------------|------------------|
| Active (CPU + DC-DC @ 2.5V)  | ~3-4 mA          |
| Active (LoRa TX @ +20dBm)    | ~120 mA          |
| BLE Advertising              | ~15 µA avg       |
| **Deep Sleep (System ON)**   | **~1.9 µA**      |

### Power Management

The nRF52832 uses the SoftDevice for automatic power management:
- CPU sleeps between events
- Peripherals are clock-gated when not in use
- RTC runs during sleep for wake timing

### Battery Life Estimate

**Recommended battery**: 21700 Li-ion cell, 5000 mAh (Samsung 50E or similar)

With 2-hour wake interval:
| Phase | Duration | Current | Energy/Cycle |
|-------|----------|---------|--------------|
| Active (sensor + LoRa TX) | ~1 sec | ~40 mA avg | ~11 µAh |
| Sleep | ~2 hours | ~1.9 µA | ~3.8 µAh |
| **Total per cycle** | | | **~15 µAh** |

- **Device average current**: ~7.5 µA
- **Battery self-discharge**: ~75-100 µA equivalent (1.5%/month)
- **Usable capacity to 2.8V**: ~4100 mAh
- **Estimated battery life**: **3-4 years**

*Note: Battery self-discharge dominates power consumption. Recommend yearly maintenance checks.*

## Firmware Updates (OTA)

Two methods are available for over-the-air firmware updates:

### Method 1: LoRa OTA (Primary - Remote Fleet Updates)

Updates are pushed from the leader to all devices without physical access.

**How it works:**
1. Controller broadcasts `OTA_ANNOUNCE` with new firmware version/size
2. Devices check if they need the update (version comparison)
3. Each device calculates a **staggered delay** (0-30 min) based on UUID
4. Devices request update chunks via LoRa
5. Controller sends firmware in 200-byte chunks with CRC
6. Device stores chunks in FRAM, ACKs each one
7. After all chunks received, device verifies full CRC
8. Device applies update via bootloader and reboots

**Timing for fleet updates:**
| Fleet Size | Approx. Time |
|------------|--------------|
| 10 devices | ~1-2 hours |
| 100 devices | ~12-24 hours |
| 500 devices | ~2-3 days |

**Features:**
- **Staggered updates** - Prevents network congestion
- **Resume support** - Continues from last chunk after power loss
- **CRC verification** - Ensures firmware integrity
- **Automatic rollout** - No manual intervention needed

### Method 2: BLE OTA (Fallback - Manual Recovery)

For manual updates or recovery if LoRa OTA fails.

1. **Press the OTA button** on the device (hold for 1 second)
2. **Blue LED** stays solid - device is advertising
3. **Open nRF Connect app** on your phone (iOS/Android)
4. **Connect to "AgSys-Soil"** device
5. **Select DFU** and upload new firmware (.zip package)
6. **Wait for transfer** (~30 seconds for typical firmware)
7. Device automatically reboots with new firmware

**OTA Window:** 5 minutes after button press

### Creating DFU Package

```bash
# Generate DFU package from compiled firmware
adafruit-nrfutil dfu genpkg --dev-type 0x0052 --application .pio/build/adafruit_feather_nrf52832/firmware.hex firmware_update.zip
```

## LoRa Configuration

Optimized for **long range** and **high device density**:

| Parameter         | Value       | Rationale                          |
|------------------|-------------|-------------------------------------|
| Frequency        | 915 MHz     | US ISM band                         |
| Bandwidth        | 125 kHz     | Narrowest = longest range           |
| Spreading Factor | SF10        | Good range, 4× faster than SF12     |
| Coding Rate      | 4/5         | Fast with error correction          |
| TX Power         | +20 dBm     | Maximum power for range             |
| Sync Word        | 0x34        | Private network                     |

### Network Capacity

- **Airtime per packet** (32 bytes): ~370 ms
- **Max packets/hour** (1% duty cycle): ~97
- **Devices supported**: 50+ sensors at 2-hour intervals

### Range Estimates

| Environment | Range |
|-------------|-------|
| Line-of-sight | 5-10 km |
| Rural with trees | 2-5 km |
| Urban/obstructed | 1-2 km |

## Protocol

See `include/protocol.h` for packet structure definitions.

### Message Types

- `0x01` - Sensor Report
- `0x02` - ACK
- `0x03` - Config Request
- `0x04` - Config Response
- `0x05` - Log Batch
- `0x06` - Time Sync

## Directory Structure

```
soilmoisture/
├── include/
│   ├── config.h          - Configuration settings
│   ├── protocol.h        - Communication protocol
│   ├── nvram.h           - NVRAM driver interface
│   ├── nvram_layout.h    - FRAM memory map and region definitions
│   ├── config_manager.h  - Configuration persistence manager
│   ├── capacitance.h     - AC capacitance measurement interface
│   └── ota_lora.h        - LoRa OTA firmware update interface
├── src/
│   ├── main.cpp          - Main application (with BLE OTA)
│   ├── protocol.cpp      - Protocol implementation
│   ├── nvram.cpp         - NVRAM driver implementation
│   ├── config_manager.cpp - Configuration manager implementation
│   ├── capacitance.cpp   - H-bridge AC capacitance driver
│   └── ota_lora.cpp      - LoRa OTA firmware update driver
├── docs/
│   └── hbridge_schematic.md - H-bridge circuit schematic
├── lib/                  - Project-specific libraries
├── platformio.ini        - PlatformIO configuration
└── README.md             - This file
```

## FRAM Memory Architecture

The device uses an 8KB SPI FRAM (FM25V02) for persistent storage of configuration, identity, and runtime data. The memory is partitioned into protected and non-protected regions to ensure critical data survives firmware updates.

### Device Identity

The device uses the **nRF52832 FICR** (Factory Information Configuration Registers) for identity:

```cpp
// Read 64-bit device ID (factory-programmed, immutable)
uint32_t deviceIdLow  = NRF_FICR->DEVICEID[0];
uint32_t deviceIdHigh = NRF_FICR->DEVICEID[1];

// Combine into 64-bit value
uint64_t deviceId = ((uint64_t)deviceIdHigh << 32) | deviceIdLow;
```

**Benefits:**
- **8 bytes** vs 16-byte UUID (50% smaller LoRa packets)
- **Factory-programmed** - globally unique, immutable
- **No FRAM storage needed** - identity tied to the chip
- **Tamper-resistant** - cannot be modified

Customer, location, and serial number information is managed in the **backend database**, keyed by the device ID.

### Memory Map

```
┌─────────────────────────────────────────────────────────────┐
│ PROTECTED REGION (0x0000 - 0x00FF) - 256 bytes              │
│ ├── Calibration (0x0000 - 0x003F) - 64 bytes                │
│ │   ├── Moisture sensor calibration (dry/wet values)        │
│ │   ├── Battery voltage offset/scale                        │
│ │   └── Temperature offset, LoRa frequency offset           │
│ ├── User Configuration (0x0040 - 0x00BF) - 128 bytes        │
│ │   ├── Sleep/report intervals                              │
│ │   ├── Battery thresholds, moisture alarms                 │
│ │   ├── LoRa TX power, spreading factor                     │
│ │   └── Gateway ID, network key                             │
│ └── Reserved (0x00C0 - 0x00FF) - 64 bytes                   │
├─────────────────────────────────────────────────────────────┤
│ FIRMWARE-MANAGED (0x0100 - 0x01FF) - 256 bytes              │
│ ├── Runtime State (0x0100 - 0x017F) - 128 bytes             │
│ │   ├── Boot count, last boot/report timestamps             │
│ │   ├── OTA progress tracking                               │
│ │   └── Firmware version history                            │
│ └── Statistics (0x0180 - 0x01FF) - 128 bytes                │
│     ├── TX/RX success/fail counters                         │
│     ├── OTA update history                                  │
│     └── Battery/temperature extremes                        │
├─────────────────────────────────────────────────────────────┤
│ OTA STAGING (0x0200 - 0x1BFF) - 6.5KB                       │
│ └── Firmware chunks during LoRa OTA updates                 │
├─────────────────────────────────────────────────────────────┤
│ LOG REGION (0x1C00 - 0x1FFF) - 1KB                          │
│ └── Circular buffer for sensor readings (~60 entries)       │
└─────────────────────────────────────────────────────────────┘
```

### Data Persistence

| Data Type | Survives OTA | Survives Factory Reset | Survives Chip Erase |
|-----------|--------------|------------------------|---------------------|
| Device ID (FICR) | ✓ | ✓ | ✓ (in chip) |
| Calibration | ✓ | ✓ | ✗ |
| User Config | ✓ | ✗ | ✗ |
| Runtime State | ✗ | ✗ | ✗ |
| Statistics | ✗ | ✗ | ✗ |

### ConfigManager API

The `ConfigManager` class provides a high-level interface for managing persistent configuration:

```cpp
#include "config_manager.h"

// Initialize (call once at startup)
ConfigManager configManager;
configManager.init();

// Get device ID (from FICR)
uint64_t deviceId = configManager.getDeviceId();
Serial.printf("Device ID: %016llX\n", deviceId);

// Get/set calibration
CalibrationData cal = configManager.getCalibration();
cal.moistureDry = 800;
cal.moistureWet = 2800;
configManager.setCalibration(cal);

// Apply calibration to sensor readings
uint8_t moisturePercent = configManager.applyMoistureCalibration(rawAdc, temperature);
uint16_t batteryMv = configManager.applyBatteryCalibration(rawBatteryMv);

// Save pending changes (call periodically or before sleep)
configManager.saveIfDirty();

// Factory reset (preserves calibration)
configManager.factoryReset();
```

### CRC32 Verification

All data blocks include CRC32 checksums for integrity verification:

- **Automatic verification** on load - corrupted blocks are detected
- **Automatic calculation** on save - ensures data integrity
- **Per-block CRC** - corruption in one block doesn't affect others

---

## LoRa Communication Protocol

### ALOHA with Channel Hopping

The sensor network uses an **ALOHA-style protocol** with **channel hopping** and **exponential backoff** for collision avoidance. This approach scales to 100+ sensors without requiring time synchronization.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Transmission Flow                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   Wake from sleep                                                        │
│        │                                                                 │
│        ▼                                                                 │
│   ┌────────────────────┐                                                 │
│   │ Random initial     │  TRNG: 0 to 2000ms                             │
│   │ jitter delay       │  Spreads first TX attempts                     │
│   └─────────┬──────────┘                                                 │
│             ▼                                                            │
│   ┌────────────────────┐                                                 │
│   │ Select random      │  TRNG: 0-63 → 902.3-914.9 MHz                  │
│   │ channel (TRNG)     │  New channel each attempt                      │
│   └─────────┬──────────┘                                                 │
│             ▼                                                            │
│   ┌────────────────────┐                                                 │
│   │     Transmit       │                                                 │
│   └─────────┬──────────┘                                                 │
│             ▼                                                            │
│   ┌────────────────────┐      Yes     ┌──────────────────┐               │
│   │  ACK received?     │─────────────►│     Success      │               │
│   │  (within 500ms)    │              │  Go back to sleep│               │
│   └─────────┬──────────┘              └──────────────────┘               │
│             │ No                                                         │
│             ▼                                                            │
│   ┌────────────────────┐                                                 │
│   │ retries++          │                                                 │
│   │ backoff = 1s × 2^n │  Exponential: 1s, 2s, 4s, 8s, 16s              │
│   │ + random jitter    │  Jitter: 0-50% of backoff                      │
│   └─────────┬──────────┘                                                 │
│             ▼                                                            │
│   ┌────────────────────┐      Yes     ┌──────────────────┐               │
│   │ retries > 5?       │─────────────►│  Give up, log    │               │
│   └─────────┬──────────┘              │  Try next cycle  │               │
│             │ No                      └──────────────────┘               │
│             ▼                                                            │
│   ┌────────────────────┐                                                 │
│   │ Sleep for backoff  │                                                 │
│   │ duration           │                                                 │
│   └─────────┬──────────┘                                                 │
│             │                                                            │
│             └──────────► Select new random channel, retry                │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Channel Hopping (US915)

The US915 ISM band provides 64 uplink channels:

| Parameter | Value |
|-----------|-------|
| Base frequency | 902.3 MHz (Channel 0) |
| Channel spacing | 200 kHz |
| Number of channels | 64 |
| Frequency range | 902.3 - 914.9 MHz |

Each transmission uses a **random channel** selected via the nRF52832's hardware TRNG. This provides:

- **Frequency diversity** - collisions only occur if two sensors pick the same channel at the same time
- **True randomness** - no predictable patterns that could cause repeated collisions
- **~1.5% collision probability** per transmission (vs ~63% with single channel)

### Exponential Backoff

On transmission failure (no ACK), the sensor waits with exponentially increasing delays:

| Retry | Base Backoff | With Jitter (0-50%) |
|-------|--------------|---------------------|
| 1 | 1 s | 1.0 - 1.5 s |
| 2 | 2 s | 2.0 - 3.0 s |
| 3 | 4 s | 4.0 - 6.0 s |
| 4 | 8 s | 8.0 - 12.0 s |
| 5 | 16 s | 16.0 - 24.0 s |
| **Max total** | | **~46 seconds** |

After 5 failed attempts, the sensor gives up and logs the failure. It will try again on the next scheduled reporting cycle.

### Why TRNG?

The nRF52832 has a hardware True Random Number Generator. Using TRNG instead of pseudo-random:

- **Prevents pattern collisions** - two sensors with similar seeds won't repeatedly collide
- **No seed management** - no need to maintain PRNG state
- **Negligible power cost** - ~60 nJ per call (0.00005% of TX energy)

### Configuration

See `config.h` for tunable parameters:

```c
// Channel hopping
#define LORA_BASE_FREQ_HZ           902300000
#define LORA_NUM_CHANNELS           64
#define LORA_USE_CHANNEL_HOPPING    1

// Backoff
#define TX_INITIAL_JITTER_MAX_MS    2000
#define BACKOFF_BASE_MS             1000
#define BACKOFF_MULTIPLIER          2
#define TX_MAX_RETRIES              5
```

---

## Soil Moisture Sensor

### AC Capacitance Measurement

The sensor uses a **discrete MOSFET H-bridge** to generate a 100kHz bipolar AC signal for capacitive soil moisture sensing. True AC measurement prevents soil polarization, enabling 10+ year probe life.

### Hardware

- **H-bridge**: 2× SSM6P15FU (P-ch) + 2× 2SK2009 (N-ch) + BAT54S flyback diodes
- **Drive**: nRF52832 Timer2 + PPI + GPIOTE (hardware-driven, zero CPU overhead)
- **Detection**: Envelope detector → ADC
- **Probe**: Sealed capacitive electrodes in epoxy (no soil contact)

### Why AC Measurement?

| Method | Pros | Cons |
|--------|------|------|
| DC Resistance | Simple, cheap | Electrolysis, probe corrosion, 1-2 year life |
| **AC Capacitance** | **No polarization, 10+ year life** | Slightly more complex circuit |

The 100kHz bipolar signal alternates polarity every 5µs, preventing any net DC current through the soil which would cause electrolysis and probe degradation.

### Measurement Process

1. Power enable asserted (P0.16)
2. Timer2 generates 100kHz complementary signals via PPI+GPIOTE
3. H-bridge drives probe with ±2.5V AC
4. Envelope detector converts AC amplitude to DC
5. ADC samples 1000 times over 1 second
6. Average computed, power disabled

### Power Consumption

- **Measurement active**: ~2.8 mA for 1 second (~0.78 µAh per reading)
- **Power gated**: 0 µA when not measuring

See `docs/hbridge_schematic.md` for circuit details.

## Calibration

**Calibration values MUST be measured on your actual hardware.** The ADC reads the envelope detector output (0-4095 for 12-bit), and the specific values depend on your probe geometry and envelope detector circuit.

In `config.h`:

```cpp
#define MOISTURE_DRY_VALUE    0       // TODO: Calibrate - ADC reading when dry
#define MOISTURE_WET_VALUE    0       // TODO: Calibrate - ADC reading when wet
```

### Calibration Procedure

1. Enable `DEBUG_MODE` in `config.h`
2. Build and flash firmware
3. Connect serial monitor (115200 baud)
4. Hold probe in **dry air** → record ADC value → set as `MOISTURE_DRY_VALUE`
5. Submerge probe in **water** → record ADC value → set as `MOISTURE_WET_VALUE`
6. Rebuild and flash with calibrated values

A compile-time warning will be generated until these values are set.

**Note:** Calibration may vary with soil type due to ionic conductivity effects at 100kHz. Consider field calibration for best accuracy.

## Security

### Code Protection (APPROTECT)

Production builds enable **APPROTECT** to prevent code readout via external debuggers.

| Build | APPROTECT | Debug Access |
|-------|-----------|--------------|
| `pio run -e debug` | Disabled | Full debug access |
| `pio run -e release` | **Enabled** | Blocked |

When APPROTECT is enabled:
- ✗ External debuggers cannot read flash (your code)
- ✗ External debuggers cannot read RAM
- ✗ Single-step debugging blocked
- ✓ **OTA updates still work** (CPU writes its own flash)
- ✓ Device ID (FICR) still accessible to firmware

### How It Works

```cpp
#include "security.h"

// Called automatically in setup() for release builds
security_init();  // Enables APPROTECT if not already enabled

// Get device ID (works regardless of APPROTECT)
uint64_t deviceId = security_getDeviceId();
```

On first boot of a release build:
1. `security_init()` checks if APPROTECT is enabled
2. If not, it writes to UICR.APPROTECT and resets
3. After reset, debug port is locked

### Recovery

To recover a protected device (e.g., for debugging):
1. Issue **ERASEALL** command via J-Link
2. This wipes all flash and UICR
3. APPROTECT is disabled (UICR reset to 0xFF)
4. Reprogram the device

⚠️ **Warning**: ERASEALL wipes calibration data in FRAM if performed during active SPI.

### Build Commands

```bash
# Development (debug enabled)
pio run -e debug
pio run -t upload -e debug

# Production (APPROTECT enabled)
pio run -e release
pio run -t upload -e release
```

### Firmware Rollback

The W25Q16 SPI flash (2 MB) stores an **encrypted backup** of the previous firmware for automatic rollback.

#### Encryption

Firmware backups are encrypted using **AES-256-CTR** with a device-specific key:

```
Key = SHA-256(SECRET_SALT || FICR_DEVICE_ID)
```

- **SECRET_SALT**: 32-byte compile-time constant (keep secret!)
- **FICR_DEVICE_ID**: 64-bit factory-programmed chip ID

This ensures:
- ✓ Backup is encrypted at rest (can't read code from external flash)
- ✓ Key is unique per device (can't copy backup between devices)
- ✓ Attacker needs both physical access AND the secret salt

⚠️ **Important**: Change `SECRET_SALT` in `firmware_crypto.cpp` before production!

```
W25Q16 Flash Layout (2 MB):
┌─────────────────────────────────────────────────────────────┐
│ Encrypted Firmware Backup      0x000000 - 0x0FFFFF (1 MB)   │
├─────────────────────────────────────────────────────────────┤
│ Firmware Metadata              0x100000 - 0x100FFF (4 KB)   │
│   - Previous version, CRC, timestamp                        │
├─────────────────────────────────────────────────────────────┤
│ Reserved                       0x101000 - 0x1FFFFF (~1 MB)  │
└─────────────────────────────────────────────────────────────┘
```

#### Rollback Process

1. Before OTA update, current firmware is **encrypted** and copied to W25Q16
2. New firmware is written to nRF52832 internal flash
3. Bootloader sets "pending validation" flag
4. New firmware boots and must call `firmware_markValid()` within 60 seconds
5. If validation fails (crash, watchdog, or timeout), bootloader **decrypts** and restores from W25Q16

#### API

```cpp
#include "firmware_crypto.h"

// Initialize crypto (call once at startup)
fw_crypto_init();

// Write encrypted firmware chunk to external flash
fw_crypto_write_chunk(flash_addr, data, length);

// Read and decrypt firmware chunk from external flash
fw_crypto_read_chunk(flash_addr, buffer, length);
```

**Manual Rollback:**

```cpp
#include "firmware.h"

// Trigger rollback to previous firmware
firmware_rollback();  // Decrypts backup from W25Q16, resets
```

### LoRa Packet Encryption

LoRa packets are encrypted using **AES-128-GCM** for authenticated encryption:

```
Packet Format: [Nonce:4][Ciphertext:N][Tag:4]
Total overhead: 8 bytes per packet
```

**Key Derivation:**
```
Key = SHA-256(LORA_SECRET_SALT || FICR_DEVICE_ID)[0:16]
```

**Security Features:**
- ✓ Encryption (AES-128) - payload confidentiality
- ✓ Authentication (GCM tag) - tamper detection
- ✓ Replay protection (nonce/packet counter)
- ✓ Device binding (device ID in associated data)

**API:**

```cpp
#include "lora_crypto.h"

// Initialize (call once at startup)
lora_crypto_init();

// Encrypt a packet
uint8_t plaintext[] = {0x01, 0x02, 0x03};
uint8_t packet[LORA_MAX_PACKET];
size_t packet_len;
lora_crypto_encrypt(plaintext, sizeof(plaintext), packet, &packet_len);

// Decrypt a packet
uint8_t decrypted[LORA_MAX_PLAINTEXT];
size_t decrypted_len;
if (lora_crypto_decrypt(packet, packet_len, decrypted, &decrypted_len)) {
    // Success - auth tag valid
} else {
    // Failed - tampered or wrong key
}
```

⚠️ **Important**: Change `LORA_SECRET_SALT` in `lora_crypto.cpp` before production!

### BLE Security

BLE connections use **LESC (LE Secure Connections)** with passkey authentication:

**Security Features:**
- ✓ LESC pairing (Elliptic Curve Diffie-Hellman key exchange)
- ✓ MITM protection (passkey verification)
- ✓ Link-layer encryption (AES-CCM)
- ✓ Bonding support (remembered pairings)

**Pairing Flow:**
1. Device enters OTA mode (button press or LoRa command)
2. Device advertises BLE, displays 6-digit passkey on serial
3. User connects from phone/laptop, enters passkey
4. LESC key exchange establishes encrypted link
5. DFU service available for firmware updates

**Configuration (in `main.cpp`):**
```cpp
Bluefruit.Security.setIOCaps(true, false, false);  // Display only
Bluefruit.Security.setMITM(true);                  // Require MITM protection
```

**Note:** For field deployment without serial access, consider:
- Fixed passkey derived from device ID (last 6 digits)
- Or app-layer encryption using `lora_crypto` module

## License

Proprietary - AgSys Control
