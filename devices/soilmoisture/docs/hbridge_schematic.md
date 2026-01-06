# H-Bridge AC Capacitance Sensor - Schematic

## Overview

This circuit generates a 100kHz bipolar AC signal to drive a sealed capacitive soil moisture probe. The true AC signal prevents soil polarization/magnetization, enabling 10+ year probe life.

## Schematic

```
                                    VCC (2.5V)
                                       │
                                       │
            ┌──────────────────────────┼──────────────────────────┐
            │                          │                          │
            │                          │                          │
           ┌┴┐                        ┌┴┐                        ┌┴┐
           │ │ R1                     │ │ R_LIMIT                │ │ R2
           │ │ 10kΩ                   │ │ 1.1kΩ                  │ │ 10kΩ
           └┬┘                        └┬┘                        └┬┘
            │                          │                          │
            │    Q1 (P-ch)             │         Q2 (P-ch)        │
            │    SSM6P15FU             │         SSM6P15FU        │
            │      │                   │           │              │
   GPIO_A ──┴──────┤G                  │         G├──────┴── GPIO_B
                   │                   │           │
              S────┤                   │         ├────S
                   │                   │           │
                   └─────────┬─────────┴─────────┬─┘
                             │                   │
                            ┌┴┐                 ┌┴┐
                        D1  │▼│                 │▼│  D2
                      BAT54 └┬┘                 └┬┘  BAT54
                             │                   │
                             │    ┌─────────┐    │
                   PROBE_A ──┴────┤  PROBE  ├────┴── PROBE_B
                                  │ (sealed)│
                                  │capacitor│
                                  └────┬────┘
                                       │
                                   PROBE_OUT ──────┬─────► To Envelope Detector
                                       │          │
                             │                   │
                            ┌┴┐                 ┌┴┐
                        D3  │▲│                 │▲│  D4
                      BAT54 └┬┘                 └┬┘  BAT54
                             │                   │
                   ┌─────────┴─────────┬─────────┴─────────┐
                   │                   │                   │
              D────┤                   │         ├────D
                   │                   │           │
   GPIO_A ──┬──────┤G                  │         G├──────┬── GPIO_B
            │    Q3 (N-ch)             │         Q4 (N-ch)        │
            │    2SK2009               │         2SK2009          │
            │      │                   │           │              │
           ┌┴┐     S                  ┌┴┐          S             ┌┴┐
           │ │ R3                     │ │ R_LIMIT                │ │ R4
           │ │ 10kΩ                   │ │ 1.1kΩ                  │ │ 10kΩ
           └┬┘                        └┬┘                        └┬┘
            │                          │                          │
            └──────────────────────────┼──────────────────────────┘
                                       │
                                      GND


## Envelope Detector

                    From PROBE_OUT
                          │
                         ┌┴┐
                         │ │ R5 (10kΩ)
                         └┬┘
                          │
                          ├────────┬──────► ADC_IN (to nRF52832)
                          │        │
                         ┌┴┐      ═══ C1
                     D5  │▼│       │  100nF
                   BAT54 └┬┘       │
                          │        │
                         GND      GND
```

## Operation

### Phase 1: GPIO_A = HIGH, GPIO_B = LOW
- Q1 (P-ch) OFF, Q3 (N-ch) ON
- Q2 (P-ch) ON, Q4 (N-ch) OFF
- Current flows: VCC → Q2 → PROBE → Q3 → GND
- Probe sees +VCC

### Phase 2: GPIO_A = LOW, GPIO_B = HIGH
- Q1 (P-ch) ON, Q3 (N-ch) OFF
- Q2 (P-ch) OFF, Q4 (N-ch) ON
- Current flows: VCC → Q1 → PROBE → Q4 → GND
- Probe sees -VCC (reversed polarity)

### Result
- 100kHz square wave with true bipolar swing
- Net DC across probe = 0
- No polarization or electrolysis

## Bill of Materials

| Ref | Part Number | Description | Qty | Package |
|-----|-------------|-------------|-----|---------|
| Q1, Q2 | SSM6P15FU,LF | P-ch MOSFET, Vgs(th)=-0.5V | 2 | SOT-323F |
| Q3, Q4 | 2SK2009TE85LF | N-ch MOSFET, Vgs(th)=0.5V | 2 | SOT-323 |
| D1-D4 | BAT54S | Dual Schottky diode | 2 | SOT-23 |
| D5 | BAT54 | Schottky diode (envelope) | 1 | SOD-323 |
| R1-R4 | - | 10kΩ 0402 1% | 4 | 0402 |
| R_LIMIT | - | 1.1kΩ 0402 1% (×2) | 2 | 0402 |
| R5 | - | 10kΩ 0402 1% | 1 | 0402 |
| C1 | - | 100nF 0402 X7R | 1 | 0402 |

**Total component cost: ~$0.50**

## Electrical Specifications

| Parameter | Value |
|-----------|-------|
| Operating voltage | 2.5V |
| Drive frequency | 100 kHz |
| Probe current (max) | ~1.1 mA |
| Power consumption | ~7 mW during measurement |
| Measurement duration | 1 second |
| Energy per measurement | ~7 mJ (~2 µAh) |

## GPIO Connections (nRF52832)

| Signal | nRF52 Pin | Function |
|--------|-----------|----------|
| GPIO_A | P0.14 | H-bridge drive A (Q1 gate inverted, Q3 gate direct) |
| GPIO_B | P0.15 | H-bridge drive B (Q2 gate inverted, Q4 gate direct) |
| ADC_IN | P0.02 (AIN0) | Envelope detector output |
| PWR_EN | P0.16 | H-bridge power enable (optional) |

## Notes

1. **Gate drive**: P-channel MOSFETs turn ON when gate is LOW relative to source. The 10kΩ pull-up resistors ensure P-ch MOSFETs are OFF when GPIO is HIGH.

2. **Flyback diodes**: BAT54S provides protection during switching transitions. Essential for inductive probe cables.

3. **Current limiting**: 2.2kΩ total series resistance (1.1kΩ × 2) limits probe current to ~1.1mA.

4. **Envelope detector**: Simple peak detector converts AC amplitude to DC for ADC reading. Time constant = 10kΩ × 100nF = 1ms.

5. **Probe design**: Sealed capacitive probe with stainless steel or copper electrodes encapsulated in marine-grade epoxy. Electrodes never contact soil directly.
