# BLE Pairing Mode - Standardized Behavior

## Overview

All AgSys devices use a consistent BLE pairing mode for device configuration and firmware updates (DFU). This document defines the standardized behavior across all device types.

## Pairing Mode Entry

### Button Hold
- **Hold Time:** 2 seconds (`PAIRING_BUTTON_HOLD_MS = 2000`)
- **Button State:** Active LOW (pressed = LOW)
- **Feedback:** LED blinks 5 times rapidly (100ms on/off), then slow blink while in pairing mode

### First Boot (Soil Moisture Only)
- Soil moisture sensors automatically enter pairing mode on first boot for initial calibration
- This allows field calibration immediately after deployment

## Pairing Mode Behavior

### Timeout
- **Duration:** 5 minutes (`BLE_PAIRING_TIMEOUT_MS = 300000`)
- After timeout, device exits pairing mode and returns to normal operation
- Timeout resets on any BLE activity (connection, command, etc.)

### LED Indication
- **Entering:** 5 rapid blinks (100ms on/off)
- **Active:** Slow blink (500ms on/off)
- **Connected:** Solid on
- **Exiting:** LED off

### BLE Advertising
- Advertising starts when pairing mode is entered
- Advertising stops on timeout or manual exit
- Device name includes device type for easy identification

## Device-Specific Details

| Device | Pairing Trigger | LED Pin | Notes |
|--------|----------------|---------|-------|
| Valve Controller | P0.06 (2s hold) | P0.17 | 24V powered, single button |
| Water Meter | UP+DOWN (2s hold) | P0.06 | Mains powered, combo avoids conflict with menu |
| Soil Moisture | P0.07 (2s hold) | P0.17 | Battery powered, also enters on first boot |
| Valve Actuator | P0.30 (2s hold) | P0.27 | 24V powered via CAN bus |

### Water Meter Special Case

The water meter has 5 navigation buttons (UP, DOWN, LEFT, RIGHT, SELECT). Since SELECT long-press enters the configuration menu, BLE pairing uses a **two-button combo**:

- **SELECT (2s hold)** → Enter configuration/menu mode
- **UP + DOWN (2s hold together)** → Enter BLE pairing mode

This prevents accidental pairing mode entry during normal menu navigation.

## PIN Authentication

All devices use a 6-digit PIN for BLE authentication:
- **Default PIN:** `123456`
- **Storage:** FRAM at device-specific address
- **Max Attempts:** 3 before 5-minute lockout
- **Lockout Duration:** 5 minutes (`AGSYS_PIN_LOCKOUT_MS = 300000`)

## Available Features During Pairing

Features available depend on device type (configured in `ble_features.h`):

| Feature | Valve Ctrl | Water Meter | Soil Moist | Valve Act |
|---------|-----------|-------------|------------|-----------|
| DFU | ✓ | ✓ | ✓ | ✓ |
| Device Info | ✓ | ✓ | ✓ | ✓ |
| Settings | ✓ | ✓ | ✗ | ✗ |
| Live Data | ✓ | ✓ | ✗ | ✗ |
| Valve Control | ✓ | ✗ | ✗ | ✗ |
| CAN Discovery | ✓ | ✗ | ✗ | ✗ |
| Calibration | ✗ | ✓ | ✓ | ✗ |
| Diagnostics | ✓ | ✓ | ✓ | ✓ |

## Implementation

### Required Defines (in device config.h)
```c
#define PIN_PAIRING_BUTTON      (X)         // Device-specific GPIO
#define PAIRING_BUTTON_HOLD_MS  2000        // 2 second hold
#define BLE_PAIRING_TIMEOUT_MS  300000      // 5 minutes
```

### Required Code Pattern
```c
// In setup() or after button detection
if (buttonHeldFor(PAIRING_BUTTON_HOLD_MS)) {
    agsys_ble_start_advertising();
    pairingModeActive = true;
    pairingModeStartTime = millis();
    // Blink LED 5 times
}

// In loop()
agsys_ble_process();
if (pairingModeActive) {
    // Slow blink LED
    if (millis() - pairingModeStartTime > BLE_PAIRING_TIMEOUT_MS) {
        agsys_ble_stop_advertising();
        pairingModeActive = false;
    }
}
```

## Security Considerations

1. **PIN Required:** All configuration changes require PIN authentication
2. **Timeout:** Pairing mode automatically exits after 5 minutes
3. **Lockout:** 3 failed PIN attempts triggers 5-minute lockout
4. **No Remote Trigger:** Pairing mode can only be entered via physical button
5. **FRAM Storage:** PIN is stored in non-volatile FRAM, survives power cycles

## Revision History

| Date | Change |
|------|--------|
| 2026-01 | Initial standardization across all devices |
| 2026-01 | Unified to 2-second hold time |
| 2026-01 | Added valve actuator support |
