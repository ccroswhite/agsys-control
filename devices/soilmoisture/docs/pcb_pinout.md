# nRF52832 Custom PCB Pinout

## Overview

This document defines the complete pinout for the soil moisture sensor custom PCB based on the Nordic nRF52832 microcontroller.

## nRF52832 Pin Assignment

### Power Pins

| Pin | Function | Notes |
|-----|----------|-------|
| VDD | 2.5V | Main supply (via LDO from LiPo) |
| VSS | GND | Ground |
| DCCH | DC-DC inductor | 10µH inductor to DCCD |
| DCCD | DC-DC output | Connect to VDD via 10µH |
| DEC1-4 | Decoupling | 100nF caps to GND |
| AVDD | Analog VDD | Connect to VDD |

### SPI Bus (Directly Routed)

| nRF52 Pin | GPIO | Function | Connected To |
|-----------|------|----------|--------------|
| P0.25 | 25 | SPI_MISO | RFM95C MISO, FRAM SO |
| P0.24 | 24 | SPI_MOSI | RFM95C MOSI, FRAM SI |
| P0.23 | 23 | SPI_SCK | RFM95C SCK, FRAM SCK |

### LoRa Module (RFM95C)

| nRF52 Pin | GPIO | Function | RFM95C Pin |
|-----------|------|----------|------------|
| P0.27 | 27 | LORA_CS | NSS |
| P0.30 | 30 | LORA_RST | RESET |
| P0.31 | 31 | LORA_DIO0 | DIO0 (IRQ) |
| - | - | VCC | 2.5V |
| - | - | GND | GND |

### NVRAM (SPI FRAM)

| nRF52 Pin | GPIO | Function | FRAM Pin |
|-----------|------|----------|----------|
| P0.11 | 11 | NVRAM_CS | CS |
| - | - | VCC | 2.5V |
| - | - | GND | GND |

### H-Bridge (AC Capacitance Sensor)

| nRF52 Pin | GPIO | Function | H-Bridge Connection |
|-----------|------|----------|---------------------|
| P0.14 | 14 | HBRIDGE_A | Q1 (P-ch) gate via 10kΩ, Q3 (N-ch) gate via 10kΩ |
| P0.15 | 15 | HBRIDGE_B | Q2 (P-ch) gate via 10kΩ, Q4 (N-ch) gate via 10kΩ |
| P0.16 | 16 | HBRIDGE_PWR | H-bridge power enable (optional) |
| P0.02 | AIN0 | MOISTURE_ADC | Envelope detector output |

### Battery Monitoring

| nRF52 Pin | GPIO | Function | Notes |
|-----------|------|----------|-------|
| P0.28 | AIN4 | BATTERY_ADC | Via 2:1 voltage divider from VBAT |

### Status LEDs (accent-sink, active LOW)

| nRF52 Pin | GPIO | Function | LED Color | Purpose |
|-----------|------|----------|-----------|---------|
| P0.17 | 17 | LED_STATUS | Green | System heartbeat |
| P0.19 | 19 | LED_SPI | Yellow | SPI activity |
| P0.20 | 20 | LED_CONN | Blue | BLE connection |

### User Interface

| nRF52 Pin | GPIO | Function | Notes |
|-----------|------|----------|-------|
| P0.07 | 7 | PAIRING_BUTTON | Active LOW with internal pull-up, 2s hold for pairing |

### Crystal Oscillator (32.768 kHz)

| nRF52 Pin | Function | Notes |
|-----------|----------|-------|
| XL1 | LFXTAL_IN | 32.768 kHz crystal |
| XL2 | LFXTAL_OUT | 32.768 kHz crystal |

### Antenna

| nRF52 Pin | Function | Notes |
|-----------|----------|-------|
| ANT | BLE Antenna | 50Ω trace to chip antenna or U.FL |

---

## Complete GPIO Summary

```
nRF52832 GPIO Allocation
========================

P0.02  [AIN0]  ← Moisture ADC (envelope detector)
P0.07  [GPIO]  ← OTA Button (input, pull-up)
P0.11  [GPIO]  → FRAM CS
P0.14  [GPIO]  → H-Bridge A (Timer/PPI driven)
P0.15  [GPIO]  → H-Bridge B (Timer/PPI driven)
P0.16  [GPIO]  → H-Bridge Power Enable
P0.17  [GPIO]  → LED Status (Green)
P0.19  [GPIO]  → LED SPI Activity (Yellow)
P0.20  [GPIO]  → LED BLE Connection (Blue)
P0.23  [SPI]   → SPI SCK
P0.24  [SPI]   → SPI MOSI
P0.25  [SPI]   ← SPI MISO
P0.27  [GPIO]  → LoRa CS
P0.28  [AIN4]  ← Battery ADC (via divider)
P0.30  [GPIO]  → LoRa RST
P0.31  [GPIO]  ← LoRa DIO0 (IRQ)

Free GPIOs: P0.03, P0.04, P0.05, P0.06, P0.08, P0.09, P0.10,
            P0.12, P0.13, P0.18, P0.21, P0.22, P0.26, P0.29
```

---

## Schematic Blocks

### 1. Power Supply

```
VBAT (3.0-4.2V) ──┬──► 2:1 divider ──► P0.28 (Battery ADC)
                  │
                  ▼
            ┌─────────┐
            │  LDO    │
            │ 2.5V    │
            └────┬────┘
                 │
                 ├──► VDD (nRF52832)
                 ├──► RFM95C VCC
                 ├──► FRAM VCC
                 └──► H-Bridge VCC
```

### 2. SPI Bus

```
                    ┌─────────────┐
P0.23 (SCK)  ──────►│             │
P0.24 (MOSI) ──────►│   SPI Bus   │──────► RFM95C
P0.25 (MISO) ◄──────│             │──────► FRAM
                    └─────────────┘
                          │
P0.27 (LORA_CS) ──────────┼──► RFM95C NSS
P0.11 (NVRAM_CS) ─────────┴──► FRAM CS
```

### 3. H-Bridge AC Capacitance

```
                         VCC (2.5V)
                            │
              ┌─────────────┼─────────────┐
              │            1.1kΩ          │
             Q1(P)          │           Q2(P)
              │             │             │
P0.14 ──10kΩ──┤             │             ├──10kΩ── P0.15
              │             │             │
              ├─────────────┴─────────────┤
              │          PROBE            │
              ├─────────────┬─────────────┤
              │             │             │
P0.14 ──10kΩ──┤             │             ├──10kΩ── P0.15
              │             │             │
             Q3(N)          │           Q4(N)
              │            1.1kΩ          │
              └─────────────┼─────────────┘
                            │
                           GND
                            │
                    Envelope Detector
                            │
                            ▼
                      P0.02 (ADC)
```

### 4. Status LEDs

```
VCC (2.5V)
    │
    ├──► 330Ω ──► LED (Green)  ──► P0.17 (sink)
    │
    ├──► 330Ω ──► LED (Yellow) ──► P0.19 (sink)
    │
    └──► 330Ω ──► LED (Blue)   ──► P0.20 (sink)

LED ON  = GPIO LOW
LED OFF = GPIO HIGH
```

**LED Status Patterns:**

| Pattern | Meaning |
|---------|---------|
| Green fast blink (100ms) | Pairing mode active |
| Green SOS pattern | Critical battery |
| Green off | Normal sleep mode |

### 5. Pairing Button

```
VCC (2.5V)
    │
    └── Internal Pull-up ──┬── P0.07 (PAIRING_BUTTON)
                           │
                           └── SW1 ── GND
```

**Operation:**
- Hold for 2 seconds during power-up → Enter pairing mode
- Hold for 2 seconds during sleep wake → Enter pairing mode
- LED (Green) blinks rapidly (100ms) during pairing mode
- Pairing mode auto-exits after 2 minutes (120 seconds)

---

## Bill of Materials (Key Components)

| Ref | Part | Package | Qty | Notes |
|-----|------|---------|-----|-------|
| U1 | nRF52832-QFAA | QFN48 | 1 | Main MCU |
| U2 | RFM95C-915S2 | SMD | 1 | LoRa module |
| U3 | FM25V10-G | SOIC-8 | 1 | 1Mbit FRAM |
| U4 | TLV70025 | SOT-23-5 | 1 | 2.5V LDO |
| Q1,Q2 | SSM6P15FU | SOT-323F | 2 | P-ch MOSFET |
| Q3,Q4 | 2SK2009 | SOT-323 | 2 | N-ch MOSFET |
| D1,D2 | BAT54S | SOT-23 | 2 | Flyback diodes |
| D3 | BAT54 | SOD-323 | 1 | Envelope detector |
| LED1 | Green | 0603 | 1 | Status |
| LED2 | Yellow | 0603 | 1 | SPI activity |
| LED3 | Blue | 0603 | 1 | BLE connection |
| Y1 | 32.768kHz | 3215 | 1 | LFXO crystal |
| L1 | 10µH | 0603 | 1 | DC-DC inductor |
| SW1 | Tactile | 6mm | 1 | OTA button |

---

## PCB Layout Notes

1. **Keep SPI traces short** - Route SCK, MOSI, MISO as short as possible
2. **Ground plane** - Solid ground plane under nRF52832 and RFM95C
3. **Antenna clearance** - Keep copper away from BLE antenna area
4. **Decoupling caps** - Place 100nF caps close to each VDD pin
5. **Crystal placement** - 32.768kHz crystal close to XL1/XL2 pins
6. **H-bridge isolation** - Keep high-current H-bridge traces away from sensitive analog
7. **Battery divider** - Use 1% resistors for accurate voltage reading
