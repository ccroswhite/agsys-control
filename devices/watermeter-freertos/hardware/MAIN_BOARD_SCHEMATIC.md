# Mag Meter Main Board - Schematic Reference

## Overview

This document provides the complete netlist, BOM, and connection details for the mag meter main board. Use this as a reference when creating the Eagle schematic.

## Bill of Materials (BOM)

### Integrated Circuits

| Ref | Part Number | Description | Package | Qty |
|-----|-------------|-------------|---------|-----|
| U1 | nRF52840-QFAA | MCU, 1MB Flash, 256KB RAM | QFN-73 (7x7mm) | 1 |
| U2 | ADS131M02IPWR | 24-bit ADC, 64kSPS, 2-ch | TSSOP-20 | 1 |
| U3 | THS4551IDGKR | Fully Differential Amp | VSSOP-8 | 1 |
| U4 | ADA4522-2ARMZ | Dual Zero-Drift Op-Amp | MSOP-8 | 1 |
| U5 | RFM95CW | LoRa Module, 915MHz | Module | 1 |
| U6 | MB85RS1MTPNF | 1Mbit FRAM (128KB) | SOIC-8 | 1 |
| U7 | LMR36006QDDAR | Buck 24-60V→5V, 0.6A (digital) | SOT-23-6 | 1 |
| U8 | LMR36006QDDAR | Buck 24-60V→5V, 0.6A (analog) | SOT-23-6 | 1 |
| U9 | AP2112K-3.3 | LDO 5V→3.3V, 600mA (digital) | SOT-23-5 | 1 |
| U10 | TPS7A2033PDBVR | LDO 5V→3.3V, 200mA (THS4551, ultra-low noise) | SOT-23-5 | 1 |
| U12 | TPS7A2033PDBVR | LDO 5V→3.3V, 200mA (ADA4522, ultra-low noise) | SOT-23-5 | 1 |
| U13 | TPS7A2033PDBVR | LDO 5V→3.3V, 200mA (ADC AVDD, ultra-low noise) | SOT-23-5 | 1 |
| U11 | MP1584EN | Buck 24-60V→12V (backlight) | SOIC-8 | 1 |

### Display

| Ref | Part Number | Description | Interface |
|-----|-------------|-------------|-----------|
| DISP1 | E28GA-T-CW250-N | 2.8" Transflective TFT, ST7789 | 4-wire SPI, 40-pin FPC |

### Connectors

| Ref | Part Number | Description | Notes |
|-----|-------------|-------------|-------|
| J1 | 8-pin header 2.54mm | Power Board Connector | To power daughter board |
| J4 | 10-pin header 1.27mm | SWD Debug | ARM Cortex debug |
| J5 | Hirose FH12-40S-0.5SH(55) | Display FPC 40-pin 0.5mm | Bottom contact |
| J6 | U.FL | Antenna | LoRa RF connector |
| J10 | Amphenol 31-10-RFX | Triaxial BNC (ELEC+) | Electrode + signal, panel mount |
| J11 | Amphenol 31-10-RFX | Triaxial BNC (ELEC-) | Electrode - signal, panel mount |
| SW1 | Tactile switch 6x6mm | BTN_UP | Navigation, active LOW |
| SW2 | Tactile switch 6x6mm | BTN_DOWN | Navigation, active LOW |
| SW3 | Tactile switch 6x6mm | BTN_LEFT | Navigation, active LOW |
| SW4 | Tactile switch 6x6mm | BTN_RIGHT | Navigation, active LOW |
| SW5 | Tactile switch 6x6mm | BTN_SELECT | Navigation, active LOW |

### Power Architecture

```
VIN (24-60V from power board J1)
    │
    ├──► U7 Buck (5V) ──► U9 LDO (3.3V) ──────────────► +3V3_D (Digital)
    │    LMR36006          AP2112K                       MCU, LoRa, FRAM, ADC DVDD
    │
    │                      ┌──► U10 LDO (3.3V) ─────────► +3V3_THS (THS4551)
    │                      │    TPS7A20 (15µVRMS)
    │                      │
    ├──► U8 Buck (5V) ─────┼──► U12 LDO (3.3V) ─────────► +3V3_ADA (ADA4522)
    │    LMR36006          │    TPS7A20 (15µVRMS)
    │    (analog)          │
    │                      └──► U13 LDO (3.3V) ─────────► +3V3_ADC (ADC AVDD)
    │                           TPS7A20 (15µVRMS)
    │
    └──► U11 Buck (12V) ────────────────────────────────► +12V_BL (Backlight)
         MP1584EN                                         Display LED anode
```

**Analog LDO Isolation:**
- Each analog stage has dedicated TPS7A20 ultra-low noise LDO (15 µVRMS)
- Prevents noise coupling between stages
- THS4551, ADA4522, and ADC AVDD each have isolated 3.3V supply

### Passive Components - Power (Digital Buck U7 + LDO U9)

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C1 | 10µF 50V | 0805 | U7 input cap |
| C2 | 22µF 10V | 0805 | U7 output cap |
| L1 | 10µH | 1210 | U7 inductor |
| C3 | 10µF | 0805 | U9 input cap |
| C4 | 10µF | 0805 | U9 output cap |

### Passive Components - Power (Analog Buck U8 + LDOs U10/U12/U13)

| Ref | Value | Package | Type | Description |
|-----|-------|---------|------|-------------|
| C5 | 10µF 50V | 0805 | X5R ceramic | U8 input cap |
| C6 | 22µF 10V | 0805 | X5R ceramic | U8 output cap |
| L2 | 10µH | 1210 | - | U8 inductor |
| C7 | 1µF | 0402 | C0G/NP0 | U10 input cap (THS4551 LDO) |
| C8 | 47µF 6.3V | 3216 (A case) | Tantalum | U10 output bulk cap |
| C8A | 100nF | 0402 | C0G/NP0 | U10 output bypass cap |
| C60 | 1µF | 0402 | C0G/NP0 | U12 input cap (ADA4522 LDO) |
| C61 | 47µF 6.3V | 3216 (A case) | Tantalum | U12 output bulk cap |
| C61A | 100nF | 0402 | C0G/NP0 | U12 output bypass cap |
| C62 | 1µF | 0402 | C0G/NP0 | U13 input cap (ADC AVDD LDO) |
| C63 | 47µF 6.3V | 3216 (A case) | Tantalum | U13 output bulk cap |
| C63A | 100nF | 0402 | C0G/NP0 | U13 output bypass cap |

**Tantalum cap recommendation:** Kemet T491A476K006AT (47µF 6.3V, 3216/A case, low ESR)
**Note:** All analog LDO bypass caps use C0G/NP0 for minimal piezoelectric noise.

### Power Indicator LEDs (Debug - DNP for Production)

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| D10 | Green LED | 0602 | +3V3_D indicator |
| D11 | Green LED | 0602 | +3V3_THS indicator |
| D12 | Green LED | 0602 | +3V3_ADA indicator |
| D13 | Green LED | 0602 | +3V3_ADC indicator |
| D14 | Green LED | 0602 | +5V_A indicator |
| D15 | Green LED | 0602 | +12V_BL indicator |
| R30 | 680Ω | 0402 | D10 current limit (2mA @ 3.3V) |
| R31 | 680Ω | 0402 | D11 current limit (2mA @ 3.3V) |
| R32 | 680Ω | 0402 | D12 current limit (2mA @ 3.3V) |
| R33 | 680Ω | 0402 | D13 current limit (2mA @ 3.3V) |
| R34 | 1.5kΩ | 0402 | D14 current limit (2mA @ 5V) |
| R35 | 5.1kΩ | 0402 | D15 current limit (2mA @ 12V) |

**Note:** Mark as DNP (Do Not Populate) for production builds. Useful for power validation during development.

### Passive Components - Power (Backlight Buck U11)

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C9 | 22µF 80V | 1206 | U11 input cap |
| C10 | 22µF 25V | 0805 | U11 output cap |
| L3 | 10µH | 1210 | U11 inductor |
| R1 | 20kΩ | 0402 | U11 feedback divider top |
| R2 | 2.2kΩ | 0402 | U11 feedback divider bottom (12V out) |

### Backlight Switch Circuit

| Ref | Part | Package | Description |
|-----|------|---------|-------------|
| Q1 | 2N7002 | SOT-23 | N-MOSFET backlight switch (Ciss ~50pF) |
| R40 | 1MΩ | 0402 | Gate pulldown (off by default, τ = 50µs) |
| R41 | 100Ω | 0402 | Gate series resistor |

```
+12V_BL (from U11) ──────────────────► J5 LED+ (backlight anode)

J5 LED- (backlight cathode) ──┬──── Q1 Drain
                              │
                              └──── Q1 Source ──── GND

DISP_BL_EN (P0.14) ── R41 ──┬──── Q1 Gate
                            │
                           R40
                            │
                           GND
```

**Note:** GPIO HIGH = backlight ON, GPIO LOW = backlight OFF

### Passive Components - Analog Front-End

**THS4551 Decoupling (on +3V3_THS rail):**

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C70 | 10µF | 0805 | THS4551 VDD bulk cap (close to pin) |
| C71 | 100nF | 0402 | THS4551 VDD bypass cap (closest to pin) |

**ADA4522 Decoupling (on +3V3_ADA rail):**

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C72 | 10µF | 0805 | ADA4522 V+ bulk cap (close to pin) |
| C73 | 100nF | 0402 | ADA4522 V+ bypass cap (closest to pin) |

**ADS131M02 Decoupling:**

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C74 | 10µF | 0805 | ADS131M02 AVDD bulk cap (on +3V3_ADC) |
| C75 | 100nF | 0402 | ADS131M02 AVDD bypass cap (closest to pin) |
| C76 | 10µF | 0805 | ADS131M02 DVDD bulk cap (on +3V3_D) |
| C77 | 100nF | 0402 | ADS131M02 DVDD bypass cap (closest to pin) |
| C78 | 100nF | 0402 | ADS131M02 CAP pin to DGND |

**RC Filter (between THS4551 and ADC):**

| Ref | Value | Package | Type | Description |
|-----|-------|---------|------|-------------|
| R10 | 47Ω | 0402 | Thin film 1% | RC filter, AIN0P |
| R11 | 47Ω | 0402 | Thin film 1% | RC filter, AIN0N |
| C20 | 100pF | 0402 | C0G/NP0 | RC filter, AIN0P (low microphonics) |
| C21 | 100pF | 0402 | C0G/NP0 | RC filter, AIN0N (low microphonics) |

**Note:** C0G/NP0 (Class 1) ceramic has minimal piezoelectric effect compared to X5R/X7R.

### Passive Components - THS4551 Gain Setting

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| R20 | 1kΩ | 0402 | Rg (gain resistor) |
| R21 | 10kΩ | 0402 | Rf+ (feedback, gain = 10) |
| R22 | 10kΩ | 0402 | Rf- (feedback, gain = 10) |
| R23 | 10kΩ | 0402 | Input resistor + |
| R24 | 10kΩ | 0402 | Input resistor - |

### Passive Components - MCU

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C30 | 100nF | 0402 | nRF52840 VDDH decoupling |
| C31 | 100nF | 0402 | nRF52840 VDD decoupling |
| C32 | 100nF | 0402 | nRF52840 VDD decoupling |
| C33 | 4.7µF | 0402 | nRF52840 DCC |
| C34 | 1µF | 0402 | nRF52840 DCC |
| C35 | 100pF | 0402 | nRF52840 DCC |
| L4 | 10nH | 0402 | nRF52840 DCC inductor |
| Y1 | 32.768kHz | 3215 | RTC crystal (low-power) |
| C36 | 6.8pF | 0402 | 32.768kHz crystal load cap |
| C37 | 6.8pF | 0402 | 32.768kHz crystal load cap |
| Y2 | 32MHz | 3215 | HFXO crystal (BLE timing) |
| C38 | 12pF | 0402 | 32MHz crystal load cap |
| C39 | 12pF | 0402 | 32MHz crystal load cap |

**Note:** nRF52840 requires 32MHz crystal for BLE operation. 32.768kHz is for low-power RTC.

### Passive Components - LoRa

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C40 | 100nF | 0402 | RFM95C decoupling |
| C41 | 10µF | 0805 | RFM95C decoupling |

### Passive Components - FRAM + Flash (Standard Memory Bus)

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| C50 | 100nF | 0402 | MB85RS1MT (FRAM) decoupling |
| C51 | 100nF | 0402 | W25Q16 (Flash) decoupling |

### Passive Components - Navigation Buttons

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| R50 | 10K | 0402 | BTN_UP pull-up (internal pull-up also available) |
| R51 | 10K | 0402 | BTN_DOWN pull-up |
| R52 | 10K | 0402 | BTN_LEFT pull-up |
| R53 | 10K | 0402 | BTN_RIGHT pull-up |
| R54 | 10K | 0402 | BTN_SELECT pull-up |
| C52 | 100nF | 0402 | BTN_UP debounce |
| C53 | 100nF | 0402 | BTN_DOWN debounce |
| C54 | 100nF | 0402 | BTN_LEFT debounce |
| C55 | 100nF | 0402 | BTN_RIGHT debounce |
| C56 | 100nF | 0402 | BTN_SELECT debounce |
| D20 | PESD5V0S1BL | SOD323 | BTN_UP ESD protection |
| D21 | PESD5V0S1BL | SOD323 | BTN_DOWN ESD protection |
| D22 | PESD5V0S1BL | SOD323 | BTN_LEFT ESD protection |
| D23 | PESD5V0S1BL | SOD323 | BTN_RIGHT ESD protection |
| D24 | PESD5V0S1BL | SOD323 | BTN_SELECT ESD protection |

**Note:** External pull-ups optional if using nRF52840 internal pull-ups. Debounce caps recommended for mechanical switches.

### Passive Components - Status LEDs

| Ref | Value | Package | Description |
|-----|-------|---------|-------------|
| LED1 | Green | 0603 | BLE status (P1.07) |
| LED2 | Blue | 0603 | LoRa status (P1.08) |
| R55 | 1K | 0402 | LED1 current limit |
| R56 | 1K | 0402 | LED2 current limit |

### TVS Diodes (Optional - Footprints Only)

| Ref | Part Number | Package | Description |
|-----|-------------|---------|-------------|
| D1 | TPD2E001DRLR | SOT-5X3 | Electrode input TVS (optional) |
| D2 | TPD2E001DRLR | SOT-5X3 | Electrode input TVS (optional) |

---

## Net List

### Power Rails

| Net Name | Source | Destinations |
|----------|--------|--------------|
| VIN | J1 pin 1 | U7 VIN, U8 VIN, U11 VIN |
| +5V_D | U7 VOUT | U9 VIN (digital LDO input) |
| +5V_A | U8 VOUT | U10 VIN, U12 VIN, U13 VIN (analog LDO inputs) |
| +3V3_D | U9 VOUT | U1 VDD, U5 VCC, U6 VCC, U2 DVDD |
| +3V3_THS | U10 VOUT | U3 VDD (THS4551 only) |
| +3V3_ADA | U12 VOUT | U4 VDD (ADA4522 only) |
| +3V3_ADC | U13 VOUT | U2 AVDD (ADC analog supply only) |
| +12V_BL | U11 VOUT | J5 (backlight anode) |
| AGND | Star point | U2 AGND, U3 GND, U4 GND, U8 GND, U10 GND, U12 GND, U13 GND |
| DGND | Star point | U1 GND, U2 DGND, U5 GND, U6 GND, U7 GND, U9 GND |
| GND | J1 pin 2 | Star ground point (AGND and DGND connect here only) |

### Electrode Inputs

| Net Name | Source | Destination |
|----------|--------|-------------|
| ELEC_P | J2 pin 1 | U4A+ (ADA4522 ch A input), U3 +IN |
| ELEC_N | J2 pin 3 | U4B+ (ADA4522 ch B input), U3 -IN |
| GUARD_P | U4A OUT | J2 pin 2 (guard shield +) |
| GUARD_N | U4B OUT | J2 pin 4 (guard shield -) |

### THS4551 to ADS131M02

| Net Name | Source | Destination |
|----------|--------|-------------|
| FDA_OUTP | U3 OUT+ | R10 (47Ω) → C20 → AGND, → U2 AIN0P |
| FDA_OUTN | U3 OUT- | R11 (47Ω) → C21 → AGND, → U2 AIN0N |
| FDA_VOCM | U3 VOCM | Bias network (mid-supply) |

### ADS131M02 to MCU (SPI)

| Net Name | U2 Pin | U1 Pin | Description |
|----------|--------|--------|-------------|
| ADC_SCLK | 14 (SCLK) | P0.05 | SPI clock |
| ADC_MOSI | 16 (DIN) | P0.04 | SPI data in |
| ADC_MISO | 15 (DOUT) | P0.03 | SPI data out |
| ADC_CS | 12 (CS) | P0.02 | Chip select |
| ADC_DRDY | 13 (DRDY) | P0.31 | Data ready (interrupt, moved from P0.21) |
| ADC_SYNC | 11 (SYNC/RST) | P0.20 | Sync/Reset |

### Display (SPI)

| Net Name | DISP1 Pin | U1 Pin | Description |
|----------|-----------|--------|-------------|
| DISP_SCLK | 34 (DOTCLK) | P0.19 | SPI clock |
| DISP_MOSI | 33 (SDA) | P0.18 | SPI data |
| DISP_CS | 10 (CSX) | P0.17 | Chip select |
| DISP_DC | 11 (DCX) | P0.30 | Data/Command (moved from P0.16) |
| DISP_RST | 38 (RESX) | P0.15 | Reset |
| DISP_BL_EN | - | P0.14 | Backlight enable (to U9 EN) |

### LoRa (SPI)

| Net Name | U5 Pin | U1 Pin | Description |
|----------|--------|--------|-------------|
| LORA_SCLK | SCK | P0.13 | SPI clock |
| LORA_MOSI | MOSI | P0.12 | SPI data in |
| LORA_MISO | MISO | P0.11 | SPI data out |
| LORA_CS | NSS | P0.10 | Chip select |
| LORA_RST | RST | P0.09 | Reset |
| LORA_DIO0 | DIO0 | P0.08 | Interrupt |

### FRAM + Flash (SPI) - STANDARD PINS

| Net Name | U6 Pin | U1 Pin | Description |
|----------|--------|--------|-------------|
| MEM_SCLK | 6 (SCK) | P0.26 | SPI clock (standard) |
| MEM_MOSI | 5 (SI) | P0.25 | SPI data in (standard) |
| MEM_MISO | 2 (SO) | P0.24 | SPI data out (standard) |
| FRAM_CS | 1 (CS) | P0.23 | Chip select (standard) |
| FLASH_CS | - | P0.22 | W25Q16 chip select (standard) |

### Power Board Connector (J1)

| Pin | Net Name | Description |
|-----|----------|-------------|
| 1 | VIN | Power input (24/48/60V from power board) |
| 2 | GND | Ground |
| 3 | +3V3 | 3.3V to power board (for tier ID) |
| 4 | TIER_ID | Analog tier identification |
| 5 | COIL_GATE | PWM signal to MOSFET gate |
| 6 | I_SENSE | Current sense voltage |
| 7 | COIL+ | Coil drive output |
| 8 | COIL- | Coil return |

### Coil Control

| Net Name | Source | Destination |
|----------|--------|-------------|
| COIL_GATE | U1 P1.00 | J1 pin 5 (to power board MOSFET) |
| I_SENSE | J1 pin 6 | U2 AIN1P (second ADC channel) |
| I_SENSE_GND | J1 pin 2 | U2 AIN1N |

### Debug (SWD)

| Net Name | J4 Pin | U1 Pin | Description |
|----------|--------|--------|-------------|
| SWDIO | 2 | SWDIO | Debug data |
| SWCLK | 4 | SWDCLK | Debug clock |
| SWO | 6 | P1.00 | Trace output (optional) |
| nRESET | 10 | P0.18 | Reset |
| VCC | 1 | +3V3 | Power |
| GND | 3,5,9 | GND | Ground |

---

## Pin Assignments Summary

### nRF52840 GPIO Usage

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| P0.02 | ADC_CS | Output | |
| P0.03 | ADC_MISO | Input | |
| P0.04 | ADC_MOSI | Output | |
| P0.05 | ADC_SCLK | Output | |
| P0.08 | LORA_DIO0 | Input | Interrupt |
| P0.09 | LORA_RST | Output | |
| P0.10 | LORA_CS | Output | |
| P0.11 | LORA_MISO | Input | |
| P0.12 | LORA_MOSI | Output | |
| P0.13 | LORA_SCLK | Output | |
| P0.14 | DISP_BL_EN | Output | Backlight PWM |
| P0.15 | DISP_RST | Output | |
| P0.30 | DISP_DC | Output | Data/Command (moved from P0.16) |
| P0.31 | ADC_DRDY | Input | Data ready interrupt |
| P0.17 | DISP_CS | Output | |
| P0.18 | DISP_MOSI | Output | |
| P0.19 | DISP_SCLK | Output | |
| P0.20 | ADC_SYNC | Output | |
| P0.22 | FLASH_CS | Output | Standard memory pin |
| P0.23 | FRAM_CS | Output | Standard memory pin |
| P0.24 | MEM_MISO | Input | Standard memory pin |
| P0.25 | MEM_MOSI | Output | Standard memory pin |
| P0.26 | MEM_SCLK | Output | Standard memory pin |
| P1.00 | COIL_GATE | Output | PWM |
| P1.01 | TIER_ID | Analog In | ADC for tier detect |
| P1.02 | BTN_UP | Input | Navigation button, active LOW |
| P1.03 | BTN_DOWN | Input | Navigation button, active LOW |
| P1.04 | BTN_LEFT | Input | Navigation button, active LOW |
| P1.05 | BTN_RIGHT | Input | Navigation button, active LOW |
| P1.06 | BTN_SELECT | Input | Navigation button, active LOW |

---

## Schematic Sheets

Organize the Eagle schematic into these sheets:

1. **Power** - LDOs, buck converter, power connector
2. **MCU** - nRF52840, crystal, decoupling, SWD
3. **Analog Front-End** - THS4551, ADA4522, RC filters, TVS
4. **ADC** - ADS131M02, decoupling, connections
5. **Peripherals** - LoRa, FRAM, display connector
6. **Connectors** - All board connectors

---

## PCB Fabrication Requirements

### Board Finish
- **ENIG (Electroless Nickel Immersion Gold)** required
- Gold thickness: 2-4 µin minimum
- Provides flat surface for fine-pitch components and prevents oxidation on exposed copper

### Signal Path Solder Mask Exclusion
- **Remove solder mask** from entire analog signal path area
- Includes: J2 (electrode connector) → ADA4522 → THS4551 → RC filter → ADS131M02 analog inputs
- Exposed copper with ENIG finish reduces parasitic capacitance and leakage currents
- Define keep-out area in Eagle using tStop layer

### Layer Stack (4-layer recommended)
1. **Top**: Signal, components
2. **Inner 1**: GND (solid pour, split AGND/DGND)
3. **Inner 2**: Power planes (+3V3_D, +3V3_A rails)
4. **Bottom**: Signal, components

---

## Triaxial Electrode Connectors

### Connector Selection

| Ref | Part Number | Description | Notes |
|-----|-------------|-------------|-------|
| J10 | Amphenol 31-10-RFX | BNC triaxial, panel mount | Electrode + signal |
| J11 | Amphenol 31-10-RFX | BNC triaxial, panel mount | Electrode - signal |

**Alternative (if BNC triax unavailable):**
- Use standard BNC for signal + separate SMA for guard
- Or use Lemo triaxial connectors (higher cost, better shielding)

### Triaxial Pinout

```
        ┌─────────────────┐
        │   Outer Shield  │ ──► Earth GND (chassis)
        │  ┌───────────┐  │
        │  │   Guard   │  │ ──► GUARD_P or GUARD_N (driven by ADA4522)
        │  │  ┌─────┐  │  │
        │  │  │Signal│  │  │ ──► ELEC_P or ELEC_N (to THS4551 input)
        │  │  └─────┘  │  │
        │  └───────────┘  │
        └─────────────────┘
```

### Connector Wiring

| Connector | Center (Signal) | Inner Shield (Guard) | Outer Shield |
|-----------|-----------------|----------------------|--------------|
| J10 (ELEC+) | ELEC_P | GUARD_P | Earth GND |
| J11 (ELEC-) | ELEC_N | GUARD_N | Earth GND |

---

## Notes

1. **Ground planes**: Use separate AGND and DGND pours, connect at star point only
2. **Guard ring**: Route guard traces around analog input traces, driven by ADA4522
3. **Decoupling**: Place all decoupling caps as close to IC pins as possible
4. **Crystal**: Keep traces short, guard with ground pour
5. **RF**: Keep LoRa antenna trace 50Ω impedance controlled
6. **ENIG finish**: Required for exposed copper in signal path
7. **Solder mask**: Exclude from analog signal path area for reduced leakage
