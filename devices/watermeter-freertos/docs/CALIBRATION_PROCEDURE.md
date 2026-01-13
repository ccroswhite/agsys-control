# Mag Meter Calibration Procedure

## Overview

The electromagnetic flow meter requires calibration for accurate measurements. There are two calibration parameters:

1. **Zero Offset** - Residual signal when no water is flowing
2. **Span Coefficient** - Relationship between signal voltage and flow velocity

## Calibration Methods

### Method 1: Auto-Zero + Default Span (Recommended for Field Install)

This is the simplest method, suitable for most vineyard installations where ±5% accuracy is acceptable.

**Setup:**
1. Install meter on pipe
2. Select correct pipe size via display menu or BLE app
3. Power on meter

**Auto-Zero Process:**
- System automatically detects when flow stops (signal < 20µV, noise < 5µV)
- After 10 seconds of stable "zero" signal, auto-zero triggers
- Zero offset is saved to FRAM
- Minimum 5 minutes between auto-zeros to prevent hunting

**Span:**
- Uses default span coefficient based on pipe size
- Calculated from Faraday's law: V = B × D × v
- Typical accuracy: ±3-5%

### Method 2: Manual Zero + Reference Meter Span (High Accuracy)

For installations requiring ±1% accuracy.

**Equipment Needed:**
- Portable ultrasonic clamp-on flow meter (reference)
- Or inline turbine meter with known accuracy

**Zero Calibration:**
1. Close upstream valve - ensure no flow
2. Wait 30 seconds for signal to stabilize
3. Navigate to: `Settings → Calibration → Zero Cal`
4. Press SELECT to capture zero offset
5. Display shows: "Zero: XX.X µV"

**Span Calibration:**
1. Open valve - establish steady flow
2. Read flow rate from reference meter
3. Navigate to: `Settings → Calibration → Span Cal`
4. Enter known flow rate using UP/DOWN buttons
5. Press SELECT to calculate span coefficient
6. Display shows: "Span: XX.X µV/(m/s)"

### Method 3: Volumetric Calibration (No Reference Meter)

For field calibration without extra equipment.

**Equipment Needed:**
- Container of known volume (e.g., 5-gallon bucket = 18.93 L)
- Stopwatch (or use meter's built-in timer)

**Procedure:**
1. Navigate to: `Settings → Calibration → Volume Cal`
2. Enter container volume (liters)
3. Press SELECT to start
4. Fill container completely
5. Press SELECT to stop
6. System calculates: actual_flow = volume / time
7. Compares to measured flow, adjusts span

**Limitations:**
- Only practical for smaller pipes (< 3")
- Requires consistent flow during fill
- Messy - water must go somewhere

## Calibration Data Storage

Calibration is stored in FRAM at address `0x0B00` (128 bytes):

```c
typedef struct {
    uint32_t magic;              // 0x464C4F57 ("FLOW")
    uint8_t  version;            // Calibration format version
    uint8_t  pipe_size;          // Pipe size enum (0-6)
    uint8_t  tier;               // Board tier (S/M/L)
    float    zero_offset_uv;     // Zero offset in µV
    float    span_uv_per_mps;    // Span coefficient
    float    temp_coeff_offset;  // Temperature compensation
    float    temp_coeff_span;
    float    ref_temp_c;         // Reference temperature
    float    pipe_diameter_m;    // Actual pipe ID (can override default)
    uint32_t cal_date;           // Unix timestamp
    uint32_t serial_number;
    uint32_t crc32;              // Data integrity check
} flow_calibration_t;
```

## Default Span Coefficients

Based on Faraday's law with 500mA coil current at 2kHz:

| Pipe Size | Inner Diameter | Default Span (µV per m/s) |
|-----------|----------------|---------------------------|
| 1.5" Sch 80 | 38.1 mm | 150 |
| 2" Sch 80 | 52.5 mm | 180 |
| 2.5" Sch 40 | 63.5 mm | 200 |
| 3" Sch 40 | 77.9 mm | 220 |
| 4" Sch 40 | 102.3 mm | 250 |
| 5" Sch 40 | 128.2 mm | 280 |
| 6" Sch 40 | 154.1 mm | 300 |

*Note: These are theoretical values. Actual span depends on coil geometry, water conductivity, and installation.*

## Calibration via BLE App

The Flutter app can perform calibration remotely:

1. Connect to meter via BLE
2. Authenticate with PIN
3. Navigate to Device Settings → Calibration
4. Options:
   - **Zero Now** - Triggers immediate zero cal (requires no flow)
   - **Set Pipe Size** - Updates pipe diameter and default span
   - **Enter Reference Flow** - For span calibration with reference meter
   - **View Cal Data** - Shows current calibration values
   - **Reset to Defaults** - Clears calibration, uses factory defaults

## Verification

After calibration, verify accuracy:

1. Establish known flow rate
2. Compare meter reading to reference
3. Error should be < 1% for Method 2, < 5% for Method 1

## Troubleshooting

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| Reading drifts over time | Temperature change | Enable temp compensation or recalibrate |
| Non-zero reading with no flow | Zero offset drift | Run zero calibration |
| Consistent % error | Span incorrect | Run span calibration with reference |
| Erratic readings | Air bubbles in pipe | Ensure pipe is full, bleed air |
| Signal too low | Wrong pipe size selected | Verify pipe size setting |
| Signal saturated | Gain too high | System will auto-adjust gain |

## Factory Calibration

For production units, factory calibration uses:

1. Test rig with reference Coriolis meter (±0.1% accuracy)
2. Multiple flow points (10%, 25%, 50%, 75%, 100% of range)
3. Temperature sweep (10°C to 40°C) for temp coefficients
4. Calibration certificate generated with serial number
5. Data programmed to FRAM before shipping

## Recalibration Interval

Recommended recalibration:
- **Auto-zero**: Continuous (automatic)
- **Span check**: Annually or after pipe work
- **Full recalibration**: Every 2-3 years or if accuracy degrades
