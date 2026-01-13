# AgSys Field Service App

## Overview

A standalone mobile application for field technicians to install, configure, calibrate, and update AgSys IoT devices **without internet connectivity**.

This is separate from the main AgSys Flutter app (which requires cloud connectivity). The field service app is designed for:
- Remote vineyard locations with no cellular signal
- Initial device installation and commissioning
- Calibration with reference meters
- Firmware updates via BLE
- Troubleshooting and diagnostics

## Key Requirements

### 1. Fully Offline Operation
- **No internet required** after initial sync
- All firmware images stored locally on device
- Device configurations cached locally
- Sync to cloud when connectivity available (optional)

### 2. BLE-Only Communication
- Direct BLE connection to each device type
- PIN authentication (same as main app)
- All device commands via BLE characteristics
- No LoRa or cloud dependency

### 3. Pre-Downloaded Content
Before going to field, technician syncs:
- Latest firmware for each device type (soil moisture, valve controller, water meter)
- Device configuration templates
- Calibration procedures and checklists
- Property/zone assignments (if known)

## Supported Devices

| Device | BLE Name Pattern | Features |
|--------|------------------|----------|
| Soil Moisture | `AgSoil-XXXX` | Config, diagnostics, OTA |
| Valve Controller | `AgValve-XXXX` | Config, schedule test, OTA |
| Valve Actuator | `AgActuator-XXXX` | Config, position test, OTA |
| Water Meter | `AgMeter-XXXX` | Config, **calibration**, diagnostics, OTA |

## Water Meter Calibration Features

### Calibration Menu
```
Water Meter → Calibration
├── Zero Calibration
│   └── Requires: No flow, stable signal
├── Span Calibration  
│   └── Requires: Reference meter reading
├── Pipe Size Selection
│   └── 1.5" - 6" options
├── View Calibration Data
│   └── Zero offset, span, pipe diameter
└── Auto-Zero Settings
    └── Enable/disable automatic zero correction
```

### BLE Commands (Water Meter)

| Command | ID | Params | Description |
|---------|-----|--------|-------------|
| Zero Cal | 0x10 | none | Trigger zero calibration |
| Span Cal | 0x11 | float32 L/min | Calibrate with known flow |
| Set Pipe Size | 0x12 | uint8 (0-6) | Set pipe size enum |
| Reset Total | 0x13 | none | Reset totalizer |
| Get Cal Data | 0x14 | none | Read calibration values |
| Save Cal | 0x15 | none | Save to FRAM |
| Auto-Zero | 0x16 | uint8 (0/1) | Enable/disable |
| Set Duty Cycle | 0x17 | uint16 on_ms, uint16 off_ms | Thermal management |
| Get Duty Cycle | 0x18 | none | Read duty cycle + auto-zero |

### Response Format
```
[cmd_id: 1B] [status: 1B] [data: variable]

Status codes:
  0x00 = OK
  0x01 = Not ready
  0x02 = Invalid param
  0x03 = Cal failed
  0x04 = Not authenticated
```

## App Architecture

### Platform
- **Flutter** (cross-platform iOS/Android)
- Could be same codebase as main app with offline mode, or separate app
- Recommend: Separate app to keep main app simple

### Local Storage
```
/field_service_app/
├── firmware/
│   ├── soil_moisture_v1.2.0.bin
│   ├── valve_controller_v1.1.0.bin
│   ├── valve_actuator_v1.1.0.bin
│   └── water_meter_v1.0.0.bin
├── config/
│   ├── device_templates.json
│   └── calibration_defaults.json
├── cache/
│   ├── properties.json      (synced from cloud)
│   └── zones.json
└── logs/
    └── field_activity.json  (to sync later)
```

### Sync Flow
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Cloud     │     │  Field App  │     │   Device    │
│   Backend   │     │  (Mobile)   │     │   (BLE)     │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       │  Sync firmware    │                   │
       │  & configs        │                   │
       │◄─────────────────►│                   │
       │                   │                   │
       │    (go to field - no internet)        │
       │                   │                   │
       │                   │  BLE connect      │
       │                   │──────────────────►│
       │                   │                   │
       │                   │  Configure/Cal    │
       │                   │◄─────────────────►│
       │                   │                   │
       │                   │  OTA update       │
       │                   │──────────────────►│
       │                   │                   │
       │    (return to office - has internet)  │
       │                   │                   │
       │  Sync activity    │                   │
       │  logs             │                   │
       │◄──────────────────│                   │
       │                   │                   │
```

## UI Screens

### 1. Home / Device Scanner
- Scan for nearby BLE devices
- Filter by device type
- Show signal strength (RSSI)
- Quick connect to known devices

### 2. Device Dashboard
- Device type, serial, firmware version
- Connection status
- Quick actions (based on device type)

### 3. Water Meter Calibration
- **Zero Cal**: Instructions + trigger button
- **Span Cal**: Number input for reference flow + trigger
- **Pipe Size**: Picker with pipe options
- **View Data**: Read-only display of current cal values

### 4. Firmware Update
- Show current vs available version
- Download progress
- Update progress
- Rollback option if update fails

### 5. Diagnostics
- Live sensor readings
- Signal quality metrics
- Error logs
- Factory reset option

### 6. Activity Log
- All actions taken on devices
- Timestamps
- Success/failure status
- Queued for cloud sync

## Development Priority

### Phase 1: Core Functionality
- [ ] BLE scanning and connection
- [ ] PIN authentication
- [ ] Water meter calibration (zero, span, pipe size)
- [ ] Basic diagnostics view

### Phase 2: OTA Updates
- [ ] Firmware download from cloud (when online)
- [ ] Local firmware storage
- [ ] BLE OTA transfer
- [ ] Version management

### Phase 3: Full Device Support
- [ ] Soil moisture sensor config
- [ ] Valve controller config and schedule test
- [ ] Valve actuator position test

### Phase 4: Cloud Sync
- [ ] Property/zone data sync
- [ ] Activity log upload
- [ ] Firmware version sync

## Open Questions

1. **Separate app or offline mode in main app?**
   - Recommend: Separate app for simplicity
   - Main app stays focused on vineyard management
   - Field app focused on technician workflows

2. **Who uses this app?**
   - AgSys installation technicians
   - Trained vineyard maintenance staff
   - Not end-user vineyard managers

3. **App distribution?**
   - Internal distribution (not public app store)
   - Or: Public but requires technician login

4. **Firmware signing?**
   - Should field app verify firmware signatures?
   - Prevents loading unauthorized firmware
