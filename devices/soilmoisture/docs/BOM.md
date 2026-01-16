# Soil Moisture Sensor Bill of Materials

## Main Board

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | MCU | nRF52832-QFAA | 1 | $2.50 | $2.50 | ARM Cortex-M4F, BLE 5.0 |
| U2 | LoRa Module | RFM95C-915S2 | 1 | $6.00 | $6.00 | 915 MHz, SPI |
| U3 | FRAM | MB85RS1MTPNF | 1 | $2.50 | $2.50 | 128KB SPI, calibration + logs |
| U4 | LDO | TPS7A0225PDBVR | 1 | $0.50 | $0.50 | 2.5V for MCU (25nA Iq) |
| U5 | Flash | W25Q16JVSSIQ | 1 | $0.50 | $0.50 | 2MB SPI, OTA firmware |
| Q1 | P-FET | SI2301CDS | 1 | $0.15 | $0.15 | Probe power switch |
| Q2 | P-FET | SI2301CDS | 1 | $0.15 | $0.15 | Reverse polarity protection |
| Y1 | Crystal | 32MHz 3215 | 1 | $0.25 | $0.25 | For nRF52832 |
| Y2 | Crystal | 32.768kHz 1610 | 1 | $0.20 | $0.20 | Low-power RTC |
| L1 | Inductor | 10nH 0402 | 1 | $0.05 | $0.05 | nRF52832 DC-DC |
| L2 | Inductor | 5.6nH 0402 | 1 | $0.05 | $0.05 | Antenna matching |
| LED1 | LED Green | 0603 | 1 | $0.02 | $0.02 | Status indicator |
| SW1 | Tactile Switch | 6x6mm | 1 | $0.05 | $0.05 | BLE pairing button |
| ANT1 | Antenna | 915MHz wire | 1 | $0.50 | $0.50 | Or PCB antenna |
| BT1 | Battery Holder | 21700 | 1 | $0.80 | $0.80 | Single cell |
| J1-J4 | Probe Connector | JST-XH 3-pin | 4 | $0.15 | $0.60 | VCC, GND, FREQ |
| J5 | Battery Connector | JST-PH 2-pin | 1 | $0.10 | $0.10 | Battery connection |
| D1-D4 | TVS Diode | PESD5V0S1BL | 4 | $0.05 | $0.20 | Probe ESD protection |
| C1,C2 | Capacitor | 10uF 0805 | 2 | $0.03 | $0.06 | LDO in/out |
| C3,C4 | Capacitor | 100nF 0402 | 2 | $0.01 | $0.02 | Button debounce, ADC filter |
| C10-C15 | Capacitor | Various 0402 | 6 | $0.01 | $0.06 | nRF52832 decoupling |
| C20,C21 | Capacitor | 12pF 0402 | 2 | $0.01 | $0.02 | 32MHz crystal load |
| C22,C23 | Capacitor | 6.8pF 0402 | 2 | $0.01 | $0.02 | 32.768kHz crystal load |
| C30 | Capacitor | 100nF 0402 | 1 | $0.01 | $0.01 | LoRa decoupling |
| C31,C32 | Capacitor | 1.5pF/1.2pF 0402 | 2 | $0.01 | $0.02 | Antenna matching |
| C40,C41 | Capacitor | 100nF 0402 | 2 | $0.01 | $0.02 | FRAM/Flash decoupling |
| R1,R2 | Resistor | 1M 0402 | 2 | $0.01 | $0.02 | Battery voltage divider |
| R3 | Resistor | 330R 0402 | 1 | $0.01 | $0.01 | LED current limit |
| R6-R8 | Resistor | 10K 0402 | 3 | $0.01 | $0.03 | Pull-ups |
| R10-R13 | Resistor | 100R 0402 | 4 | $0.01 | $0.04 | Probe input filter |
| - | PCB | 2-layer | 1 | $1.50 | $1.50 | |
| | | | | **Total** | **~$19** | |

## Probe Module (x4)

Each probe contains an oscillator circuit that outputs a frequency proportional to soil capacitance.

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | Schmitt Trigger | 74LVC1G17 | 1 | $0.10 | $0.10 | Oscillator buffer |
| C1 | Capacitor | 100pF NPO | 1 | $0.02 | $0.02 | Timing cap |
| R1 | Resistor | 100K 1% | 1 | $0.01 | $0.01 | Timing resistor |
| - | PCB | Probe shape | 1 | $0.30 | $0.30 | Conformal coated |
| - | Cable | 3-wire | 1 | $0.50 | $0.50 | Length varies by depth |
| | | | | **Total** | **~$1** | per probe |

## Probe Depths and Cable Lengths

| Probe | Depth | Cable Length | Purpose |
|-------|-------|--------------|---------|
| 1 | 1 ft (30cm) | 0.5m | Surface moisture |
| 2 | 3 ft (90cm) | 1.2m | Root zone upper |
| 3 | 5 ft (150cm) | 1.8m | Root zone lower |
| 4 | 7 ft (210cm) | 2.5m | Deep soil |

## Complete Sensor Assembly Cost

| Item | Cost |
|------|------|
| Main Board | $17 |
| Probes (4x) | $4 |
| 21700 Battery (5000mAh) | $8 |
| Enclosure (IP67) | $5 |
| Probe installation tube | $10 |
| **Total** | **~$44** |

## Pin Assignments (Standard Memory Bus)

| Signal | Pin | Description |
|--------|-----|-------------|
| SPI_LORA_SCK | P0.14 | LoRa SPI Clock |
| SPI_LORA_MOSI | P0.13 | LoRa SPI MOSI |
| SPI_LORA_MISO | P0.12 | LoRa SPI MISO |
| SPI_CS_LORA | P0.11 | LoRa Chip Select |
| MEM_SCK | P0.26 | Memory SPI Clock (standard) |
| MEM_MOSI | P0.25 | Memory SPI MOSI (standard) |
| MEM_MISO | P0.24 | Memory SPI MISO (standard) |
| FRAM_CS | P0.23 | FRAM Chip Select (standard) |
| FLASH_CS | P0.22 | Flash Chip Select (standard) |
| LORA_RESET | P0.30 | LoRa Reset |
| LORA_DIO0 | P0.31 | LoRa Interrupt |
| PROBE_PWR | P0.16 | Probe power enable |
| PROBE1_FREQ | P0.03 | Probe 1 frequency input |
| PROBE2_FREQ | P0.04 | Probe 2 frequency input |
| PROBE3_FREQ | P0.05 | Probe 3 frequency input |
| PROBE4_FREQ | P0.28 | Probe 4 frequency input |
| LED_STATUS | P0.17 | Status LED |
| PAIRING_BTN | P0.07 | BLE pairing button |

## Power Budget

| State | Current | Duration | Notes |
|-------|---------|----------|-------|
| Deep Sleep | 2 µA | 2 hours | RTC running |
| Probe Measurement | 5 mA | 500 ms | All 4 probes |
| LoRa TX | 120 mA | 100 ms | SF10, 20 dBm |
| LoRa RX | 12 mA | 200 ms | Waiting for ACK |
| **Average** | ~5 µA | | 2-hour cycle |

**Battery Life Estimate:** 21700 5000mAh → ~10 years (theoretical), ~5 years (practical with self-discharge)
