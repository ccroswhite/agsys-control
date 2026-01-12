# Power Supply Unit (PSU) Bill of Materials

## Overview

The PSU provides 24V DC power to the valve controller system with automatic battery backup switchover.

**Specifications:**
- AC Input: 100-240VAC, 50/60Hz
- DC Output: 24V @ 2A continuous, 5A peak
- Battery: 7S Li-ion (25.9V nominal, 21V-29.4V range)
- Switchover time: <10ms
- Power fail signal: Active LOW when on battery

## Component List

### AC Input Section

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| J1 | IEC C14 Inlet | - | 1 | $2.00 | $2.00 | Panel mount with fuse holder |
| F1 | Fuse | 5A/250V 5x20mm | 1 | $0.20 | $0.20 | Slow-blow |
| MOV1 | Varistor | 275VAC 14mm | 1 | $0.50 | $0.50 | Surge protection |
| SW1 | Power Switch | DPST Rocker | 1 | $1.50 | $1.50 | Illuminated |

### AC-DC Converter

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| PS1 | AC-DC PSU | Mean Well LRS-50-24 | 1 | $15.00 | $15.00 | 24V 2.2A enclosed |

### Battery Management System

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | 7S BMS | Generic 7S 20A | 1 | $5.00 | $5.00 | Balance + protection |
| BT1 | Battery Pack | 7S 18650 pack | 1 | $40.00 | $40.00 | 25.9V nominal |
| J2 | Battery Connector | XT60 | 1 | $1.00 | $1.00 | For removable pack |

### Battery Charger

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U2 | Charger IC | LTC4020 or TP5100 | 1 | $3.00 | $3.00 | CC/CV Li-ion charger |
| L1 | Inductor | 22µH 5A shielded | 1 | $1.50 | $1.50 | |
| Q1 | MOSFET | IRF3205 or similar | 1 | $0.50 | $0.50 | |
| D1 | Schottky | SS54 | 1 | $0.20 | $0.20 | |
| R3 | Current Sense | 0.05Ω 2W | 1 | $0.30 | $0.30 | |
| - | Passives | Various | - | - | $1.00 | Caps, resistors |

### Power Path Control (Auto-Switchover)

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U3 | PowerPath IC | LTC4412 | 1 | $2.50 | $2.50 | Ideal diode controller |
| Q2, Q3 | P-FET | SI4435DDY | 2 | $0.50 | $1.00 | Low Rds(on) |
| D2, D3 | Schottky | SS54 | 2 | $0.20 | $0.40 | |

### AC Sense / Power Fail Detection

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U4 | Optocoupler | PC817 | 1 | $0.15 | $0.15 | AC isolation |
| R4, R5 | Resistor | 100K 1W | 2 | $0.10 | $0.20 | AC voltage divider |
| R6 | Pull-up | 10K | 1 | $0.01 | $0.01 | |
| C3 | Filter Cap | 100nF | 1 | $0.02 | $0.02 | Debounce |
| D4 | Diode | 1N4148 | 1 | $0.02 | $0.02 | Rectifier |

### Output Protection

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| F2 | Blade Fuse | 15A | 1 | $0.30 | $0.30 | Automotive type |
| D5 | TVS | SMBJ28A | 1 | $0.20 | $0.20 | Surge protection |
| C4 | Bulk Cap | 470µF/35V | 1 | $0.50 | $0.50 | |
| C5 | Bypass Cap | 100nF | 1 | $0.02 | $0.02 | |

### Output Connector

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| J3 | Output Connector | Phoenix XPC 3-pin | 1 | $1.50 | $1.50 | 24V, GND, PF signal |

### Status LEDs

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| LED1 | Green LED | 5mm | 1 | $0.10 | $0.10 | Mains OK |
| LED2 | Yellow LED | 5mm | 1 | $0.10 | $0.10 | Charging |
| LED3 | Red LED | 5mm | 1 | $0.10 | $0.10 | Battery Low |
| R7-R9 | LED Resistors | 1K | 3 | $0.01 | $0.03 | |

### Enclosure & Misc

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| - | Enclosure | IP65 plastic | 1 | $10.00 | $10.00 | Weatherproof |
| - | Cable Glands | PG9 | 3 | $0.50 | $1.50 | AC in, DC out, battery |
| - | PCB | 2-layer | 1 | $3.00 | $3.00 | |
| - | Wiring | Various | - | - | $2.00 | |

## Cost Summary

| Section | Cost |
|---------|------|
| AC Input | $4.20 |
| AC-DC Converter | $15.00 |
| Battery System | $46.00 |
| Charger | $6.50 |
| Power Path | $3.90 |
| AC Sense | $0.40 |
| Output Protection | $1.02 |
| Output Connector | $1.50 |
| Status LEDs | $0.33 |
| Enclosure & Misc | $16.50 |
| **Total** | **~$95** |

*Note: Battery pack is the largest cost. Customers may supply their own.*

## Output Connector Pinout

| Pin | Signal | Description |
|-----|--------|-------------|
| 1 | +24V | 24V DC output |
| 2 | GND | Ground |
| 3 | PF | Power Fail (active LOW when on battery) |

## LED Indicators

| LED | Color | State | Meaning |
|-----|-------|-------|---------|
| LED1 | Green | ON | Mains power OK |
| LED1 | Green | OFF | On battery power |
| LED2 | Yellow | ON | Battery charging |
| LED2 | Yellow | OFF | Fully charged or no mains |
| LED3 | Red | ON | Battery low (<20%) |
| LED3 | Red | BLINK | Battery critical (<10%) |
