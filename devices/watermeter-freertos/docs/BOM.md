# Water Meter Bill of Materials

## Main Board

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | MCU | nRF52840-QIAA | 1 | $4.00 | $4.00 | ARM Cortex-M4F, BLE 5.0, USB |
| U2 | ADC | ADS131M02 | 1 | $8.00 | $8.00 | 24-bit delta-sigma, 2-ch |
| U3 | FDA | THS4551 | 1 | $2.50 | $2.50 | Fully differential amplifier |
| U4 | Op-Amp | ADA4522-2 | 1 | $3.00 | $3.00 | Zero-drift, dual |
| U5 | LoRa Module | RFM95C-915S2 | 1 | $6.00 | $6.00 | 915 MHz, SPI |
| U6 | FRAM | MB85RS1MTPNF | 1 | $2.50 | $2.50 | 128KB SPI, calibration + logs |
| U7 | Flash | W25Q16JVSSIQ | 1 | $0.50 | $0.50 | 2MB SPI, OTA firmware |
| U8 | Display Driver | ST7789V | - | - | - | Integrated in display |
| U9 | LED Driver | TPS61165 | 1 | $0.80 | $0.80 | Backlight boost |
| U10 | LDO Analog | TPS7A20 | 1 | $0.60 | $0.60 | 3.0V ultra-low noise |
| U11 | LDO Digital | MCP1700-3302E | 1 | $0.30 | $0.30 | 3.3V 250mA |
| Y1 | Crystal | 32.768kHz 3215 | 1 | $0.20 | $0.20 | nRF52840 LFXO |
| Y2 | Crystal | 32MHz 3215 | 1 | $0.25 | $0.25 | nRF52840 HFXO (BLE) |
| DISP1 | Display | ST7789 2.8" TFT | 1 | $8.00 | $8.00 | 240x320, SPI |
| ANT1 | Antenna | 915MHz wire | 1 | $0.50 | $0.50 | Or PCB antenna |
| SW1-5 | Navigation | 6x6mm tactile | 5 | $0.05 | $0.25 | Up/Down/Left/Right/Select |
| LED1 | LED Green | 0603 | 1 | $0.02 | $0.02 | BLE status |
| LED2 | LED Blue | 0603 | 1 | $0.02 | $0.02 | LoRa status |
| J1 | Power Connector | Phoenix XPC 5-pin | 1 | $1.50 | $1.50 | To power board |
| J2 | Coil Connector | Phoenix XPC 4-pin | 1 | $1.00 | $1.00 | Electrode signals |
| L4 | Inductor | 10nH 0402 | 1 | $0.05 | $0.05 | nRF52840 DC-DC |
| D20-D24 | TVS | PESD5V0S1BL | 5 | $0.05 | $0.25 | Button ESD protection |
| R10,R11 | Resistor | 47R 0402 thin film | 2 | $0.02 | $0.04 | ADC RC filter |
| R20-R24 | Resistor | 10K 0402 | 5 | $0.01 | $0.05 | Gain setting |
| R50-R54 | Resistor | 10K 0402 | 5 | $0.01 | $0.05 | Button pull-ups (optional) |
| R55,R56 | Resistor | 1K 0402 | 2 | $0.01 | $0.02 | LED current limit |
| C20,C21 | Capacitor | 100pF C0G 0402 | 2 | $0.02 | $0.04 | ADC RC filter (low microphonics) |
| C30-C35 | Capacitor | Various 0402 | 6 | $0.01 | $0.06 | nRF52840 decoupling |
| C36,C37 | Capacitor | 6.8pF 0402 | 2 | $0.01 | $0.02 | 32.768kHz crystal load |
| C38,C39 | Capacitor | 12pF 0402 | 2 | $0.01 | $0.02 | 32MHz crystal load |
| C40,C41 | Capacitor | 100nF/10uF | 2 | $0.02 | $0.04 | LoRa decoupling |
| C50,C51 | Capacitor | 100nF 0402 | 2 | $0.01 | $0.02 | FRAM/Flash decoupling |
| C52-C56 | Capacitor | 100nF 0402 | 5 | $0.01 | $0.05 | Button debounce |
| C70-C78 | Capacitor | Various | 9 | $0.02 | $0.18 | Analog stage decoupling |
| - | PCB | 4-layer ENIG | 1 | $5.00 | $5.00 | Controlled impedance, exposed copper |
| | | | | **Total** | **~$48** | |

## Power Board (Tier-Specific)

### Tier 1: 24V DC Input (1.5" - 2" pipes)

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | Buck | TPS54202DDCR | 1 | $0.50 | $0.50 | 24V to 5V |
| U2 | LDO | MCP1700-3302E | 1 | $0.30 | $0.30 | 5V to 3.3V |
| Q1 | N-FET | IRLML6244 | 1 | $0.20 | $0.20 | Coil driver |
| L1 | Inductor | 10µH 2A | 1 | $0.15 | $0.15 | For buck |
| - | Passives | Various | - | - | $1.00 | |
| - | PCB | 2-layer | 1 | $0.50 | $0.50 | |
| | | | | **Total** | **~$3** | |

### Tier 2: 48V DC Input (3" - 4" pipes)

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | Buck | LM5164 | 1 | $2.00 | $2.00 | 48V to 5V |
| U2 | LDO | MCP1700-3302E | 1 | $0.30 | $0.30 | 5V to 3.3V |
| Q1 | N-FET | IRLML6244 | 1 | $0.20 | $0.20 | Coil driver |
| L1 | Inductor | 22µH 1A | 1 | $0.20 | $0.20 | For buck |
| - | Passives | Various | - | - | $1.50 | |
| - | PCB | 2-layer | 1 | $0.50 | $0.50 | |
| | | | | **Total** | **~$5** | |

### Tier 3: 120/240V AC Input (4" - 6" pipes)

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | AC-DC | HLK-PM03 | 1 | $3.50 | $3.50 | 120/240V to 3.3V 1A |
| Q1 | N-FET | IRLML6244 | 1 | $0.20 | $0.20 | Coil driver |
| F1 | Fuse | 250mA slow | 1 | $0.10 | $0.10 | AC protection |
| MOV1 | Varistor | 275V | 1 | $0.30 | $0.30 | Surge protection |
| - | Passives | Various | - | - | $1.00 | |
| - | PCB | 2-layer | 1 | $0.50 | $0.50 | Creepage/clearance |
| | | | | **Total** | **~$6** | |

## Coil Assembly

| Item | Description | Est. Price |
|------|-------------|------------|
| Coil former | 3D printed or machined | $2-5 |
| Magnet wire | 26 AWG enameled | $1 |
| Electrodes | 316L stainless steel | $5-10 |
| Pipe fitting | Schedule 80 PVC | $5-15 |
| Potting compound | Epoxy | $2 |
| **Total** | | **$15-33** |

## Complete Meter Assembly Cost

| Configuration | Main Board | Power Board | Coil | Enclosure | Total |
|---------------|------------|-------------|------|-----------|-------|
| Tier 1 (24V, 1.5-2") | $46 | $3 | $15 | $10 | **~$74** |
| Tier 2 (48V, 3-4") | $46 | $5 | $25 | $15 | **~$91** |
| Tier 3 (AC, 4-6") | $46 | $6 | $33 | $20 | **~$105** |

## Pin Assignments (Standard Memory Bus)

| Signal | Pin | Description |
|--------|-----|-------------|
| SPI0_SCK (ADC) | P0.05 | ADC SPI Clock |
| SPI0_MOSI (ADC) | P0.04 | ADC SPI MOSI |
| SPI0_MISO (ADC) | P0.03 | ADC SPI MISO |
| ADC_CS | P0.02 | ADC Chip Select |
| ADC_DRDY | P0.31 | ADC Data Ready |
| ADC_SYNC | P0.20 | ADC Sync/Reset |
| SPI1_SCK (Display) | P0.19 | Display SPI Clock |
| SPI1_MOSI (Display) | P0.18 | Display SPI MOSI |
| DISP_CS | P0.17 | Display Chip Select |
| DISP_DC | P0.30 | Display Data/Command |
| DISP_RST | P0.15 | Display Reset |
| DISP_BL | P0.14 | Backlight PWM |
| SPI2_SCK (LoRa) | P0.13 | LoRa SPI Clock |
| SPI2_MOSI (LoRa) | P0.12 | LoRa SPI MOSI |
| SPI2_MISO (LoRa) | P0.11 | LoRa SPI MISO |
| LORA_CS | P0.10 | LoRa Chip Select |
| LORA_RST | P0.09 | LoRa Reset |
| LORA_DIO0 | P0.08 | LoRa Interrupt |
| MEM_SCK | P0.26 | Memory SPI Clock (standard) |
| MEM_MOSI | P0.25 | Memory SPI MOSI (standard) |
| MEM_MISO | P0.24 | Memory SPI MISO (standard) |
| FRAM_CS | P0.23 | FRAM Chip Select (standard) |
| FLASH_CS | P0.22 | Flash Chip Select (standard) |
| COIL_GATE | P1.00 | Coil driver PWM |
| TIER_ID | P1.01 | Power board tier detect |
| BTN_UP | P1.02 | Navigation button |
| BTN_DOWN | P1.03 | Navigation button |
| BTN_LEFT | P1.04 | Navigation button |
| BTN_RIGHT | P1.05 | Navigation button |
| BTN_SELECT | P1.06 | Navigation button |
| LED_BLE | P1.07 | BLE status LED |
| LED_LORA | P1.08 | LoRa status LED |

## Power Budget (Tier 1 - 24V)

| State | Current (3.3V) | Notes |
|-------|----------------|-------|
| Idle (display off) | 15 mA | ADC sampling, MCU active |
| Display on | 45 mA | Backlight at 50% |
| LoRa TX | 120 mA | 20 dBm, 100ms |
| Coil excitation | 50 mA | 1-2 kHz square wave |
| **Typical** | ~60 mA | Display on, measuring |

**Power Consumption:** ~200 mW typical (at 3.3V)
