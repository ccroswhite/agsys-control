# Valve Controller Bill of Materials

## Pin Assignments (Standard Memory Bus)

| Signal | Pin | Description |
|--------|-----|-------------|
| PERIPH_SCK | P0.27 | Peripheral SPI Clock (CAN + LoRa) |
| PERIPH_MOSI | P0.28 | Peripheral SPI MOSI |
| PERIPH_MISO | P0.29 | Peripheral SPI MISO |
| CAN_CS | P0.30 | MCP2515 Chip Select |
| LORA_CS | P0.31 | RFM95C Chip Select |
| MEM_SCK | P0.26 | Memory SPI Clock (standard) |
| MEM_MOSI | P0.25 | Memory SPI MOSI (standard) |
| MEM_MISO | P0.24 | Memory SPI MISO (standard) |
| FRAM_CS | P0.23 | FRAM Chip Select (standard) |
| FLASH_CS | P0.22 | Flash Chip Select (standard) |
| CAN_INT | P0.14 | MCP2515 Interrupt |
| LORA_DIO0 | P0.15 | LoRa Interrupt |
| LORA_RST | P0.16 | LoRa Reset |
| I2C_SDA | P0.02 | RTC I2C Data |
| I2C_SCL | P0.03 | RTC I2C Clock |

---

## Valve Controller Board

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | MCU | nRF52832-QFAA | 1 | $2.50 | $2.50 | ARM Cortex-M4F, BLE |
| U2 | LoRa Module | RFM95C-915S2 | 1 | $6.00 | $6.00 | 915 MHz |
| U3 | CAN Controller | MCP2515-I/SO | 1 | $1.00 | $1.00 | SPI interface |
| U4 | CAN Transceiver | SN65HVD230DR | 1 | $0.50 | $0.50 | 3.3V compatible |
| U5 | FRAM | MB85RS1MTPNF | 1 | $2.50 | $2.50 | 128KB SPI |
| U6 | Flash | W25Q16JVSSIQ | 1 | $0.50 | $0.50 | 2MB SPI, OTA |
| U7 | RTC | RV-3028-C7 | 1 | $2.00 | $2.00 | Ultra-low power |
| U8 | LDO | AP2112K-3.3 | 1 | $0.25 | $0.25 | 3.3V for peripherals |
| U9 | LDO | TLV73325PDBVR | 1 | $0.30 | $0.30 | 2.5V for MCU |
| Q1 | P-FET | SI2301CDS | 1 | $0.15 | $0.15 | Reverse polarity protection |
| Y1 | Crystal | 32MHz 3215 | 1 | $0.25 | $0.25 | nRF52832 HFXO |
| Y2 | Crystal | 32.768kHz 2012 | 1 | $0.20 | $0.20 | nRF52832 LFXO |
| Y3 | Crystal | 16MHz HC49 | 1 | $0.15 | $0.15 | MCP2515 |
| L1 | Inductor | 10nH 0402 | 1 | $0.05 | $0.05 | nRF52832 DC-DC |
| L2 | Inductor | 5.6nH 0402 | 1 | $0.05 | $0.05 | Antenna matching |
| BT1 | Battery Holder | CR2032 | 1 | $0.20 | $0.20 | RTC backup |
| LED1-3 | LED | 0603 GYR | 3 | $0.02 | $0.06 | Status indicators |
| D1 | TVS | PESD1CAN | 1 | $0.15 | $0.15 | CAN bus ESD |
| D2 | TVS | SMBJ28A | 1 | $0.15 | $0.15 | 24V input protection |
| D3,D4 | TVS | PESD5V0S1BL | 2 | $0.05 | $0.10 | Button/power fail ESD |
| F1 | PTC Fuse | 500mA 1206 | 1 | $0.10 | $0.10 | Input protection |
| J1,J2 | CAN Connector | M12 4-pin | 2 | $6.00 | $12.00 | A-coded |
| J3 | Power Connector | Phoenix XPC 3-pin | 1 | $1.00 | $1.00 | 24V + GND + PF |
| SW1 | Button | 6x6mm tactile | 1 | $0.05 | $0.05 | Pairing |
| ANT1 | Antenna | 915MHz wire | 1 | $0.50 | $0.50 | Or PCB antenna |
| R1-R3 | Resistor | 1K 0402 | 3 | $0.01 | $0.03 | LED current limit |
| R4-R6 | Resistor | 10K 0402 | 3 | $0.01 | $0.03 | Pull-ups |
| R10 | Resistor | 120R 0603 | 1 | $0.01 | $0.01 | CAN termination |
| R20,R21 | Resistor | 4.7K 0402 | 2 | $0.01 | $0.02 | I2C pull-ups |
| R22 | Resistor | 10K 0402 | 1 | $0.01 | $0.01 | P-FET gate |
| C1 | Capacitor | 10uF/50V 0805 | 1 | $0.05 | $0.05 | LDO input |
| C2 | Capacitor | 10uF 0805 | 1 | $0.03 | $0.03 | LDO output |
| C3,C4 | Capacitor | 12pF 0402 | 2 | $0.01 | $0.02 | 32MHz crystal load |
| C5,C6 | Capacitor | 6.8pF 0402 | 2 | $0.01 | $0.02 | 32.768kHz crystal load |
| C10,C11 | Capacitor | 22pF 0402 | 2 | $0.01 | $0.02 | 16MHz crystal load |
| C20-C25 | Capacitor | Various 0402 | 6 | $0.01 | $0.06 | nRF52832 decoupling |
| C30,C31 | Capacitor | 100nF/10uF | 2 | $0.02 | $0.04 | LoRa decoupling |
| C32,C33 | Capacitor | 1.5pF/1.2pF 0402 | 2 | $0.01 | $0.02 | Antenna matching |
| C40,C41 | Capacitor | 100nF 0402 | 2 | $0.01 | $0.02 | CAN IC decoupling |
| C50,C51 | Capacitor | 100nF 0402 | 2 | $0.01 | $0.02 | FRAM/Flash decoupling |
| C52 | Capacitor | 100nF 0402 | 1 | $0.01 | $0.01 | RTC decoupling |
| C60,C61 | Capacitor | 100nF 0402 | 2 | $0.01 | $0.02 | Button/PF debounce |
| C70,C71 | Capacitor | 1uF 0402 | 2 | $0.01 | $0.02 | MCU LDO in/out caps |
| - | PCB | 4-layer | 1 | $3.00 | $3.00 | |
| | | | | **Total** | **~$35** | |

## Valve Actuator Board (per unit)

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | MCU | nRF52810-QFAA | 1 | $1.75 | $1.75 | ARM Cortex-M4, BLE 5.0 |
| U2 | CAN Controller | MCP2515-I/SO | 1 | $1.00 | $1.00 | SPI interface |
| U3 | CAN Transceiver | SN65HVD230DR | 1 | $0.30 | $0.30 | 3.3V compatible |
| U4 | Buck Converter | TPS54202DDCR | 1 | $0.50 | $0.50 | 24V to 3.3V |
| U5 | FRAM | MB85RS1MTPNF | 1 | $1.30 | $1.30 | 128KB SPI, BLE PIN + logs |
| U6 | Flash | W25Q16JVSSIQ | 1 | $0.50 | $0.50 | 2MB SPI, OTA firmware |
| L1 | Inductor | 10µH 2A | 1 | $0.15 | $0.15 | For buck |
| Q1,Q2 | P-FET | AO3401A | 2 | $0.10 | $0.20 | High-side H-bridge |
| Q3,Q4 | N-FET | AO3400A | 2 | $0.05 | $0.10 | Low-side H-bridge |
| D1-D4 | Schottky | SS14 | 4 | $0.03 | $0.12 | Flyback diodes |
| D5,D6 | TVS | SMBJ28A | 2 | $0.15 | $0.30 | Motor protection |
| D7,D8 | TVS | SMBJ5.0A | 2 | $0.10 | $0.20 | Limit switch protection |
| F1 | PTC Fuse | MF-MSMF200 | 1 | $0.10 | $0.10 | 2A resettable |
| R1 | Shunt | 0.05Ω 1% 1/2W | 1 | $0.03 | $0.03 | Current sense |
| R2 | Termination | 120Ω | 1 | $0.01 | $0.01 | CAN termination |
| SW1 | DIP Switch | 10-position | 1 | $0.35 | $0.35 | 1-6: Addr, 10: Term |
| SW2 | Tactile Switch | 6x6mm | 1 | $0.05 | $0.05 | BLE Pairing button |
| Y1 | Crystal | 16MHz | 1 | $0.15 | $0.15 | For MCP2515 |
| LED1 | LED Green | 0603 | 1 | $0.02 | $0.02 | 3.3V indicator |
| LED2 | LED Yellow | 0603 | 1 | $0.02 | $0.02 | 24V indicator |
| LED3 | LED Red | 0603 | 1 | $0.02 | $0.02 | Status/Pairing |
| LED4 | LED Blue | 0603 | 1 | $0.02 | $0.02 | Valve open |
| J1 | Valve Connector | Phoenix XPC 5-pin | 1 | $1.50 | $1.50 | Motor + limits |
| J2,J3 | CAN Connector | Phoenix XPC 4-pin | 2 | $1.00 | $2.00 | Daisy chain |
| - | Passives | Various | - | - | $0.60 | Caps, resistors |
| - | PCB | 2-layer | 1 | $0.50 | $0.50 | |
| | | | | **Total** | **~$12** | |

## System Cost Summary

| Configuration | Controller | Actuators | Total |
|---------------|------------|-----------|-------|
| 8 valves | $34 | $96 | **$130** |
| 16 valves | $34 | $192 | **$226** |
| 32 valves | $34 | $384 | **$418** |
| 64 valves | $34 | $768 | **$802** |

*Note: Prices are component costs only. Does not include:*
- *Power Supply Unit (separate board)*
- *Enclosure*
- *Wiring and cables*
- *Assembly labor*

## Power Supply Unit (Separate)

| Component | Description | Est. Price |
|-----------|-------------|------------|
| AC-DC PSU | Mean Well or similar, 24V 5A | $15-25 |
| Battery | 7S Li-ion 25.9V pack | $40-60 |
| BMS | 7S battery management | $5-10 |
| Charger | 7S Li-ion charger circuit | $5-10 |
| AC Sense | Optocoupler circuit | $2 |
| Enclosure | Weatherproof box | $10-20 |
| **Total** | | **$77-127** |
