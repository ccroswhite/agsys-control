# Valve Actuator - Schematic Reference

## Overview

This document provides the complete netlist and connection details for the valve actuator board. Use this as a reference when updating the Eagle schematic.

## Bill of Materials (BOM)

### Integrated Circuits

| Ref | Part Number | Description | Package | Qty |
|-----|-------------|-------------|---------|-----|
| U1 | nRF52810-QFAA | MCU, 192KB Flash, 24KB RAM, BLE 5.0 | QFN-48 (6x6mm) | 1 |
| U2 | MCP2515-I/SO | CAN Controller, SPI interface | SOIC-18 | 1 |
| U3 | SN65HVD230DR | CAN Transceiver, 3.3V | SOIC-8 | 1 |
| U4 | TPS54202DDCR | Buck Converter, 24V→3.3V, 2A | SOT-23-6 | 1 |
| U5 | FM25V02-G | 256Kbit FRAM, SPI | SOIC-8 | 1 |

### Connectors

| Ref | Part Number | Description | Notes |
|-----|-------------|-------------|-------|
| J1 | Phoenix XPC 5-pin | Valve Motor Connector | Motor+, Motor-, Limit Open, Limit Closed, GND |
| J2 | Phoenix XPC 4-pin | CAN Bus In | CAN_H, CAN_L, 24V, GND |
| J3 | Phoenix XPC 4-pin | CAN Bus Out | CAN_H, CAN_L, 24V, GND (daisy chain) |
| SW1 | 10-pos DIP Switch | Address & Config | Bits 1-6: Address, Bit 10: CAN Term |
| SW2 | Tactile 6x6mm | BLE Pairing Button | Active LOW, with 10K pullup |

### H-Bridge Components

| Ref | Part Number | Description | Notes |
|-----|-------------|-------------|-------|
| Q1, Q2 | AO3401A | P-FET, High-side | SOT-23, -30V, -4A |
| Q3, Q4 | AO3400A | N-FET, Low-side | SOT-23, 30V, 5.7A |
| D1-D4 | SS14 | Schottky Flyback | SMA, 1A, 40V |
| D5, D6 | SMBJ28A | TVS Motor Protection | SMB, 28V standoff |
| R1 | 0.05Ω 1% 1/2W | Current Sense Shunt | 2512 package |

### Power

| Ref | Part Number | Description | Notes |
|-----|-------------|-------------|-------|
| U4 | TPS54202DDCR | Buck 24V→3.3V | 2A output |
| L1 | 10µH 2A | Buck Inductor | 1210 package |
| F1 | MF-MSMF200 | PTC Fuse 2A | Resettable |
| C1 | 10µF 50V | Buck Input | 0805 X5R |
| C2 | 22µF 10V | Buck Output | 0805 X5R |

### Passives

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C3 | 100nF | 0402 | MCU decoupling |
| C4 | 100nF | 0402 | MCP2515 decoupling |
| C5 | 100nF | 0402 | CAN transceiver decoupling |
| C6 | 100nF | 0402 | FRAM decoupling |
| C7, C8 | 22pF | 0402 | MCP2515 crystal load |
| R2 | 120Ω | 0603 | CAN termination (switchable) |
| R3 | 10K | 0402 | Pairing button pullup |
| R4-R7 | 330Ω | 0402 | LED current limiting |
| Y1 | 16MHz | HC49 | MCP2515 crystal |

### LEDs

| Ref | Color | Description |
|-----|-------|-------------|
| LED1 | Green | 3.3V Power indicator |
| LED2 | Yellow | 24V Present indicator |
| LED3 | Red | Status/Error |
| LED4 | Blue | Valve Open indicator |

---

## Pin Assignments - nRF52810

| Pin | GPIO | Function | Direction | Notes |
|-----|------|----------|-----------|-------|
| P0.02 | AIN0 | CURRENT_SENSE | Input | ADC, voltage across shunt |
| P0.03 | - | HBRIDGE_A | Output | High-side A (open direction) |
| P0.04 | - | HBRIDGE_B | Output | High-side B (close direction) |
| P0.05 | - | HBRIDGE_EN_A | Output | Low-side A enable |
| P0.06 | - | HBRIDGE_EN_B | Output | Low-side B enable |
| P0.07 | - | FRAM_CS | Output | FRAM chip select |
| P0.08 | - | CAN_INT | Input | MCP2515 interrupt |
| P0.09 | - | LIMIT_OPEN | Input | Valve fully open (active LOW) |
| P0.10 | - | LIMIT_CLOSED | Input | Valve fully closed (active LOW) |
| P0.11 | - | CAN_CS | Output | MCP2515 chip select |
| P0.12 | - | SPI_MOSI | Output | SPI data out |
| P0.13 | - | SPI_MISO | Input | SPI data in |
| P0.14 | - | SPI_SCK | Output | SPI clock |
| P0.15-P0.20 | - | DIP_1-6 | Input | Address bits (active LOW) |
| P0.21-P0.23 | - | DIP_7-9 | Input | Reserved |
| P0.24 | - | DIP_10 | Input | CAN termination enable |
| P0.25 | - | LED_3V3 | Output | Green LED |
| P0.26 | - | LED_24V | Output | Yellow LED |
| P0.27 | - | LED_STATUS | Output | Red LED |
| P0.28 | - | LED_VALVE_OPEN | Output | Blue LED |
| P0.29 | AIN5 | 24V_SENSE | Input | ADC, voltage divider from 24V |
| P0.30 | - | PAIRING_BTN | Input | BLE pairing button (active LOW) |

---

## New Components for BLE Support

### FRAM (U5 - FM25V02-G)

The FRAM stores:
- BLE pairing PIN (6 bytes at address 0x0010)
- Valve cycle count (4 bytes at address 0x0020)
- Boot count (4 bytes at address 0x0024)
- Last error code (2 bytes at address 0x0028)

**Connections:**

| FM25V02 Pin | Signal | nRF52810 Pin |
|-------------|--------|--------------|
| 1 (CS#) | FRAM_CS | P0.07 |
| 2 (SO) | SPI_MISO | P0.13 |
| 3 (WP#) | 3.3V | - |
| 4 (VSS) | GND | - |
| 5 (SI) | SPI_MOSI | P0.12 |
| 6 (SCK) | SPI_SCK | P0.14 |
| 7 (HOLD#) | 3.3V | - |
| 8 (VDD) | 3.3V | - |

**Decoupling:** 100nF ceramic (C6) close to VDD pin.

### BLE Pairing Button (SW2)

Tactile switch for entering BLE pairing/DFU mode.

**Operation:**
- Hold for 3 seconds during power-up → Enter pairing mode
- Hold for 5 seconds while running → Enter DFU mode
- LED3 (Red) blinks rapidly (100ms) during pairing mode
- Pairing mode auto-exits after 2 minutes (120 seconds)

**Connections:**

```
3.3V ──┬── R3 (10K) ──┬── P0.30 (PAIRING_BTN)
       │              │
       └──────────────┴── SW2 ── GND
```

**ESD Protection:** Add TVS diode (PESD5V0S1BL) on P0.30 for field robustness.

---

## SPI Bus Sharing

The SPI bus is shared between MCP2515 (CAN) and FM25V02 (FRAM):

| Signal | nRF52810 | MCP2515 | FM25V02 |
|--------|----------|---------|---------|
| MOSI | P0.12 | SI (14) | SI (5) |
| MISO | P0.13 | SO (15) | SO (2) |
| SCK | P0.14 | SCK (13) | SCK (6) |
| CS_CAN | P0.11 | CS (16) | - |
| CS_FRAM | P0.07 | - | CS (1) |

**SPI Mode:** Both devices use SPI Mode 0 (CPOL=0, CPHA=0).
**Max Speed:** MCP2515: 10MHz, FM25V02: 40MHz. Use 8MHz for compatibility.

---

## Power Architecture

```
24V (from CAN bus J2/J3)
    │
    ├──► F1 (PTC 2A) ──► Protected 24V rail
    │                         │
    │                         ├──► H-Bridge (Q1-Q4)
    │                         │
    │                         └──► Voltage divider ──► P0.29 (24V_SENSE)
    │
    └──► U4 (TPS54202) ──► 3.3V rail
                              │
                              ├──► U1 (nRF52810)
                              ├──► U2 (MCP2515)
                              ├──► U3 (SN65HVD230)
                              ├──► U5 (FM25V02)
                              └──► LEDs, pullups
```

---

## DIP Switch Configuration

| Switch | Function | ON | OFF |
|--------|----------|-----|-----|
| 1 | Address bit 0 | +1 | +0 |
| 2 | Address bit 1 | +2 | +0 |
| 3 | Address bit 2 | +4 | +0 |
| 4 | Address bit 3 | +8 | +0 |
| 5 | Address bit 4 | +16 | +0 |
| 6 | Address bit 5 | +32 | +0 |
| 7 | Reserved | - | - |
| 8 | Reserved | - | - |
| 9 | Reserved | - | - |
| 10 | CAN Termination | Enabled | Disabled |

**Valid Addresses:** 1-63 (address 0 is invalid)

---

## Revision History

| Rev | Date | Changes |
|-----|------|---------|
| 1.0 | 2025-01 | Initial design |
| 1.1 | 2026-01 | Added FM25V02 FRAM for BLE PIN storage |
| 1.1 | 2026-01 | Added BLE pairing button (SW2) |
