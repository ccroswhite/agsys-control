# Solenoid Valve Actuator Bill of Materials

## Design Specifications

- **Solenoid Voltage**: 24V AC
- **Switching**: TRIAC with optoisolated driver (MOC3021 + BTA06-600B)
- **Valve Type**: Configurable NO (Normally Open) or NC (Normally Closed) via DIP switch
- **Position Sensing**: None (solenoid state = energized state)
- **Communication**: CAN bus for valve control, BLE for OTA firmware updates
- **MCU**: nRF52832 (BLE 5.0 for OTA support)
- **Power**: 24V AC → rectified → 3.3V buck → 2.8V LDO for MCU

## Pin Assignments

| Signal | Pin | Description |
|--------|-----|-------------|
| CAN_SCK | P0.14 | CAN SPI Clock |
| CAN_MOSI | P0.12 | CAN SPI MOSI |
| CAN_MISO | P0.13 | CAN SPI MISO |
| CAN_CS | P0.11 | MCP2515 Chip Select |
| CAN_INT | P0.08 | MCP2515 Interrupt |
| MEM_SCK | P0.26 | Memory SPI Clock (standard) |
| MEM_MOSI | P0.25 | Memory SPI MOSI (standard) |
| MEM_MISO | P0.24 | Memory SPI MISO (standard) |
| FRAM_CS | P0.23 | FRAM Chip Select (standard) |
| FLASH_CS | P0.22 | Flash Chip Select (standard) |
| SOLENOID_CTRL | P0.03 | TRIAC control (via optocoupler) |
| ZERO_CROSS | P0.04 | AC zero-cross detection input |
| LED_POWER | P0.07 | 3.3V indicator |
| LED_AC | P0.21 | 24V AC present indicator |
| LED_STATUS | P0.29 | Status/Pairing LED |
| LED_VALVE | P0.30 | Valve energized indicator |
| PAIRING_BTN | P0.31 | BLE pairing button (hold 3s) |
| DIP_NONC | P0.27 | NO/NC configuration switch |
| DIP_TERM | P0.28 | CAN termination switch |
| DIP_1-6 | P0.15-P0.20 | Address DIP switches |

---

## Solenoid Valve Actuator Board (per unit)

| Ref | Component | Part Number | Qty | Unit Price | Total | Notes |
|-----|-----------|-------------|-----|------------|-------|-------|
| U1 | MCU | nRF52832-QFAA | 1 | $2.50 | $2.50 | ARM Cortex-M4, BLE 5.0 for OTA |
| U2 | CAN Controller | MCP2515-I/SO | 1 | $1.00 | $1.00 | SPI interface |
| U3 | CAN Transceiver | SN65HVD230DR | 1 | $0.30 | $0.30 | 3.3V compatible |
| U4 | Buck Converter | TPS54202DDCR | 1 | $0.50 | $0.50 | 40V input to 3.3V |
| **U8** | **LDO** | **TLV73325PDBVR** | 1 | **$0.30** | **$0.30** | **3.3V to 2.5V for MCU** |
| U5 | FRAM | MB85RS1MTPNF | 1 | $1.30 | $1.30 | 128KB SPI, config + logs |
| U6 | Flash | W25Q16JVSSIQ | 1 | $0.50 | $0.50 | 2MB SPI, OTA firmware |
| **U7** | **Optocoupler** | **MOC3021** | 1 | **$0.40** | **$0.40** | **Zero-cross TRIAC driver** |
| **T1** | **TRIAC** | **BTA06-600B** | 1 | **$0.50** | **$0.50** | **6A 600V, TO-220** |
| **BR1** | **Bridge Rectifier** | **MB6S** | 1 | **$0.15** | **$0.15** | **600V 0.5A SOIC-4** |
| L1 | Inductor | 10µH 2A 1210 | 1 | $0.15 | $0.15 | Buck converter |
| L2 | Inductor | 10nH 0402 | 1 | $0.05 | $0.05 | nRF52832 DC-DC |
| **MOV1** | **MOV** | **V33ZA2** | 1 | **$0.20** | **$0.20** | **AC surge protection** |
| D5 | TVS | SMBJ40A | 1 | $0.15 | $0.15 | DC rail protection |
| D9 | TVS | PESD5V0S1BL | 1 | $0.05 | $0.05 | Pairing button ESD |
| D10 | Diode | BAV99 | 1 | $0.02 | $0.02 | Zero-cross clamp |
| F1 | PTC Fuse | MF-MSMF100 | 1 | $0.10 | $0.10 | 1A resettable |
| Y1 | Crystal | 16MHz HC49 | 1 | $0.15 | $0.15 | For MCP2515 |
| Y2 | Crystal | 32MHz 2520 | 1 | $0.25 | $0.25 | For nRF52832 (optional) |
| LED1-4 | LED | 0603 GYRB | 4 | $0.02 | $0.08 | Status indicators |
| SW1 | DIP Switch | 10-position SMD | 1 | $0.35 | $0.35 | Address + Config |
| SW2 | Tactile Switch | 6x6mm | 1 | $0.05 | $0.05 | BLE Pairing button |
| J1 | Solenoid Connector | Phoenix XPC 2-pin | 1 | $0.75 | $0.75 | Solenoid output |
| J2,J3 | CAN Connector | Phoenix XPC 4-pin | 2 | $1.00 | $2.00 | Daisy chain |
| J4 | AC Input Connector | Phoenix XPC 2-pin | 1 | $0.75 | $0.75 | 24V AC input |
| R1 | Resistor | 100K 0603 | 1 | $0.01 | $0.01 | Buck feedback |
| R2 | Resistor | 31.6K 0603 | 1 | $0.01 | $0.01 | Buck feedback |
| R10 | Resistor | 120R 0603 | 1 | $0.01 | $0.01 | CAN termination |
| R11-R14 | Resistor | 1K 0402 | 4 | $0.01 | $0.04 | LED current limit |
| R15 | Resistor | 10K 0402 | 1 | $0.01 | $0.01 | Pairing button pull-up |
| R30 | Resistor | 330R 0603 | 1 | $0.01 | $0.01 | TRIAC gate resistor |
| R31 | Resistor | 100R/1W 2512 | 1 | $0.05 | $0.05 | Snubber resistor |
| R32 | Resistor | 180R 0402 | 1 | $0.01 | $0.01 | Optocoupler LED limit |
| R33,R34 | Resistor | 100K 0603 | 2 | $0.01 | $0.02 | Zero-cross divider |
| R35 | Resistor | 10K 0402 | 1 | $0.01 | $0.01 | Zero-cross pull-down |
| RN1 | Resistor Array | 10K x 8 0603x4 | 1 | $0.05 | $0.05 | DIP switch pull-ups |
| R22,R23 | Resistor | 10K 0402 | 2 | $0.01 | $0.02 | DIP switch pull-ups |
| **C1** | **Capacitor** | **100uF/50V** | 1 | **$0.30** | **$0.30** | **Rectifier filter** |
| C2 | Capacitor | 22uF/10V 0805 | 1 | $0.03 | $0.03 | Buck output |
| C8 | Capacitor | 100nF 0402 | 1 | $0.01 | $0.01 | Buck decoupling |
| C10,C11 | Capacitor | 22pF 0402 | 2 | $0.01 | $0.02 | MCP2515 crystal load |
| C12,C13 | Capacitor | 100nF 0402 | 2 | $0.01 | $0.02 | CAN IC decoupling |
| C20,C21 | Capacitor | 12pF 0402 | 2 | $0.01 | $0.02 | nRF52832 crystal load |
| C22-C27 | Capacitor | Various 0402 | 6 | $0.01 | $0.06 | nRF52832 decoupling |
| **C30** | **Capacitor** | **100nF/250V** | 1 | **$0.15** | **$0.15** | **Snubber capacitor** |
| C50,C51 | Capacitor | 1uF 0402 | 2 | $0.01 | $0.02 | LDO in/out caps |
| C40,C41 | Capacitor | 100nF 0402 | 2 | $0.01 | $0.02 | FRAM/Flash decoupling |
| C3-C7 | Capacitor | 100nF 0402 | 5 | $0.01 | $0.05 | General decoupling |
| C9 | Capacitor | 100nF 0402 | 1 | $0.01 | $0.01 | Pairing button debounce |
| - | PCB | 2-layer | 1 | $0.75 | $0.75 | |
| | | | | **Total** | **~$14.20** | |

## DIP Switch Configuration

| Switch | Function | ON | OFF |
|--------|----------|-----|-----|
| 1 | Address bit 0 | +1 | +0 |
| 2 | Address bit 1 | +2 | +0 |
| 3 | Address bit 2 | +4 | +0 |
| 4 | Address bit 3 | +8 | +0 |
| 5 | Address bit 4 | +16 | +0 |
| 6 | Address bit 5 | +32 | +0 |
| **7** | **Valve Type** | **NC** | **NO** |
| 8 | Reserved | - | - |
| 9 | Reserved | - | - |
| 10 | CAN Termination | Enabled | Disabled |

**Valve Type Configuration (SW7):**
- **NO (Normally Open)** - SW7 OFF: Valve is open when de-energized, closes when energized
- **NC (Normally Closed)** - SW7 ON: Valve is closed when de-energized, opens when energized

**Address Examples:**
- Address 1: Switch 1 ON, all others OFF
- Address 5: Switches 1 and 3 ON
- Address 32: Switch 6 ON
- Address 63: Switches 1-6 all ON (maximum)
- Last actuator in chain: Set switch 10 ON for termination

## BLE Pairing for OTA Updates

The solenoid valve actuator supports BLE for OTA firmware updates only. No configuration is available over BLE.

### Entering Pairing Mode

1. **Hold the pairing button (SW2) for 3 seconds** at power-on or during operation
2. **Status LED (LED3) will blink rapidly** (100ms on/off) to indicate pairing mode
3. **Pairing window is 2 minutes** - after timeout, device exits pairing mode automatically

### LED Patterns

| Pattern | Meaning |
|---------|---------|
| Fast blink (100ms) | BLE pairing mode active |
| Solid ON (LED4) | Solenoid energized |
| Fast blink (200ms) | Fault condition |
| OFF | Idle / Solenoid de-energized |

### OTA Update Process

1. Enter pairing mode on the actuator
2. Connect with mobile app via BLE
3. Authenticate with device PIN (stored in FRAM)
4. Transfer new firmware via BLE OTA service
5. Device automatically reboots and validates new firmware
6. If validation fails, bootloader rolls back to previous firmware

## Electrical Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| AC Input Voltage | 24V AC ±10% | 50/60 Hz |
| Max Solenoid Current | 1A | Limited by PTC fuse |
| TRIAC Rating | 6A / 600V | BTA06-600B |
| Isolation | 5kV | MOC3021 optocoupler |
| Snubber | 100Ω + 100nF | For inductive load |
| MCU Power | 2.8V @ 20mA | From 3.3V via LDO (low noise) |
| Peripheral Power | 3.3V | CAN, memory, LEDs, optocoupler |
| Standby Power | <0.5W | MCU + CAN only |
