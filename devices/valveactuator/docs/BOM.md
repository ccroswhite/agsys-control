# Valve Actuator Bill of Materials

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
| SW1 | DIP Switch | 10-position | 1 | $0.35 | $0.35 | 1-6: Address, 10: Term |
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

## DIP Switch Configuration

| Switch | Function | ON | OFF |
|--------|----------|-----|-----|
| 1 | Address bit 0 | +1 | +0 |
| 2 | Address bit 1 | +2 | +0 |
| 3 | Address bit 2 | +4 | +0 |
| 4 | Address bit 3 | +8 | +0 |
| 5 | Address bit 4 | +16 | +0 |
| 6 | Address bit 5 | +32 | +0 |
| 7 | Reserved | - | - |
| 8 | Reserved | - | - |
| 9 | Reserved | - | - |
| 10 | CAN Termination | Enabled | Disabled |

**Address Examples:**
- Address 1: Switch 1 ON, all others OFF
- Address 5: Switches 1 and 3 ON
- Address 32: Switch 6 ON
- Address 63: Switches 1-6 all ON (maximum)
- Last actuator in chain: Set switch 10 ON for termination
