# AgSys Unified BLE Architecture

## Overview

All AgSys IoT devices share a common BLE service library that provides:
- Consistent 6-digit PIN authentication
- Device identification
- Feature-based characteristics (enabled via compile-time flags)

## Advertising Data

All AgSys devices broadcast the following in their advertising packets:

1. **Service UUID** - `AGSYS001-4167-5379-732D-4D6167000000`
2. **Device Name** - e.g., "AgSys ValveCtrl"
3. **Manufacturer Data** - Device type and UID for pre-connect identification

### Manufacturer Data Format
```
[Company ID: 2 bytes][Device Type: 1 byte][UID: 8 bytes]
```

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 2 | Company ID (0xFFFF for dev, register with Bluetooth SIG for production) |
| 2 | 1 | Device Type (0x01=Soil, 0x02=Valve, 0x03=Meter) |
| 3 | 8 | Device UID (from nRF52 FICR) |

This allows the mobile app to identify device type and UID **before connecting**, enabling:
- Filtering by device type in scan results
- Showing friendly names based on device type
- Matching against known/registered devices

## UUID Scheme

All AgSys BLE services use a common base UUID:
```
AGSYS-xxxx-4167-5379-732D-4D6167000000
      ^^^^
      Feature ID
```

### Service UUIDs

| ID | Service | Description |
|----|---------|-------------|
| 0x0001 | AgSys Device | Main service (all devices) |

### Characteristic UUIDs

| ID | Characteristic | Properties | Description |
|----|----------------|------------|-------------|
| 0x0010 | Device Info | Read | UID, device type, firmware version |
| 0x0011 | PIN Auth | Read/Write | 6-digit PIN authentication |
| 0x0012 | PIN Change | Write | Change PIN (requires auth) |
| 0x0020 | Settings | Read/Write | Device settings (auth required) |
| 0x0021 | Live Data | Read/Notify | Real-time sensor data |
| 0x0030 | Valve Command | Write | Valve control (auth required) |
| 0x0031 | Valve Status | Read/Notify | Valve/actuator status |
| 0x0032 | CAN Discovery | Read/Write/Notify | Trigger CAN discovery |
| 0x0033 | Actuator List | Read | List of discovered actuators |
| 0x0040 | Calibration | Read/Write | Calibration data (auth required) |
| 0x0041 | Cal Command | Write | Calibration commands |
| 0x0050 | Diagnostics | Read | Device diagnostics |
| 0x0051 | Debug Log | Read | Debug log data |

## Feature Flags

Each device defines which features are enabled in `ble_features.h`:

```c
// Example: Valve Controller
#define AGSYS_BLE_FEATURE_AUTH          1
#define AGSYS_BLE_FEATURE_DEVICE_INFO   1
#define AGSYS_BLE_FEATURE_SETTINGS      0
#define AGSYS_BLE_FEATURE_LIVE_DATA     0
#define AGSYS_BLE_FEATURE_VALVE         1
#define AGSYS_BLE_FEATURE_CAN_DISCOVERY 1
#define AGSYS_BLE_FEATURE_CALIBRATION   0
#define AGSYS_BLE_FEATURE_DIAGNOSTICS   1
```

## Device Types

| Type ID | Device | Features |
|---------|--------|----------|
| 0x01 | Soil Moisture Sensor | Auth, DeviceInfo, Calibration, Diagnostics, DFU |
| 0x02 | Valve Controller | Auth, DeviceInfo, Valve, CAN Discovery, Diagnostics, DFU |
| 0x03 | Water Meter | Auth, DeviceInfo, Settings, LiveData, Calibration, Diagnostics, DFU |
| 0x04 | Valve Actuator | Auth, DeviceInfo, Diagnostics, DFU |

**Note:** Valve Actuators (nRF52810) can ONLY be updated via BLE DFU - they have no LoRa and communicate via CAN bus to the Valve Controller.

## Authentication

### 6-Digit PIN
- Default PIN: `123456`
- Stored in FRAM at device-specific address
- 3 failed attempts = 5 minute lockout
- Session timeout: 5 minutes

### Auth Flow
1. Mobile app connects to device
2. Reads Device Info to identify device type
3. Writes 6-digit PIN to PIN Auth characteristic
4. Reads PIN Auth status (0x01 = authenticated)
5. Can now access protected characteristics

### Status Codes
| Code | Meaning |
|------|---------|
| 0x00 | Not authenticated |
| 0x01 | Authenticated |
| 0x02 | Failed (wrong PIN) |
| 0x03 | Locked out |
| 0x04 | PIN changed |

## Data Structures

### Device Info (Read)
```c
struct {
    uint8_t  uid[8];        // Device unique ID
    uint8_t  deviceType;    // Device type (0x01-0x03)
    uint8_t  fwMajor;       // Firmware version
    uint8_t  fwMinor;
    uint8_t  fwPatch;
    uint16_t features;      // Bitmask of enabled features
} // 14 bytes
```

### Settings (Read/Write) - Water Meter
```c
struct {
    uint8_t  unitSystem;    // 0=metric, 1=imperial
    uint16_t trendPeriodMin;
    uint16_t avgPeriodMin;
    uint16_t maxFlowLPM;
    uint8_t  backlightOn;
} // 8 bytes
```

### Live Data (Notify) - Water Meter
```c
struct {
    float    flowRate;      // L/min or GPM
    float    totalVolume;   // Liters or gallons
    float    trendVolume;
    float    avgVolume;
    uint8_t  direction;     // 0=none, 1=forward, 2=reverse
    uint8_t  flags;
} // 18 bytes
```

### Valve Command (Write)
```c
struct {
    uint8_t  command;       // 0x01=open, 0x02=close, 0x03=stop, 0x04=query
    uint8_t  address;       // Actuator address (1-64) or 0xFF=all
    uint16_t durationSec;   // Duration (optional)
} // 4 bytes
```

### Calibration (Read/Write) - Soil Moisture
```c
struct {
    uint8_t  probeIndex;    // 0-3
    uint32_t fAir;          // Air frequency
    uint32_t fDry;          // Dry soil frequency
    uint32_t fWet;          // Wet soil frequency
} // 13 bytes
```

### Calibration (Read/Write) - Water Meter
```c
struct {
    int32_t  zeroOffset;
    float    spanFactor;
    float    kFactor;
    uint32_t calDate;
} // 16 bytes
```

## File Structure

```
common/
├── include/
│   ├── agsys_ble.h           # Main API
│   └── agsys_ble_features.h  # Feature flag template
├── src/
│   └── agsys_ble.cpp         # Implementation
└── docs/
    └── BLE_ARCHITECTURE.md   # This document

valvecontrol/
└── include/
    └── ble_features.h        # Device-specific features

watermeter/
└── include/
    └── ble_features.h        # Device-specific features

soilmoisture/
└── include/
    └── ble_features.h        # Device-specific features
```

## Usage

### Device Implementation

```c
#include "agsys_ble.h"

void setup() {
    // Initialize BLE with FRAM address for PIN storage
    agsys_ble_init(FRAM_ADDR_BLE_PIN);
    
    // Set device-specific callbacks
    agsys_ble_set_valve_callback(onValveCommand);
    agsys_ble_set_settings_callback(onSettingsChange);
    
    // Start advertising when ready
    agsys_ble_start_advertising();
}

void loop() {
    agsys_ble_process();
    
    // Update live data if connected
    if (agsys_ble_is_connected()) {
        agsys_ble_update_live_data(&liveData);
    }
}
```

### Mobile App (Flutter)

```dart
// Scan for AgSys devices
final devices = await FlutterBluePlus.scanResults
    .where((r) => r.advertisementData.serviceUuids
        .contains(Guid('AGSYS001-4167-5379-732D-4D6167000000')));

// Connect and authenticate
await device.connect();
final service = await device.discoverServices()
    .firstWhere((s) => s.uuid == agsysServiceUuid);

// Read device info
final deviceInfo = await service.characteristics
    .firstWhere((c) => c.uuid == deviceInfoUuid).read();

// Authenticate with PIN
await service.characteristics
    .firstWhere((c) => c.uuid == pinAuthUuid)
    .write(utf8.encode('123456'));

// Check auth status
final status = await pinAuthChar.read();
if (status[0] == 0x01) {
    // Authenticated - can access protected characteristics
}
```
