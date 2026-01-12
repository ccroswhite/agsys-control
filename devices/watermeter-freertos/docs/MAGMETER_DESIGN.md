# AgSys Electromagnetic Flow Meter Design Specification

## Overview

This document describes the design of the AgSys electromagnetic (mag) flow meter for agricultural water measurement. The design uses capacitive coupling through PVC pipe walls to avoid electrode fouling from silty water common in vineyard and farm irrigation systems.

## Design Goals

| Requirement | Target |
|-------------|--------|
| Accuracy | ≤1% of reading |
| Repeatability | High (temperature compensated) |
| Pipe sizes | 1.5", 2", 2.5", 3", 4" |
| Pipe material | Schedule 40 PVC |
| Power source | Mains (120/240V AC) |
| Sampling rate | 1 second |
| Reporting interval | 1 minute (via LoRa) |
| Fouling resistance | Excellent (no wetted electrodes) |

## Operating Principle

### Faraday's Law of Electromagnetic Induction

When a conductive fluid flows through a magnetic field perpendicular to the flow direction, a voltage is induced proportional to the flow velocity:

```
V = B × D × v

Where:
  V = induced voltage (volts)
  B = magnetic field strength (tesla)
  D = pipe inner diameter (meters)
  v = fluid velocity (m/s)
```

### Capacitive Coupling

Unlike traditional mag meters with wetted electrodes, this design uses **capacitive coupling** through the PVC pipe wall:

- Copper foil electrodes mounted on the **outside** of the pipe
- PVC pipe wall acts as the dielectric
- High-frequency (100kHz+) excitation couples through the capacitive barrier
- Eliminates electrode fouling, corrosion, and maintenance

## Physical Design

### Pipe Section Assembly

```
Side View (Flow left to right):

   GND      Coil A     Sense      Coil B     GND
   foil                foils                 foil
    │         │     ┌───┴───┐      │         │
    ▼         ▼     ▼       ▼      ▼         ▼
┌───╫─────────╫═════╫═══════╫═════╫─────────╫───┐
│   ║         ║     ║  PVC  ║     ║         ║   │
│   ║         ║     ║ (H2O) ║     ║         ║   │  ══► Flow
│   ║         ║     ║       ║     ║         ║   │
└───╫─────────╫═════╫═══════╫═════╫─────────╫───┘
    │         │     │       │     │         │
    │         │     └───┬───┘     │         │
   GND     Coil A    Triax     Coil B      GND
                     cable
                     to PCB


Cross Section (looking down pipe):

              Coil (top)
                 ║
            ┌────╨────┐
       ┌────┤         ├────┐
  Cu ──┤    │   PVC   │    ├── Cu
 foil  │    │  (H2O)  │    │  foil
       └────┤         ├────┘
            └────╥────┘
                 ║
              Coil (bottom)
```

### Helmholtz Coil Configuration

The excitation coils are arranged in a **Helmholtz configuration** for uniform magnetic field:

- Two identical coils, coaxial, separated by distance equal to coil radius
- Provides ~1% field uniformity over central 50% of gap
- Sensing electrodes positioned at center of gap for maximum flux linkage

## Board Variants

Three board variants handle different pipe size ranges, each with optimized power stage:

| Board | Pipes | Schedule | Voltage | Frequency | Peak Current | Peak Power |
|-------|-------|----------|---------|-----------|--------------|------------|
| **MM-S** | 1.5" - 2" | 80 | 24V DC | 500 Hz | 2.5A | ~100W |
| **MM-M** | 2.5" - 4" | 40 | 48V DC | 1 kHz | 4A | ~300W |
| **MM-L** | 5" - 6" | 40 | 60V DC | 2 kHz | 5A | ~550W |

**Shared across all boards:**
- MCU (nRF52840-QFAA) - 1MB Flash, 256KB RAM for LVGL
- LoRa (RFM95C)
- FRAM (MB85RS1MT - 128KB)
- Display (Focus LCDs E28GA-T-CW250-N) - 2.8" transflective TFT, ST7789
- Fully Differential Amp (THS4551)
- Guard Driver (ADA4522)
- Electrode input circuit with active guarding
- Firmware with LVGL graphics (tier-configurable)

**Different per board:**
- Power supply input voltage
- MOSFET (voltage/current rating)
- Flyback diode (current rating)
- ADC (ADS1220 for MM-S/MM-M, ADS1256 for MM-L)
- Current sense resistor

**Coil Parameters (per pipe size):**

| Pipe Size | Schedule | Coil Radius | Turns | Wire Gauge | Resistance |
|-----------|----------|-------------|-------|------------|------------|
| 1.5" | 80 | 30mm | 150 | 26 AWG | 3.0Ω |
| 2" | 80 | 38mm | 150 | 26 AWG | 3.8Ω |
| 2.5" | 40 | 45mm | 150 | 26 AWG | 4.6Ω |
| 3" | 40 | 55mm | 150 | 26 AWG | 5.6Ω |
| 4" | 40 | 70mm | 150 | 26 AWG | 7.2Ω |
| 5" | 40 | 85mm | 150 | 24 AWG | 5.4Ω |
| 6" | 40 | 100mm | 150 | 24 AWG | 6.4Ω |

### Electrode Configuration

**Sensing Electrodes:**
- Material: Copper foil (self-adhesive tape)
- Position: Center of Helmholtz gap, 180° apart (opposing sides)
- Width: 10-20mm (TBD based on testing)
- Coverage: Partial wrap around pipe circumference

**Grounding Electrodes:**
- Material: Copper foil
- Position: 2-3 pipe diameters from sensing electrodes (both ends)
- Purpose: Bleed off stray voltages from water (galvanic, pump noise, etc.)
- Connection: Tied to signal ground (capacitively coupled, no wetted contact)

### Signal Cabling and Active Guarding

Given the extremely weak signal (µV level), proper shielding and active guarding is critical.

**Triaxial Cable Structure:**
```
Cross-section:
    ┌─────────────────────┐
    │  Outer Jacket       │  ← Mechanical protection
    │  ┌───────────────┐  │
    │  │ Outer Shield  │  │  ← Ground (drain to GND)
    │  │ ┌───────────┐ │  │
    │  │ │ Guard     │ │  │  ← Driven by ADA4522 (tracks signal)
    │  │ │ ┌───────┐ │ │  │
    │  │ │ │Signal │ │ │  │  ← Electrode connection to THS4551
    │  │ │ └───────┘ │ │  │
    │  │ └───────────┘ │  │
    │  └───────────────┘  │
    └─────────────────────┘
```

**Cable Options:**
| Type | Example | Notes |
|------|---------|-------|
| True triaxial | Belden 9222 | Best performance, expensive |
| Double-shielded coax | Belden 9273 | Inner braid + outer foil |
| DIY: Coax + foil | RG-316 + copper tape | Custom guard layer |

**Active Guard Drive Circuit:**
```
                    ┌─────────────┐
Electrode+ ──┬──────┤+            │
             │      │   ADA4522   ├──┬──► Guard Shield (+)
             │  ┌───┤-   (unity)  │  │
             │  │   └─────────────┘  │
             │  └────────────────────┘
             │
             └─────────────────────────► To THS4551 (+IN)

                    ┌─────────────┐
Electrode- ──┬──────┤+            │
             │      │   ADA4522   ├──┬──► Guard Shield (-)
             │  ┌───┤-   (unity)  │  │
             │  │   └─────────────┘  │
             │  └────────────────────┘
             │
             └─────────────────────────► To THS4551 (-IN)
```

**Why ADA4522 for Guard Driver:**
- Zero-drift chopper amplifier
- Ultra-low offset: 0.1 µV (tracks signal precisely)
- Ultra-low drift: 0.005 µV/°C
- Unity-gain follower configuration
- Eliminates capacitive leakage between signal and ground

**Guard Continuity:**
- Guard extends from copper foil electrode all the way to THS4551 input pins
- PCB guard traces surround signal traces
- Guard plane under signal traces on PCB

**Cable Length:**
- Keep electrode-to-board distance minimal (<30cm recommended)
- Longer runs increase capacitance and noise pickup

### Grounding Architecture

Proper grounding is critical for µV-level signal acquisition. A star topology prevents ground loops and noise coupling.

```
                PIPE SECTION                              MAIN BOARD
                ════════════════════════════════════════════════════════════
                
Grounding       ┌─────────────┐
Electrode ──────┤ Cu foil     ├─────────────────────┐
(upstream)      │ (capacitive)│                     │
                └─────────────┘                     │
                                                    │
Sensing         ┌─────────────┐    Triax            │    ┌─────────────┐
Electrode+ ─────┤ Cu foil     ├══════════════════════════┤ THS4551 +IN │
                └─────────────┘    (guarded)        │    └─────────────┘
                                                    │
Sensing         ┌─────────────┐    Triax            │    ┌─────────────┐
Electrode- ─────┤ Cu foil     ├══════════════════════════┤ THS4551 -IN │
                └─────────────┘    (guarded)        │    └─────────────┘
                                                    │
Grounding       ┌─────────────┐                     │
Electrode ──────┤ Cu foil     ├─────────────────────┼───► AGND
(downstream)    │ (capacitive)│                     │
                └─────────────┘                     │
                                                    │
Triax Outer     ────────────────────────────────────────► AGND
Shields
                                                    │
                                                    ▼
                                            ┌───────────────┐
                                            │ STAR GROUND   │
                                            │ (single point)│
                                            └───────┬───────┘
                                                    │
                            ┌────────────────┬──────┴──────┬────────────────┐
                            ▼                ▼             ▼                ▼
                         AGND            DGND          Earth GND       Power GND
                      (analog)         (digital)      (enclosure)      (24/48/60V)
```

**Ground Domains:**

| Ground | Purpose | Components |
|--------|---------|------------|
| **AGND** | Analog signal reference | THS4551, ADA4522, ADS1220, grounding electrodes |
| **DGND** | Digital circuits | nRF52840, RFM95C, display, FRAM |
| **Earth GND** | Lightning/safety | Enclosure, ground rod, pipe ground strap |
| **Power GND** | Coil return, power stage | MOSFET source, power supply return |

**Critical Grounding Rules:**

1. **Star topology** - All ground domains meet at ONE point on PCB
2. **Separate planes** - AGND and DGND run as separate copper pours, joined only at star point
3. **Grounding electrodes → AGND** - Establishes common reference with water potential
4. **Triax outer shields → AGND** - NOT to chassis (avoids ground loops)
5. **Guard shields → ADA4522 output** - Actively driven, NOT grounded
6. **No ground loops** - Single path from any point to star ground
7. **Wide traces** - Low impedance ground returns for analog section

**PCB Layout Guidelines:**

- Place star ground point near ADS1220 (most sensitive component)
- Keep analog section physically separated from digital
- Route AGND return traces directly under signal traces
- Use ground plane under THS4551 and ADA4522
- Ferrite bead between AGND and DGND at star point (optional, for HF isolation)
- Keep coil drive traces away from analog input section

## Electronics Architecture

### Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Water Meter Electronics                          │
│                                                                          │
│  ┌──────────┐    ┌─────────────┐    ┌──────────────────────────────┐   │
│  │ nRF52840 │───▶│ MOSFET +    │───▶│ Helmholtz Coils              │   │
│  │   MCU    │    │ Flyback     │    │ (Pulsed DC, 500Hz-2kHz)      │   │
│  │          │    └─────────────┘    └──────────────────────────────┘   │
│  │          │                                                           │
│  │          │    ┌─────────────┐    ┌─────────────┐    ┌───────────┐   │
│  │          │◀───│ ADS131M02   │◀─RC│  THS4551    │◀───│ ADA4522   │◀──┤
│  │          │    │ 24-bit ADC  │    │  Diff Amp   │    │ Guard Drv │   │
│  │          │    │ 64kSPS+PGA  │    └─────────────┘    └───────────┘   │
│  │          │    └─────────────┘                                      │
│  │          │                                                ▲          │
│  │          │    ┌─────────────┐                             │          │
│  │          │───▶│ RFM95C LoRa │                      ┌──────┴──────┐  │
│  │          │    └─────────────┘                      │  Electrodes │  │
│  │          │                                         │  (Cu foil)  │  │
│  │          │    ┌─────────────┐                      │  + Triax    │  │
│  │          │───▶│ 2.8" TFT    │                      └─────────────┘  │
│  │          │    │ (LVGL)      │                                       │
│  └──────────┘    └─────────────┘                                       │
│                                                                          │
│  Power: Mains → 24/48/60V DC → Buck → 5V (analog) / 3.3V (digital)     │
└─────────────────────────────────────────────────────────────────────────┘
```

### Signal Chain

1. **Excitation Generation**
   - MCU generates pulsed DC signal (no H-bridge needed)
   - Single MOSFET driver with flyback diode
   - Pulse frequency: **1 kHz** (above 1/f noise corner for better SNR)
   - Current: ~100-500mA peak
   - Pulsed DC is sufficient because:
     - No wetted electrodes (capacitive coupling eliminates polarization)
     - Non-ferrous copper electrodes (no magnetic interaction)
     - Plastic housing (no eddy currents)
   - **Bidirectional flow detection**: Sign of (V_on - V_off) indicates direction

2. **Signal Pickup**
   - Copper foil electrodes capacitively couple to water
   - Expected signal: 10-100 µV (depending on flow rate and pipe size)
   - Triaxial cable with active guard to PCB

3. **Instrumentation Amplifier**
   - High CMRR (>100dB) to reject common-mode noise
   - Low noise (<10 nV/√Hz)
   - Gain: 60-80 dB
   - Guard driver output for active shielding

4. **Pulsed DC Measurement (Offset Cancellation)**
   - Sample during field-ON: V_on = V_flow + V_offset
   - Sample during field-OFF: V_off = V_offset
   - Compute: V_flow = V_on - V_off
   - Eliminates DC offset and low-frequency drift
   - No complex lock-in amplifier needed

5. **ADC**
   - 24-bit resolution for µV-level signals
   - Built-in or external temperature sensor for compensation
   - SPI interface to MCU

### Temperature Compensation

For ≤1% accuracy, temperature compensation is required for:

- **Coil resistance** (affects field strength)
- **Amplifier gain drift**
- **ADC reference drift**
- **Water conductivity** (minor effect for capacitive coupling)

**Approach:**
- Temperature sensor(s) on PCB near critical components
- Calibration coefficients stored in FRAM
- Real-time correction in firmware

## Component Selection

### Shared Components (All Boards)

| Function | Part | Notes |
|----------|------|-------|
| MCU | nRF52840-QFAA | 1MB Flash, 256KB RAM for LVGL |
| Diff Amp | THS4551IDGKR | 1.5 nV/√Hz, fully differential, VSSOP-8 |
| Guard Driver | ADA4522-2ARMZ | Zero-drift chopper, 0.1µV offset, dual, MSOP-8 |
| ADC | ADS131M02IPWR | 24-bit, 64 kSPS, 2-ch simultaneous, PGA 1-128×, TSSOP-20 |
| LoRa | RFM95C | Existing platform, 915 MHz |
| FRAM | MB85RS1MTPNF | 1Mbit (128KB), calibration + runtime logs |
| Display | E28GA-T-CW250-N | 2.8" transflective TFT, ST7789, 240x320 |
| Backlight Buck | MP1584 or similar | 24V → 12V for LED backlight |
| LDO (5V) | AP2112K-5.0 | 5V 600mA for analog section |
| LDO (3.3V) | AP2112K-3.3 | 3.3V 600mA for MCU/LoRa |
| Temp Sensor | nRF52840 internal | Used for temperature compensation |

### Analog Front-End Signal Chain

```
Electrode+ ──┬──► ADA4522 (guard+) ──► Guard Shield+
             │
             └──► THS4551 (+IN) ──► R ──┬──► ADS131M02 (AIN1+)
                                       │
                                       C
                                       │
                                      GND

Electrode- ──┬──► THS4551 (-IN) ──► R ──┬──► ADS131M02 (AIN1-)
             │                         │
             │                         C
             │                         │
             │                        GND
             │
             └──► ADA4522 (guard-) ──► Guard Shield-
```

**THS4551 Configuration:**
- Fully differential amplifier
- Single-supply 5V operation
- Gain set by external resistors (10-100×, let ADC PGA handle fine adjustment)
- Differential output to ADS131M02 via RC filter

**RC Filter (ADC Input Protection):**
- R = 47Ω (isolates charge kickback from ADC sampling)
- C = 100pF C0G/NP0 (absorbs charge injection, low distortion)
- RC time constant = 4.7 ns (fast enough for 64 kSPS)

**ADA4522-2 Configuration:**
- Dual zero-drift op-amp (one per electrode)
- Unity-gain follower
- Drives guard shields to track electrode signals

**Decoupling Capacitors:**
- THS4551: 100nF + 10µF on each supply pin
- ADA4522: 100nF on supply
- ADS131M02:
  - AVDD: 1µF + 100nF to AGND (per datasheet requirement)
  - DVDD: 1µF + 100nF to DGND (per datasheet requirement)
  - CAP pin: 100nF to DGND (since DVDD = 3.3V > 2V)
- Place as close to IC pins as possible

**TVS Protection Footprints:**
- Electrode inputs: TPD2E001 or PESD5V0S2BT pads (ultra-low capacitance)
- Populate based on field testing for signal attenuation

### ADS131M02 PCB Layout with Guard Ring

Based on TI layout guidance (Figure 11-1) with guard ring modifications:

```
                                    +3.3V (AVDD)        +3.3V (DVDD)
                                        │                   │
                                       ┌┴┐                 ┌┴┐
                                       │1µF                │1µF
                                       └┬┘                 └┬┘
                                        │                   │
    From THS4551                   ┌────┴───────────────────┴────┐
    ════════════                   │                             │
                                   │  1: AVDD          20: DVDD  │
    Guard+ ─────────────────────┐  │  2: AGND          19: DGND  │
                                │  │                             │
    ┌─[R]─┬─────────────────────┼──│  3: AIN0P         18: CAP   │──┬── 100nF ── DGND
    │     C                     │  │  4: AIN0N         17: CLKIN │  │
    │     │                     │  │                             │  │
    │    GND                    │  │  5: AIN1N         16: DIN   │──── SPI
    │                           │  │  6: AIN1P         15: DOUT  │──── SPI
    │  ┌─[R]─┬──────────────────┼──│                   14: SCLK  │──── SPI
    │  │     C                  │  │  7: NC            13: DRDY  │──── GPIO (IRQ)
    │  │     │                  │  │  8: NC            12: CS    │──── GPIO
    │  │    GND                 │  │  9: NC            11: SYNC  │──── GPIO or tie high
    │  │                        │  │ 10: NC                      │
    │  │                        │  │                             │
    │  │                        │  │      ┌─────────┐            │
    │  │                        │  │      │ AGND    │            │
    │  │                        │  │      │ (pad)   │            │
    │  │                        │  └──────┴─────────┴────────────┘
    │  │                        │              │
    │  │                        │              ▼
    │  │                        │         Star Ground
    │  │                        │
    │  │  ┌─────────────────────┴─────────────────────────────────┐
    │  │  │                    GUARD RING                         │
    │  │  │  (copper pour surrounding analog input traces)        │
    │  │  │  Driven by ADA4522 outputs                            │
    │  │  └───────────────────────────────────────────────────────┘
    │  │
    │  └── From THS4551 OUT-
    │
    └───── From THS4551 OUT+
```

**Guard Ring Layout Rules:**
1. Guard ring surrounds AIN0P, AIN0N, AIN1P, AIN1N traces
2. Guard driven by ADA4522 (tracks input signal)
3. Guard ring does NOT connect to ground
4. Maintain gap between guard and ground planes
5. Route guard on same layer as signal traces
6. Guard plane on layer 2 under signal traces on layer 1

**Additional Layout Guidelines:**
- Keep analog inputs (pins 3-6) away from digital signals (pins 11-17)
- RC filter components as close to AIN pins as possible
- AGND and DGND connected only at star ground point
- Decoupling caps within 3mm of power pins
- Terminate long digital lines with series resistors if needed

### Board-Specific Components

**Power Board Variants (plug into main board):**

**MM-S (1.5" - 2", 24V, 500Hz):**

| Function | Part | Notes |
|----------|------|-------|
| Power Input | 24V DC | From external AC-DC or shared with valve controller |
| MOSFET | IRLB8721PBF | 30V 62A, Rds 8.7mΩ @ 4.5V, TO-220 |
| Flyback Diode | SS34 | 3A 40V Schottky |
| Current Sense | 0.1Ω 1W | 250mV @ 2.5A |

**MM-M (2.5" - 4", 48V, 1kHz):**

| Function | Part | Notes |
|----------|------|-------|
| Power Input | 48V DC | Industrial supply |
| MOSFET | IPD50N06S4-14 | 60V 50A, Rds 9mΩ, TO-252 |
| Flyback Diode | SS54 | 5A 40V Schottky |
| Current Sense | 0.05Ω 2W | 200mV @ 4A |

**MM-L (5" - 6", 60V, 2kHz):**

| Function | Part | Notes |
|----------|------|-------|
| Power Input | 60V DC | Industrial supply |
| MOSFET | IPD50N06S4-14 | 60V 50A, Rds 9mΩ, TO-252 |
| Flyback Diode | SS56 | 5A 60V Schottky |
| Current Sense | 0.05Ω 3W | 250mV @ 5A |

**Note:** ADC (ADS131M02) is shared on main board across all tiers. 64 kSPS handles all excitation frequencies.

## Display Interface

**2.8" Transflective TFT (240x320) with LVGL:**

```
┌──────────────────────────────────────────┐
│  ┌──────────────────────────────────────┐  │
│  │           FLOW RATE                  │  │
│  │                                      │  │
│  │         125.3 GPM                   │  │
│  │    ██████████████████░░░░░░           │  │
│  └──────────────────────────────────────┘  │
│                                          │
│  ┌────────────────┐  ┌─────────────────┐  │
│  │ TODAY          │  │ TOTAL           │  │
│  │   1,523 gal    │  │   847,291 gal   │  │
│  └────────────────┘  └─────────────────┘  │
│                                          │
│  TEMP: 72°F    SIGNAL: ████░  ● Online   │
└──────────────────────────────────────────┘
```

**Display Hardware:**
- Part: Focus LCDs E28GA-T-CW250-N
- Controller: ST7789
- Resolution: 240x320
- Interface: 4-wire SPI
- Backlight: 12V @ 40mA (via buck from 24V)
- Transflective: Readable in direct sunlight
- **FPC Connector: Hirose FH12S-40S-0.5SH(55)** - 40-pin, 0.5mm pitch, bottom contact, ZIF

**Graphics Library:**
- LVGL (Light and Versatile Graphics Library)
- MIT license (commercial use OK)
- Widgets: Labels, arcs/gauges, bars, containers
- Fonts: Custom TrueType fonts for premium look
- Update rate: 1 Hz (matches measurement rate)

**Display Information:**
- Current flow rate (GPM or L/min, configurable)
- Accumulated total volume (resettable)
- Water/electronics temperature
- Signal quality indicator

## Communication

### LoRa Protocol

Uses the unified AgSys LoRa protocol (see `devices/common/PROTOCOL.md`).

**Report Payload (sent every minute):**

| Field | Size | Description |
|-------|------|-------------|
| Current flow | 4 bytes | float32, current instantaneous flow (GPM) |
| Average flow | 4 bytes | float32, 60-second average (GPM) |
| Min flow | 4 bytes | float32, minimum in period (GPM) |
| Max flow | 4 bytes | float32, maximum in period (GPM) |
| Total volume | 4 bytes | uint32, total gallons × 100 |
| Temperature | 2 bytes | int16, °C × 100 |
| Signal quality | 1 byte | 0-100% |
| Status flags | 1 byte | Error/warning bits |

**Total payload: 24 bytes**

### Network Impact

| Metric | Value |
|--------|-------|
| Reports per hour | 60 |
| Payload size | ~24 bytes |
| Packet size (with header + encryption) | ~50 bytes |
| Airtime per packet (SF10, 125kHz) | ~150ms |
| Duty cycle per meter | ~0.25% |
| Max meters per property | 40+ (within 10% duty cycle) |

## Calibration

### Factory Calibration

Each meter requires calibration for:
1. Zero offset (no flow)
2. Span (known flow rate)
3. Temperature coefficients

**Calibration data stored in FRAM:**
- Zero offset (µV)
- Span coefficient (µV per GPM)
- Temperature coefficients (offset and span)
- Pipe size / K-factor
- Serial number and calibration date

### Field Verification

- Zero check: Close valves, verify zero reading
- Span check: Compare against reference meter or timed fill

## Lightning / Surge Protection

Given outdoor installation near water pipes (excellent ground path), a layered protection strategy is used.

**Primary Protection: Earth Grounding**
- Heavy gauge earth ground strap from enclosure to ground rod
- Grounding electrode on pipe section (capacitively coupled)
- Provides low-impedance path for lightning energy to bypass electronics
- Single-point ground topology for analog section
- Separate digital and analog grounds, joined at one point

**Secondary Protection: TVS Diodes**

PCB includes footprints for TVS protection. Populate as needed based on field testing.

| Location | TVS Type | Notes |
|----------|----------|-------|
| Electrode inputs | TPD2E001 or PESD5V0S2BT | Ultra-low capacitance (<1pF), test for signal attenuation |
| Coil drive | SMBJ24CA | Clamp coil flyback transients |
| Mains input | MOV (14D471K) + GDT | Primary AC line protection |
| 3.3V rail | SMBJ3.3CA | Post-regulator protection |
| LoRa antenna | CG2-12L (GDT) | Low insertion loss |

**Design Note:** Electrode input TVS footprints allow A/B testing - can measure signal attenuation with/without TVS populated to verify acceptable performance.

## Installation Requirements

1. **Pipe orientation**: Electrodes horizontal (3 and 9 o'clock positions)
2. **Straight run**: 5D upstream, 3D downstream (D = pipe diameter)
3. **Grounding**: Earth ground connection **required** (lightning path)
4. **Electrical**: Dedicated circuit, surge protection at panel recommended
5. **Environment**: Weather-tight enclosure or sheltered location

## Alternative: Pulse Meter Input

For clean water installations where cost is prioritized over fouling resistance, the electronics support an alternative **pulse meter input**:

- Hall-effect or reed switch pulse output
- Configurable K-factor (pulses per gallon)
- Battery/solar powered option (low power mode)
- Same LoRa reporting format

This will be documented separately.

## Revision History

| Rev | Date | Author | Changes |
|-----|------|--------|---------|
| 0.1 | 2026-01-07 | - | Initial design decisions |
