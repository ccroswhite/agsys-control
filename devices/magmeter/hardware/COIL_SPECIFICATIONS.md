# Magmeter Coil Specifications

## Document Information

| Item | Value |
|------|-------|
| Document | COIL_SPECIFICATIONS.md |
| Version | 1.0 |
| Date | January 12, 2026 |
| Author | AgSys Engineering |

---

## Overview

This document specifies the electromagnetic coil assemblies for the AgSys magmeter product line. Each magmeter uses a Helmholtz coil pair (two identical coils in series) to generate a uniform magnetic field perpendicular to the flow direction.

### Design Parameters

- **Configuration:** Helmholtz pair (2 coils wired in series)
- **Pipe Standard:** Schedule 40 PVC (Schedule 80 compatible via calibration)
- **Mounting:** Spool/bobbin that slides over pipe onto sensor housing
- **Spool Clearance:** 0.2mm over pipe OD
- **Coil Center Offset:** +4mm from pipe OD (spool wall + wire buildup)

---

## Product Tiers

| Tier | Pipe Sizes | Supply Voltage | Target Current | Wire Gauge |
|------|------------|----------------|----------------|------------|
| **MM-S** | 3/4" - 2" | 24V DC | 1.0A | 28 AWG |
| **MM-M** | 2.5" - 4" | 48V DC | 2.5A | 24 AWG |
| **MM-L** | 5" - 6" | 60V DC | 4.0A | 22 AWG |

---

## Wire Specifications

| Gauge | Resistance | Diameter | Current Capacity | Insulation |
|-------|------------|----------|------------------|------------|
| 28 AWG | 0.213 Ω/m | 0.321 mm | 1.4A | Polyurethane (Class 155) |
| 24 AWG | 0.084 Ω/m | 0.511 mm | 3.5A | Polyurethane (Class 155) |
| 22 AWG | 0.053 Ω/m | 0.644 mm | 5.5A | Polyurethane (Class 155) |

**Material:** Copper magnet wire with solderable polyurethane insulation

---

## MM-S Tier Coil Specifications

### Electrical Parameters

| Parameter | Value |
|-----------|-------|
| Wire Gauge | 28 AWG |
| Turns per Coil | 50 |
| Supply Voltage | 24V DC |
| Operating Current | 1.0A (PWM controlled) |
| Excitation Frequency | 2 kHz |

### Coil Dimensions by Pipe Size

| Pipe Size | Pipe OD | Spool ID | Coil Dia | Circumference |
|-----------|---------|----------|----------|---------------|
| 3/4" | 26.7 mm | 26.9 mm | 31 mm | 97 mm |
| 1" | 33.4 mm | 33.6 mm | 38 mm | 119 mm |
| 1.5" | 48.3 mm | 48.5 mm | 52 mm | 163 mm |
| 2" | 60.3 mm | 60.5 mm | 64 mm | 201 mm |

### Winding Specifications

| Pipe Size | Wire Length/Coil | Wire Length/Pair | Resistance/Coil | Total Resistance |
|-----------|------------------|------------------|-----------------|------------------|
| 3/4" | 4.9 m | 9.8 m | 1.0 Ω | **2.1 Ω** |
| 1" | 6.0 m | 12.0 m | 1.3 Ω | **2.6 Ω** |
| 1.5" | 8.2 m | 16.4 m | 1.7 Ω | **3.5 Ω** |
| 2" | 10.1 m | 20.2 m | 2.1 Ω | **4.3 Ω** |

### Electrical Characteristics at 24V

| Pipe Size | Total R | Max Current | PWM Duty @ 1A | Power @ 1A |
|-----------|---------|-------------|---------------|------------|
| 3/4" | 2.1 Ω | 11.4 A | 9% | 2.1 W |
| 1" | 2.6 Ω | 9.2 A | 11% | 2.6 W |
| 1.5" | 3.5 Ω | 6.9 A | 15% | 3.5 W |
| 2" | 4.3 Ω | 5.6 A | 18% | 4.3 W |

---

## MM-M Tier Coil Specifications

### Electrical Parameters

| Parameter | Value |
|-----------|-------|
| Wire Gauge | 24 AWG |
| Turns per Coil | 75 |
| Supply Voltage | 48V DC |
| Operating Current | 2.5A (PWM controlled) |
| Excitation Frequency | 2 kHz |

### Coil Dimensions by Pipe Size

| Pipe Size | Pipe OD | Spool ID | Coil Dia | Circumference |
|-----------|---------|----------|----------|---------------|
| 2.5" | 73.0 mm | 73.2 mm | 77 mm | 242 mm |
| 3" | 88.9 mm | 89.1 mm | 93 mm | 292 mm |
| 4" | 114.3 mm | 114.5 mm | 118 mm | 371 mm |

### Winding Specifications

| Pipe Size | Wire Length/Coil | Wire Length/Pair | Resistance/Coil | Total Resistance |
|-----------|------------------|------------------|-----------------|------------------|
| 2.5" | 18.2 m | 36.4 m | 1.5 Ω | **3.1 Ω** |
| 3" | 21.9 m | 43.8 m | 1.8 Ω | **3.7 Ω** |
| 4" | 27.8 m | 55.6 m | 2.3 Ω | **4.7 Ω** |

### Electrical Characteristics at 48V

| Pipe Size | Total R | Max Current | PWM Duty @ 2.5A | Power @ 2.5A |
|-----------|---------|-------------|-----------------|--------------|
| 2.5" | 3.1 Ω | 15.5 A | 16% | 19 W |
| 3" | 3.7 Ω | 13.0 A | 19% | 23 W |
| 4" | 4.7 Ω | 10.2 A | 25% | 29 W |

---

## MM-L Tier Coil Specifications

### Electrical Parameters

| Parameter | Value |
|-----------|-------|
| Wire Gauge | 22 AWG |
| Turns per Coil | 100 |
| Supply Voltage | 60V DC |
| Operating Current | 4.0A (PWM controlled) |
| Excitation Frequency | 2 kHz |

### Coil Dimensions by Pipe Size

| Pipe Size | Pipe OD | Spool ID | Coil Dia | Circumference |
|-----------|---------|----------|----------|---------------|
| 5" | 141.3 mm | 141.5 mm | 145 mm | 456 mm |
| 6" | 168.3 mm | 168.5 mm | 172 mm | 540 mm |

### Winding Specifications

| Pipe Size | Wire Length/Coil | Wire Length/Pair | Resistance/Coil | Total Resistance |
|-----------|------------------|------------------|-----------------|------------------|
| 5" | 45.6 m | 91.2 m | 2.4 Ω | **4.8 Ω** |
| 6" | 54.0 m | 108.0 m | 2.9 Ω | **5.7 Ω** |

### Electrical Characteristics at 60V

| Pipe Size | Total R | Max Current | PWM Duty @ 4A | Power @ 4A |
|-----------|---------|-------------|---------------|------------|
| 5" | 4.8 Ω | 12.5 A | 32% | 77 W |
| 6" | 5.7 Ω | 10.5 A | 38% | 91 W |

---

## Spool/Bobbin Specifications

### Material
- **Recommended:** Glass-filled nylon (PA66-GF30) or POM (Delrin)
- **Alternative:** 3D printed PETG or ABS for prototypes

### Dimensions

| Dimension | Formula | Notes |
|-----------|---------|-------|
| Inner Diameter | Pipe OD + 0.2 mm | Sliding fit over pipe |
| Outer Diameter | Inner Dia + 8 mm | Allows for wall + wire |
| Flange Width | Wire Dia × Layers + 2 mm | Contains windings |
| Wall Thickness | 2.0 mm minimum | Structural integrity |

### Features
- Alignment notch for positioning on sensor housing
- Wire entry/exit slots
- Mounting tabs (optional, for securing to housing)

---

## Winding Instructions

### Equipment Required
- Coil winding machine or lathe with turn counter
- Wire tensioner (50-100g tension for 28 AWG, scale up for thicker wire)
- Soldering iron with fine tip
- Heat shrink tubing (3mm)

### Procedure

1. **Prepare Spool**
   - Verify spool ID matches specification for pipe size
   - Clean any debris from winding area

2. **Secure Start Wire**
   - Thread wire through entry slot
   - Leave 150mm tail for connection
   - Apply small amount of adhesive to secure

3. **Wind Coil**
   - Maintain consistent tension throughout
   - Wind in neat layers (approximately 10 turns per layer for 50-turn coil)
   - Keep turns tight against each other
   - Count turns carefully

4. **Secure End Wire**
   - Thread wire through exit slot
   - Leave 150mm tail for connection
   - Apply adhesive to secure final turns

5. **Terminate Wires**
   - Strip 5mm of insulation from each tail
   - Tin wire ends with solder
   - Apply heat shrink for strain relief

6. **Test**
   - Measure resistance (should match specification ±5%)
   - Check for shorts between windings and spool
   - Verify continuity

---

## Quality Control

### Acceptance Criteria

| Parameter | Tolerance | Test Method |
|-----------|-----------|-------------|
| Resistance | ±5% of nominal | DMM measurement |
| Turn Count | Exact | Visual/counter |
| Insulation | >100 MΩ @ 500V | Megger test |
| Continuity | <0.1 Ω variation | DMM measurement |

### Inspection Checklist

- [ ] Correct wire gauge used
- [ ] Correct number of turns
- [ ] Resistance within tolerance
- [ ] No visible wire damage
- [ ] Wire tails properly terminated
- [ ] Spool ID matches pipe size
- [ ] No loose windings

---

## Helmholtz Pair Assembly

### Coil Spacing

For optimal field uniformity, Helmholtz coils should be spaced at a distance equal to the coil radius (R):

```
Spacing = Coil Radius = Coil Diameter / 2
```

| Pipe Size | Coil Diameter | Optimal Spacing |
|-----------|---------------|-----------------|
| 3/4" | 31 mm | 15.5 mm |
| 1" | 38 mm | 19 mm |
| 1.5" | 52 mm | 26 mm |
| 2" | 64 mm | 32 mm |
| 2.5" | 77 mm | 38.5 mm |
| 3" | 93 mm | 46.5 mm |
| 4" | 118 mm | 59 mm |
| 5" | 145 mm | 72.5 mm |
| 6" | 172 mm | 86 mm |

### Wiring

Both coils must be wired in series with the same polarity (magnetic fields add):

```
COIL+ ────► Coil 1 (+) ──► Coil 1 (-) ────► Coil 2 (+) ──► Coil 2 (-) ────► COIL-
```

**Verify:** With DC current applied, both coils should attract a magnet on the same side (field direction matches).

---

## Thermal Considerations

### Duty Cycle Operation

The magmeter operates with duty-cycled excitation to manage heat:

| Parameter | Default Value |
|-----------|---------------|
| Measurement Time | 1.1 seconds |
| Sleep Time | 13.9 seconds |
| Cycle Period | 15 seconds |
| Duty Cycle | 7.3% |

### Average Power Dissipation

| Tier | Peak Power | Average Power (7.3% duty) |
|------|------------|---------------------------|
| MM-S | 2.1 - 4.3 W | 0.15 - 0.31 W |
| MM-M | 19 - 29 W | 1.4 - 2.1 W |
| MM-L | 77 - 91 W | 5.6 - 6.6 W |

### Temperature Rise

Expected temperature rise above ambient (in still air):

| Tier | Estimated Rise |
|------|----------------|
| MM-S | < 5°C |
| MM-M | 10-15°C |
| MM-L | 20-30°C |

**Note:** MM-L installations may benefit from passive heat sinking or increased sleep time.

---

## Bill of Materials (per Helmholtz pair)

### MM-S Tier

| Item | 3/4" | 1" | 1.5" | 2" |
|------|------|-----|------|-----|
| 28 AWG Wire | 10 m | 12 m | 17 m | 21 m |
| Spool (qty 2) | Custom | Custom | Custom | Custom |
| Heat Shrink 3mm | 4 pcs | 4 pcs | 4 pcs | 4 pcs |

### MM-M Tier

| Item | 2.5" | 3" | 4" |
|------|------|-----|-----|
| 24 AWG Wire | 37 m | 44 m | 56 m |
| Spool (qty 2) | Custom | Custom | Custom |
| Heat Shrink 3mm | 4 pcs | 4 pcs | 4 pcs |

### MM-L Tier

| Item | 5" | 6" |
|------|-----|-----|
| 22 AWG Wire | 92 m | 108 m |
| Spool (qty 2) | Custom | Custom |
| Heat Shrink 3mm | 4 pcs | 4 pcs |

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-12 | AgSys Engineering | Initial release |

---

## Appendix A: Pipe Dimensions Reference (Schedule 40)

| Nominal Size | OD (mm) | OD (in) | ID (mm) | ID (in) | Wall (mm) |
|--------------|---------|---------|---------|---------|-----------|
| 3/4" | 26.7 | 1.050 | 20.9 | 0.824 | 2.9 |
| 1" | 33.4 | 1.315 | 26.6 | 1.049 | 3.4 |
| 1.5" | 48.3 | 1.900 | 40.9 | 1.610 | 3.7 |
| 2" | 60.3 | 2.375 | 52.5 | 2.067 | 3.9 |
| 2.5" | 73.0 | 2.875 | 62.7 | 2.469 | 5.2 |
| 3" | 88.9 | 3.500 | 77.9 | 3.068 | 5.5 |
| 4" | 114.3 | 4.500 | 102.3 | 4.026 | 6.0 |
| 5" | 141.3 | 5.563 | 128.2 | 5.047 | 6.6 |
| 6" | 168.3 | 6.625 | 154.1 | 6.065 | 7.1 |

---

## Appendix B: Wire Gauge Reference

| AWG | Diameter (mm) | Resistance (Ω/m) | Current (A)* |
|-----|---------------|------------------|--------------|
| 20 | 0.812 | 0.033 | 7.5 |
| 22 | 0.644 | 0.053 | 5.5 |
| 24 | 0.511 | 0.084 | 3.5 |
| 26 | 0.405 | 0.134 | 2.2 |
| 28 | 0.321 | 0.213 | 1.4 |
| 30 | 0.255 | 0.339 | 0.9 |

*Current capacity for chassis wiring; magnet wire in coil may be derated.
