# Valve Controller Bill of Materials

## Valve Controller Board

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | MCU | nRF52832-QFAA | 1 | $2.50 | $2.50 | ARM Cortex-M4F, BLE |
| U2 | LoRa Module | RFM95C | 1 | $6.00 | $6.00 | 915 MHz |
| U3 | CAN Controller | MCP2515-I/SO | 1 | $1.00 | $1.00 | SPI interface |
| U4 | CAN Transceiver | SN65HVD230DR | 1 | $0.50 | $0.50 | 3.3V compatible |
| U5 | FRAM | FM25V02-G | 1 | $2.00 | $2.00 | 256Kbit SPI |
| U6 | Flash | W25Q16JVSSIQ | 1 | $0.50 | $0.50 | 2MB SPI |
| U7 | RTC | RV-3028-C7 | 1 | $2.00 | $2.00 | Ultra-low power |
| U8 | LDO | MCP1700-3302E | 1 | $0.30 | $0.30 | 3.3V 250mA |
| Y1 | Crystal | 32MHz | 1 | $0.20 | $0.20 | For nRF52832 |
| Y2 | Crystal | 32.768kHz | 1 | $0.15 | $0.15 | For RTC |
| Y3 | Crystal | 16MHz | 1 | $0.15 | $0.15 | For MCP2515 |
| BT1 | Battery Holder | CR2032 | 1 | $0.20 | $0.20 | RTC backup |
| LED1 | LED Green | 0603 | 1 | $0.02 | $0.02 | 3.3V indicator |
| LED2 | LED Yellow | 0603 | 1 | $0.02 | $0.02 | 24V indicator |
| LED3 | LED Red | 0603 | 1 | $0.02 | $0.02 | Status |
| J1 | CAN Connector | M12 4-pin | 2 | $6.00 | $12.00 | A-coded |
| J2 | Power Connector | Phoenix XPC 3-pin | 1 | $1.00 | $1.00 | 24V + GND + PF |
| SW1 | Button | 6mm tactile | 1 | $0.10 | $0.10 | Pairing |
| - | Passives | Various | - | - | $2.00 | Caps, resistors |
| - | PCB | 4-layer | 1 | $3.00 | $3.00 | |
| | | | | **Total** | **~$34** | |

## Valve Actuator Board (per unit)

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | MCU | nRF52810-QFAA | 1 | $1.75 | $1.75 | ARM Cortex-M4 |
| U2 | CAN Controller | MCP2515-I/SO | 1 | $1.00 | $1.00 | SPI interface |
| U3 | CAN Transceiver | SN65HVD230DR | 1 | $0.30 | $0.30 | 3.3V compatible |
| U4 | Buck Converter | TPS54202DDCR | 1 | $0.50 | $0.50 | 24V to 3.3V |
| L1 | Inductor | 10µH 2A | 1 | $0.15 | $0.15 | For buck |
| Q1,Q2 | P-FET | AO3401A | 2 | $0.10 | $0.20 | High-side H-bridge |
| Q3,Q4 | N-FET | AO3400A | 2 | $0.05 | $0.10 | Low-side H-bridge |
| D1-D4 | Schottky | SS14 | 4 | $0.03 | $0.12 | Flyback diodes |
| D5,D6 | TVS | SMBJ28A | 2 | $0.15 | $0.30 | Motor protection |
| D7,D8 | TVS | SMBJ5.0A | 2 | $0.10 | $0.20 | Limit switch protection |
| F1 | PTC Fuse | MF-MSMF200 | 1 | $0.10 | $0.10 | 2A resettable |
| R1 | Shunt | 0.1Ω 1% 1/4W | 1 | $0.02 | $0.02 | Current sense |
| R2 | Termination | 120Ω | 1 | $0.01 | $0.01 | CAN termination |
| SW1 | DIP Switch | 10-position | 1 | $0.35 | $0.35 | 1-6: Addr, 10: Term |
| Y1 | Crystal | 16MHz | 1 | $0.15 | $0.15 | For MCP2515 |
| LED1 | LED Green | 0603 | 1 | $0.02 | $0.02 | 3.3V indicator |
| LED2 | LED Yellow | 0603 | 1 | $0.02 | $0.02 | 24V indicator |
| LED3 | LED Red | 0603 | 1 | $0.02 | $0.02 | Status |
| LED4 | LED Blue | 0603 | 1 | $0.02 | $0.02 | Valve open |
| J1 | Valve Connector | Phoenix XPC 5-pin | 1 | $1.50 | $1.50 | Motor + limits |
| J2,J3 | CAN Connector | Phoenix XPC 4-pin | 2 | $1.00 | $2.00 | Daisy chain |
| - | Passives | Various | - | - | $0.50 | Caps, resistors |
| - | PCB | 2-layer | 1 | $0.50 | $0.50 | |
| | | | | **Total** | **~$10** | |

## System Cost Summary

| Configuration | Controller | Actuators | Total |
|---------------|------------|-----------|-------|
| 8 valves | $34 | $80 | **$114** |
| 16 valves | $34 | $160 | **$194** |
| 32 valves | $34 | $320 | **$354** |
| 64 valves | $34 | $640 | **$674** |

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
