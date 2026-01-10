# Valve Controller - Schematic Reference

## Overview

This document provides the complete netlist and connection details for the valve controller board. Use this as a reference when updating the Eagle schematic.

## Bill of Materials (BOM)

### Integrated Circuits

| Ref | Part Number | Description | Package | Qty |
|-----|-------------|-------------|---------|-----|
| U1 | nRF52832-QFAA | MCU, 512KB Flash, 64KB RAM, BLE 5.0 | QFN-48 (6x6mm) | 1 |
| U2 | MCP2515-I/SO | CAN Controller, SPI interface | SOIC-18 | 1 |
| U3 | SN65HVD230DR | CAN Transceiver, 3.3V | SOIC-8 | 1 |
| U4 | RFM95C-915S2 | LoRa Transceiver, 915 MHz | SMD Module | 1 |
| U5 | FM25V02-G | 256Kbit FRAM, SPI | SOIC-8 | 1 |
| U6 | W25Q16JVSSIQ | 16Mbit SPI Flash | SOIC-8 | 1 |
| U7 | RV-3028-C7 | Ultra-low power RTC, I2C | SOIC-8 | 1 |
| U8 | MCP1700-3302E | 3.3V LDO, 250mA | SOT-23 | 1 |

### Connectors

| Ref | Part Number | Description | Notes |
|-----|-------------|-------------|-------|
| J1 | Phoenix XPC 4-pin | CAN Bus In | CAN_H, CAN_L, 24V, GND |
| J2 | Phoenix XPC 4-pin | CAN Bus Out | CAN_H, CAN_L, 24V, GND (daisy chain) |
| J3 | Phoenix XPC 3-pin | Power Input | 24V, GND, POWER_FAIL |
| J4 | U.FL | LoRa Antenna | 50Ω to external antenna |
| SW1 | Tactile 6x6mm | BLE Pairing Button | Active LOW, with 10K pullup |

### Passives

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C1-C4 | 100nF | 0402 | IC decoupling |
| C5, C6 | 22pF | 0402 | MCP2515 crystal load |
| C7 | 10µF | 0805 | LDO input |
| C8 | 10µF | 0805 | LDO output |
| R1 | 120Ω | 0603 | CAN termination (switchable) |
| R2 | 10K | 0402 | Pairing button pullup |
| R3-R5 | 330Ω | 0402 | LED current limiting |
| Y1 | 16MHz | HC49 | MCP2515 crystal |
| Y2 | 32.768kHz | 3215 | RTC crystal |

### LEDs

| Ref | Color | GPIO | Description |
|-----|-------|------|-------------|
| LED1 | Green | P0.17 | 3.3V Power indicator |
| LED2 | Yellow | P0.18 | 24V Present indicator |
| LED3 | Red | P0.20 | Status/Pairing indicator |

---

## Pin Assignments - nRF52832

| Pin | GPIO | Function | Direction | Notes |
|-----|------|----------|-----------|-------|
| P0.02 | AIN0 | 24V_SENSE | Input | ADC, voltage divider from 24V |
| P0.03 | - | SPI_SCK | Output | SPI clock |
| P0.04 | - | SPI_MOSI | Output | SPI data out |
| P0.05 | - | SPI_MISO | Input | SPI data in |
| P0.06 | - | CAN_CS | Output | MCP2515 chip select |
| P0.07 | - | CAN_INT | Input | MCP2515 interrupt |
| P0.08 | - | LORA_CS | Output | RFM95C chip select |
| P0.09 | - | LORA_RST | Output | RFM95C reset |
| P0.10 | - | LORA_DIO0 | Input | RFM95C interrupt |
| P0.11 | - | FRAM_CS | Output | FM25V02 chip select |
| P0.12 | - | FLASH_CS | Output | W25Q16 chip select |
| P0.13 | - | I2C_SCL | Output | RTC I2C clock |
| P0.14 | - | I2C_SDA | I/O | RTC I2C data |
| P0.15 | - | RTC_INT | Input | RV-3028 interrupt |
| P0.16 | - | POWER_FAIL | Input | Power fail signal (active LOW) |
| P0.17 | - | LED_3V3 | Output | Green LED (active HIGH) |
| P0.18 | - | LED_24V | Output | Yellow LED (active HIGH) |
| P0.20 | - | LED_STATUS | Output | Red LED (active HIGH) |
| P0.30 | - | PAIRING_BTN | Input | BLE pairing button (active LOW) |

---

## BLE Pairing Button (SW1)

Tactile switch for entering BLE pairing mode.

**Operation:**
- Hold for 3 seconds during power-up → Enter pairing mode
- LED3 (Red) blinks rapidly (100ms) during pairing mode
- Pairing mode auto-exits after 2 minutes (120 seconds)

**Connections:**

```
3.3V ──┬── R2 (10K) ──┬── P0.30 (PAIRING_BTN)
       │              │
       └──────────────┴── SW1 ── GND
```

**ESD Protection:** Add TVS diode (PESD5V0S1BL) on P0.30 for field robustness.

---

## LED Indicators

### LED1 - 3.3V Power (Green)

```
3.3V ── R3 (330Ω) ── LED1 (Green) ── P0.17
```

- **ON:** 3.3V power present
- Always on during normal operation

### LED2 - 24V Power (Yellow)

```
3.3V ── R4 (330Ω) ── LED2 (Yellow) ── P0.18
```

- **ON:** 24V power present (not on battery)
- **OFF:** Running on battery backup

### LED3 - Status (Red)

```
3.3V ── R5 (330Ω) ── LED3 (Red) ── P0.20
```

| Pattern | Meaning |
|---------|---------|
| Fast blink (100ms) | Pairing mode active |
| Slow blink (1s) | Running on battery |
| Off | Normal operation |

---

## SPI Bus Sharing

The SPI bus is shared between multiple devices:

| Signal | nRF52832 | MCP2515 | RFM95C | FM25V02 | W25Q16 |
|--------|----------|---------|--------|---------|--------|
| SCK | P0.03 | SCK (13) | SCK | SCK (6) | CLK (6) |
| MOSI | P0.04 | SI (14) | MOSI | SI (5) | DI (5) |
| MISO | P0.05 | SO (15) | MISO | SO (2) | DO (2) |
| CS_CAN | P0.06 | CS (16) | - | - | - |
| CS_LORA | P0.08 | - | NSS | - | - |
| CS_FRAM | P0.11 | - | - | CS (1) | - |
| CS_FLASH | P0.12 | - | - | - | CS (1) |

**SPI Mode:** All devices use SPI Mode 0 (CPOL=0, CPHA=0).
**Max Speed:** Use 8MHz for compatibility across all devices.

---

## I2C Bus

| Signal | nRF52832 | RV-3028 |
|--------|----------|---------|
| SCL | P0.13 | SCL (6) |
| SDA | P0.14 | SDA (5) |

**Pull-ups:** 4.7K to 3.3V on both lines.

---

## Power Architecture

```
External PSU (24V)
    │
    ├──► J3 Pin 1 (24V) ──► CAN bus power out (J1, J2)
    │                            │
    │                            └──► Voltage divider ──► P0.02 (24V_SENSE)
    │
    ├──► J3 Pin 2 (GND) ──► System ground
    │
    └──► J3 Pin 3 (POWER_FAIL) ──► P0.16 (active LOW when AC lost)

24V ──► U8 (MCP1700) ──► 3.3V rail
                              │
                              ├──► U1 (nRF52832)
                              ├──► U2 (MCP2515)
                              ├──► U3 (SN65HVD230)
                              ├──► U4 (RFM95C)
                              ├──► U5 (FM25V02)
                              ├──► U6 (W25Q16)
                              ├──► U7 (RV-3028)
                              └──► LEDs, pullups
```

---

## Power Fail Behavior

1. AC loss detected via optocoupler on PSU board
2. POWER_FAIL signal goes LOW
3. Controller immediately sends emergency close to all actuators via CAN
4. Event logged to FRAM
5. Enter low-power mode (battery conservation)
6. LED2 (24V) turns off, LED3 blinks slowly
7. When AC restored, resume normal operation

---

## Revision History

| Rev | Date | Changes |
|-----|------|---------|
| 1.0 | 2025-01 | Initial design |
| 1.1 | 2026-01 | Added BLE pairing button (SW1) |
| 1.1 | 2026-01 | Added LED status patterns for pairing mode |
| 1.1 | 2026-01 | Added 2-minute pairing timeout |
