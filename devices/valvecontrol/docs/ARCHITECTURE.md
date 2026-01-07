# Valve Controller Architecture

## Overview

The Valve Controller manages up to 64 valve actuators via CAN bus, communicates with the property controller via LoRa, and supports BLE for local configuration.

## System Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     PROPERTY CONTROLLER                          │
│                    (Raspberry Pi Leader)                         │
└──────────────────────────┬──────────────────────────────────────┘
                           │ LoRa (915 MHz)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                    VALVE CONTROLLER                              │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ nRF52832 + RFM95C LoRa + MCP2515 CAN                        ││
│  │  • BLE for local configuration                              ││
│  │  • FRAM for schedule storage                                ││
│  │  • RV-3028 RTC for accurate timing                          ││
│  │  • Power fail detection                                     ││
│  └─────────────────────────────────────────────────────────────┘│
│                           │                                      │
│                    CAN Bus (M12 connectors)                      │
│         ┌─────────┬───────┴───────┬─────────┐                   │
│         ▼         ▼               ▼         ▼                   │
│    ┌────────┐ ┌────────┐     ┌────────┐ ┌────────┐             │
│    │Actuator│ │Actuator│ ... │Actuator│ │Actuator│             │
│    │   1    │ │   2    │     │   63   │ │   64   │             │
│    └────────┘ └────────┘     └────────┘ └────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

## Hardware Components

### Valve Controller Board

| Component | Part Number | Description |
|-----------|-------------|-------------|
| MCU | nRF52832-QFAA | ARM Cortex-M4F, BLE |
| LoRa | RFM95C | 915 MHz transceiver |
| CAN Controller | MCP2515 | SPI-to-CAN bridge |
| CAN Transceiver | SN65HVD230 | 3.3V CAN transceiver |
| FRAM | FM25V02 | 256Kbit non-volatile storage |
| Flash | W25Q16 | 2MB SPI flash |
| RTC | RV-3028 | Ultra-low power RTC |
| LDO | MCP1700-3302E | 3.3V regulator |

### Valve Actuator Board

| Component | Part Number | Description |
|-----------|-------------|-------------|
| MCU | nRF52810-QFAA | ARM Cortex-M4, low cost |
| CAN Controller | MCP2515 | SPI-to-CAN bridge |
| CAN Transceiver | SN65HVD230 | 3.3V CAN transceiver |
| Buck Converter | TPS54202 | 24V to 3.3V |
| H-Bridge High | AO3401 (×2) | P-channel MOSFET |
| H-Bridge Low | AO3400 (×2) | N-channel MOSFET |
| Flyback Diodes | SS14 (×4) | Schottky diodes |
| TVS Diodes | SMBJ28A, SMBJ5.0A | Surge protection |

## Communication Protocols

### LoRa (Controller ↔ Property Controller)

- Frequency: 915 MHz (US915)
- Bandwidth: 125 kHz
- Spreading Factor: SF9
- Sync Word: 0x34 (private network)

Message types:
- Schedule pull request/response
- Status reports
- Proceed check (before scheduled irrigation)
- Emergency commands

### CAN Bus (Controller ↔ Actuators)

- Speed: 125 kbps
- Termination: 120Ω at both ends

| CAN ID | Direction | Description |
|--------|-----------|-------------|
| 0x100 | Ctrl → Act | Open valve |
| 0x101 | Ctrl → Act | Close valve |
| 0x102 | Ctrl → Act | Stop motor |
| 0x103 | Ctrl → Act | Query status |
| 0x1FF | Ctrl → All | Emergency close |
| 0x200+addr | Act → Ctrl | Status response |

## Power Architecture

```
External PSU (separate board)
    │
    ├── 24V DC ──► CAN bus power + valve motors
    │
    ├── Power Fail Signal ──► GPIO interrupt
    │
    └── 3.3V (via LDO) ──► Controller logic
```

### Power Fail Behavior

1. AC loss detected via optocoupler on PSU board
2. Power fail signal goes LOW
3. Controller immediately sends emergency close to all actuators
4. Event logged to FRAM
5. Enter low-power mode (battery conservation)
6. When AC restored, resume normal operation

## Schedule Storage

Schedules stored in FRAM (FM25V02, 32KB):

| Address | Size | Content |
|---------|------|---------|
| 0x0000 | 256B | Device configuration |
| 0x0100 | 8KB | Schedule entries (256 max) |
| 0x2100 | 1KB | Actuator state cache |
| 0x2500 | 22KB | Event log |

### Schedule Entry Structure (12 bytes)

```c
typedef struct {
    uint8_t valve_id;           // 1-64
    uint8_t days_of_week;       // Bitmask
    uint16_t start_time_min;    // Minutes from midnight
    uint16_t duration_min;      // Duration
    uint16_t target_gallons;    // Volume target
    uint8_t priority;           // Conflict resolution
    uint8_t flags;              // Enabled, skip if wet, etc.
    uint8_t flow_group;         // Shared supply line
    uint8_t max_concurrent;     // Max simultaneous zones
} ScheduleEntry;
```

## LED Indicators

### Controller

| LED | Color | Meaning |
|-----|-------|---------|
| PWR_3V3 | Green | 3.3V present |
| PWR_24V | Yellow | 24V present |
| STATUS | Red | Error/status patterns |

### Actuator

| LED | Color | Meaning |
|-----|-------|---------|
| PWR_3V3 | Green | 3.3V present |
| PWR_24V | Yellow | 24V present |
| STATUS | Red | Error/status patterns |
| VALVE_OPEN | Blue | Valve in open position |

## BLE Configuration

- Device name: "ValveCtrl"
- Pairing: 2-second button hold
- Timeout: 5 minutes
- Services: Configuration, schedule management, diagnostics
