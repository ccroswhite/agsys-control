# Mag Meter Power Board - Schematic Reference

## Overview

The power board is a modular daughter board that plugs into the main board. Three variants exist for different pipe sizes and coil requirements. This document covers all three variants.

## Board Variants

| Variant | Pipe Size | Input Voltage | Excitation Freq | Coil Current |
|---------|-----------|---------------|-----------------|--------------|
| MM-S | 3/4" - 2" | 24V DC | 2 kHz | 1A |
| MM-M | 2.5" - 4" | 48V DC | 2 kHz | 2.5A |
| MM-L | 5" - 6" | 60V DC | 2 kHz | 4A |

**Note:** All variants now use 2 kHz excitation for better 1/f noise rejection and capacitive coupling through PVC.

---

## MM-S Power Board (24V, 2kHz)

### Bill of Materials

| Ref | Part Number | Description | Package | Qty |
|-----|-------------|-------------|---------|-----|
| Q1 | IRLB8721PBF | N-ch MOSFET, 30V 62A, Rds 8.7mΩ | TO-220 | 1 |
| D1 | SS34 | Schottky diode, 3A 40V (24V supply OK) | SMA | 1 |
| R1 | 0.1Ω 1W | Current sense resistor | 2512 | 1 |
| R2 | 10kΩ | Gate pulldown | 0402 | 1 |
| R3 | 100Ω | Gate series resistor | 0402 | 1 |
| C1 | 100µF 35V | Input bulk cap | Electrolytic | 1 |
| C2 | 100nF | Input bypass | 0805 | 1 |
| D2 | SMBJ28A | TVS, 28V standoff (optional) | SMB | 1 |
| J1 | 8-pin header | Main board connector | 2.54mm | 1 |
| J2 | 2-pin terminal | Power input (24V) | 5.08mm | 1 |
| J3 | 2-pin terminal | Coil output | 5.08mm | 1 |
| R4 | 1MΩ 1% | Tier ID resistor (top) | 0402 | 1 |
| R5 | 3MΩ 1% | Tier ID resistor (bottom) | 0402 | 1 |
| C3 | 100pF | Tier ID filter cap (optional) | 0402 | 1 |

### Schematic

```
                                    +24V
                                     │
        J2 (Power In)                │
        ┌───┐                        │
   24V ─┤ 1 ├────────────┬───────────┼─────────────────────────────┐
        │   │            │           │                             │
   GND ─┤ 2 ├──┐         │          ┌┴┐                           │
        └───┘  │        ┌┴┐         │ │ R4 (1M)                   │
               │        │ │ C1      │ │ Tier ID                   │
               │        │ │ 100µF   └┬┘                           │
               │        └┬┘          ├───┬────────────► TIER_ID   │
               │         │          ┌┴┐ ┌┴┐ C3 (100pF)  (to J1)   │
               │         │          │ │ └┬┘                       │
               │         │          │ │ R5 (3M)                   │
               │         │          │ │                           │
               │         │          └┬┘                           │
               │         │           │                            │
               ├─────────┴───────────┴────────────────────────────┤
               │                                                   │
              GND                                                 GND


        MOSFET Driver Section:
        ──────────────────────

                              +24V
                               │
                              ┌┴┐
                              │ │ Coil
                              │ │ (external)
                              └┬┘
                               │
                               ├──────────────────────► COIL+ (J3 pin 1)
                               │
        COIL_GATE ────┬───[R3]─┤
        (from J1)     │  100Ω  │
                     ┌┴┐       │ D (Drain)
                     │ │       │
                     │ │ R2   ┌┴┐
                     │ │ 10k  │ │ Q1
                     └┬┘      │ │ IRLB8721
                      │       └┬┘
                      │        │ S (Source)
                      │        │
                      │       ┌┴┐
                      │       │ │ R1 (0.1Ω)
                      │       │ │ Current Sense
                      │       └┬┘
                      │        │
                      │        ├──────────────────────► I_SENSE (J1 pin 6)
                      │        │
                      └────────┴──────────────────────► GND


        Flyback Protection:
        ───────────────────

                    ┌──────────────────┐
                    │                  │
               COIL+│    ┌────┐        │
                ────┼────┤ D1 ├────────┤
                    │    │SS34│        │
                    │    └────┘        │
                    │     ▲            │
                    │     │ (cathode   │
                    │       to +24V)   │
                    │                  │
               COIL-│                  │+24V
                ────┼──────────────────┘
                    │
                   GND
```

### Main Board Connector (J1) Pinout

| Pin | Signal | Direction | Description |
|-----|--------|-----------|-------------|
| 1 | VIN | Output | 24V to main board |
| 2 | GND | Common | Ground |
| 3 | +3V3 | Input | 3.3V from main board (for tier ID) |
| 4 | TIER_ID | Output | Voltage divider output (~0.825V for MM-S) |
| 5 | COIL_GATE | Input | PWM signal from MCU |
| 6 | I_SENSE | Output | Current sense voltage |
| 7 | COIL+ | Output | Coil drive (through connector) |
| 8 | COIL- | Output | Coil return (GND) |

### Tier ID Voltage Divider

Each power board has a unique high-impedance voltage divider to identify itself.
Uses 1% tolerance resistors for accuracy. Optional 100pF cap for noise filtering.

| Variant | R4 (top) | R5 (bottom) | TIER_ID Voltage | ADC Value (12-bit) | Current |
|---------|----------|-------------|-----------------|-------------------|---------|
| MM-S | 1MΩ | 3MΩ | 0.825V | ~1024 | 0.83 µA |
| MM-M | 1MΩ | 1MΩ | 1.65V | ~2048 | 1.65 µA |
| MM-L | 3MΩ | 1MΩ | 2.475V | ~3072 | 0.83 µA |

**Note:** High-impedance dividers minimize power waste (~5µW vs ~0.5mW with 10k/30k).

---

## MM-M Power Board (48V, 2kHz)

### Bill of Materials (differences from MM-S)

| Ref | Part Number | Description | Package | Notes |
|-----|-------------|-------------|---------|-------|
| Q1 | IPD50N06S4-14 | N-ch MOSFET, 60V 50A, Rds 9mΩ | TO-252 | Higher voltage |
| D1 | SS56 | Schottky diode, 5A 60V | SMC | 60V for 48V supply margin |
| R1 | 0.02Ω 1W | Current sense resistor | 1206 | Low power with amp |
| U1 | INA181A1IDBVR | Current sense amp, 20V/V | SOT-23-5 | Amplifies shunt voltage |
| C1 | 100µF 63V | Input bulk cap | Electrolytic | Higher voltage |
| D2 | SMBJ51A | TVS, 51V standoff (optional) | SMB | Higher voltage |
| R4 | 1MΩ 1% | Tier ID resistor (top) | 0402 | High-Z divider |
| R5 | 1MΩ 1% | Tier ID resistor (bottom) | 0402 | 1.65V output |
| C3 | 100pF | Tier ID filter cap (optional) | 0402 | Noise filtering |

### Current Sense Calculation

- At 4A coil current: V_sense = 4A × 0.02Ω = 80mV
- INA181A1 gain = 20 V/V → Output = 80mV × 20 = 1.6V ✓
- Power dissipation: P = 4² × 0.02 = 0.32W (1206 resistor OK)

---

## MM-L Power Board (60V, 2kHz)

### Bill of Materials (differences from MM-S)

| Ref | Part Number | Description | Package | Notes |
|-----|-------------|-------------|---------|-------|
| Q1 | IPD50N06S4-14 | N-ch MOSFET, 60V 50A, Rds 9mΩ | TO-252 | Same as MM-M |
| D1 | SS510 | Schottky diode, 5A 100V | SMC | 100V for 60V supply margin |
| R1 | 0.02Ω 1W | Current sense resistor | 1206 | Low power with amp |
| U1 | INA181A1IDBVR | Current sense amp, 20V/V | SOT-23-5 | Amplifies shunt voltage |
| C1 | 100µF 80V | Input bulk cap | Electrolytic | Higher voltage |
| D2 | SMBJ64A | TVS, 64V standoff (optional) | SMB | Higher voltage |
| R4 | 3MΩ 1% | Tier ID resistor (top) | 0402 | High-Z divider |
| R5 | 1MΩ 1% | Tier ID resistor (bottom) | 0402 | 2.475V output |
| C3 | 100pF | Tier ID filter cap (optional) | 0402 | Noise filtering |

### Current Sense Calculation

- At 5A coil current: V_sense = 5A × 0.02Ω = 100mV
- INA181A1 gain = 20 V/V → Output = 100mV × 20 = 2.0V ✓
- Power dissipation: P = 5² × 0.02 = 0.5W (1206 resistor OK)

---

## Current Sense Amplifier Circuit (MM-M and MM-L)

The INA181 is a low-side current sense amplifier that amplifies the small voltage across the shunt resistor.

```
        MOSFET Source
             │
             │
            ┌┴┐
            │ │ R1 (0.02Ω)
            │ │ Shunt Resistor
            └┬┘
             │
             ├────────────────────────────────────► GND
             │
             │
    ┌────────┴────────┐
    │                 │
    │  IN+       IN-  │
    │    │       │    │
    │    └───┬───┘    │
    │        │        │
    │     ┌──┴──┐     │
    │     │     │     │
    │     │ INA │     │
    │     │ 181 │     │
    │     │     │     │
    │     └──┬──┘     │
    │        │        │
    │       OUT       │
    │        │        │
    └────────┼────────┘
             │
             ├───────────────────────────────────► I_SENSE (to J1 pin 6)
             │
            ┌┴┐
            │ │ 100nF (optional filter)
            └┬┘
             │
            GND


    INA181 Pinout (SOT-23-5):
    ┌─────────────────┐
    │  1: GND         │
    │  2: IN+         │
    │  3: IN-         │
    │  4: OUT         │
    │  5: VS (3.3V)   │
    └─────────────────┘
```

### INA181 Variant Selection

| Variant | Gain | Max Shunt Voltage | Use Case |
|---------|------|-------------------|----------|
| INA181A1 | 20 V/V | 165mV for 3.3V out | MM-M, MM-L (recommended) |
| INA181A2 | 50 V/V | 66mV for 3.3V out | Lower current apps |
| INA181A3 | 100 V/V | 33mV for 3.3V out | Very low current |
| INA181A4 | 200 V/V | 16.5mV for 3.3V out | Micro-current |

### Power Comparison

| Board | Old Design | New Design | Savings |
|-------|------------|------------|---------|
| MM-S | 0.1Ω @ 2.5A = 0.625W | Keep 0.1Ω (low current) | - |
| MM-M | 0.05Ω @ 4A = 0.8W | 0.02Ω @ 4A = 0.32W | 60% |
| MM-L | 0.05Ω @ 5A = 1.25W | 0.02Ω @ 5A = 0.5W | 60% |

**Note:** MM-S can keep the simple 0.1Ω resistor since power is manageable at 2.5A.

---

## Design Notes

### MOSFET Selection Criteria

1. **Vds > 2× supply voltage** (for inductive kickback margin)
2. **Rds(on) < 20mΩ** (minimize power dissipation)
3. **Logic-level gate** (Vgs(th) < 2V for 3.3V drive)
4. **Id > 2× peak coil current** (thermal margin)

### Flyback Diode Selection

1. **Vrrm > 1.5× supply voltage** (margin for inductive spikes)
2. **If > peak coil current**
3. **Fast recovery** (Schottky preferred)
4. **Place as close to MOSFET as possible**

**Selected Diodes:**
| Tier | Supply | Diode | Vrrm | Margin |
|------|--------|-------|------|--------|
| MM-S | 24V | SS34 | 40V | 67% |
| MM-M | 48V | SS56 | 60V | 25% |
| MM-L | 60V | SS510 | 100V | 67% |

### Current Sense Resistor

1. **Low resistance** to minimize power loss
2. **High power rating** (P = I²R)
3. **Low inductance** (use 4-terminal Kelvin sense for accuracy)
4. **Temperature stable** (±1% tolerance)

### Gate Drive

- R3 (100Ω series) limits gate current and reduces EMI
- R2 (10kΩ pulldown) ensures MOSFET stays off when MCU is in reset
- Direct 3.3V GPIO drive is sufficient for logic-level MOSFETs

### TVS Protection

- D2 is optional - populate based on field testing
- Protects against voltage spikes from coil switching
- Standoff voltage should be slightly above supply voltage

---

## PCB Layout Guidelines

1. **Star ground** - All ground connections meet at one point
2. **Short gate trace** - Minimize inductance in gate drive path
3. **Kelvin sense** - Route I_SENSE directly from sense resistor pads
4. **Thermal relief** - MOSFET tab needs adequate copper for heat dissipation
5. **Flyback diode** - Place immediately adjacent to MOSFET drain
6. **Input caps** - Place close to power input connector
