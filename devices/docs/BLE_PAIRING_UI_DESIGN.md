# BLE Pairing UI Design

## Overview

This document describes the UI design for BLE pairing across AgSys devices. The design accommodates devices with displays (Water Meter, Valve Controller) and those without (Soil Moisture Sensor).

## BLE Connection States

| State | Description | LED Behavior | Display Icon |
|-------|-------------|--------------|--------------|
| **IDLE** | BLE off, not advertising | LED off | Hidden |
| **ADVERTISING** | Pairing mode, discoverable | Slow blink (1Hz) | Slow blink (1Hz) |
| **CONNECTED** | Device connected, not authenticated | Fast blink (2Hz) | Fast blink (2Hz) |
| **AUTHENTICATED** | Connected and PIN verified | Solid on | Solid on |
| **DISCONNECTED** | Was connected, now lost | 3 quick blinks, then off | Triple flash, then hide |

## Entering Pairing Mode

### Devices with Display (Water Meter, Valve Controller)

**Method 1: Button Combo at Boot**
- Hold SELECT + UP during power-on
- Device enters pairing mode for 2 minutes
- Display shows "Pairing Mode" screen

**Method 2: Menu Option**
- Navigate to: Menu → LoRa Config → BLE Pairing
- Select "Enable Pairing"
- Pairing mode active for 2 minutes

**Method 3: Long Press (Main Screen)**
- Long press SELECT (3 seconds) from main screen
- Quick access to pairing mode

### Devices without Display (Soil Moisture Sensor)

**Method: Button Hold at Boot**
- Hold the single button during power-on for 5 seconds
- LED indicates pairing mode (slow blink)
- Pairing mode active for 2 minutes

## Display UI Elements

### BLE Icon (Water Meter)

A small 24x24 pixel Bluetooth icon in the **lower-right corner** of the main screen, partially overlaying the totalizer section. The icon flashes to match LED behavior on other devices.

```
┌─────────────────────────────────────┐
│                                     │
│         Flow Rate Display           │
│                                     │
├─────────────────────────────────────┤
│   Trend        │      Avg          │
├─────────────────────────────────────┤
│                                     │
│      Total Volume: 1234.5 L    [B] │  ← BLE icon in corner
│                                     │
└─────────────────────────────────────┘
```

**Icon Behavior:**
- **Hidden**: BLE idle (not advertising)
- **Slow blink (500ms)**: Advertising/pairing mode
- **Fast blink (250ms)**: Connected, awaiting authentication
- **Solid on**: Connected and authenticated
- **Triple flash (100ms)**: Disconnected (then hides)

**Icon Appearance:**
- 24x24 pixel rounded square
- Bluetooth blue background (#0082FC)
- White Bluetooth symbol (LVGL LV_SYMBOL_BLUETOOTH)
- 4px corner radius
- Positioned 4px from bottom-right corner

## LED Patterns (All Devices)

For devices with a BLE status LED:

| Pattern | Meaning |
|---------|---------|
| Off | BLE idle |
| Slow blink (500ms on, 500ms off) | Advertising/pairing mode |
| Fast blink (250ms on, 250ms off) | Connected, awaiting auth |
| Solid on | Authenticated |
| Triple flash (100ms × 3) | Disconnected |
| Double flash every 2s | Auth failed (lockout) |

## Security Considerations

### Pairing Mode Timeout
- Default: 2 minutes
- Maximum: 10 minutes (with extensions)
- After timeout, BLE stops advertising

### PIN Lockout
- After 5 failed PIN attempts: 5-minute lockout
- After 10 failed attempts: 30-minute lockout
- Lockout resets on successful auth or device reboot

### Auto-Disconnect
- If authenticated but idle for 10 minutes, disconnect
- Configurable via device settings

## Implementation Notes

### BLE UI State Enum (ui_types.h)
```c
typedef enum {
    BLE_UI_STATE_IDLE = 0,       /* BLE off, not advertising */
    BLE_UI_STATE_ADVERTISING,    /* Pairing mode, discoverable */
    BLE_UI_STATE_CONNECTED,      /* Connected, not authenticated */
    BLE_UI_STATE_AUTHENTICATED,  /* Connected and PIN verified */
    BLE_UI_STATE_DISCONNECTED    /* Was connected, now lost */
} BleUiState_t;
```

### Display API (display.h)
```c
void display_updateBleStatus(BleUiState_t state);
BleUiState_t display_getBleStatus(void);
void display_tickBleIcon(void);  /* Call from display task for flash animation */
```

### Usage in main.c
```c
/* On BLE event callback */
void ble_event_handler(const agsys_ble_evt_t *evt) {
    switch (evt->type) {
        case AGSYS_BLE_EVT_CONNECTED:
            display_updateBleStatus(BLE_UI_STATE_CONNECTED);
            break;
        case AGSYS_BLE_EVT_AUTHENTICATED:
            display_updateBleStatus(BLE_UI_STATE_AUTHENTICATED);
            break;
        case AGSYS_BLE_EVT_DISCONNECTED:
            display_updateBleStatus(BLE_UI_STATE_DISCONNECTED);
            /* After brief flash, return to idle */
            break;
        // ...
    }
}

/* When entering pairing mode */
display_updateBleStatus(BLE_UI_STATE_ADVERTISING);

/* In display task loop */
display_tickBleIcon();  /* Updates flash animation */
```

## Flutter App Integration

The Flutter app should:
1. Scan for devices advertising the AgSys service UUID
2. Display device name and signal strength
3. Connect and prompt for PIN
4. Show connection status with retry option
5. Handle disconnection gracefully

## Device-Specific Notes

### Water Meter (watermeter-freertos)
- Full display UI as described above
- BLE LED on P1.07 (LED_BLE_PIN)
- 5-button navigation

### Valve Controller (valvecontrol-freertos)
- Similar display UI (if display present)
- May have simpler UI if no display
- BLE LED for status

### Soil Moisture Sensor (soilmoisture-freertos)
- LED-only indication
- Single button for pairing mode entry
- No display screens
