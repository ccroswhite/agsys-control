# AgSys BLE OTA App

Standalone Flutter application for Bluetooth Low Energy (BLE) firmware updates of AgSys IoT devices.

## Purpose

This app provides a **fallback mechanism** for firmware updates when LoRa OTA is not available or has failed. It uses the Nordic DFU (Device Firmware Update) protocol over BLE.

## Features

- **BLE Device Scanning**: Automatically discovers AgSys devices in DFU mode
- **Nordic DFU Protocol**: Industry-standard secure firmware update
- **Progress Tracking**: Real-time upload progress with speed indicators
- **Resume Support**: Can retry failed updates
- **Deep Link Integration**: Can be launched from the main AgSys app
- **Version Management**: Download and cache multiple firmware versions
- **Upgrade/Downgrade**: Support for both upgrades and rollbacks
- **Offline Support**: Pre-download firmware for field use without connectivity
- **Compatibility Checking**: Validates version compatibility before install

## When to Use

| Scenario | Use LoRa OTA | Use BLE OTA |
|----------|--------------|-------------|
| Routine fleet updates | ✓ | |
| Single device recovery | | ✓ |
| Device not responding to LoRa | | ✓ |
| Initial device provisioning | | ✓ |
| Field troubleshooting | | ✓ |

## Installation

### Prerequisites

- Flutter SDK 3.10+
- Android Studio or Xcode
- Physical device (BLE doesn't work in simulators)

### Build

```bash
cd mobile/ble_ota

# Get dependencies
flutter pub get

# Build for Android
flutter build apk --release

# Build for iOS
flutter build ios --release
```

## Usage

### Standalone

1. **Put device in OTA mode**: Press the OTA button on the AgSys device
2. **Open the app**: Launch AgSys BLE OTA
3. **Scan for devices**: Tap "Scan for Devices"
4. **Select device**: Tap on the device in the list
5. **Select firmware**: Choose the firmware .zip file
6. **Start update**: Tap "Start Firmware Update"
7. **Wait for completion**: ~30 seconds for typical firmware

### From Main App (Deep Link)

The main AgSys Flutter app can launch this app with pre-configured parameters:

```dart
// In main app
import 'package:url_launcher/url_launcher.dart';

Future<void> launchBleOta({String? firmwarePath, String? deviceId}) async {
  final uri = Uri(
    scheme: 'agsys-ota',
    host: 'update',
    queryParameters: {
      if (firmwarePath != null) 'firmware': firmwarePath,
      if (deviceId != null) 'device': deviceId,
    },
  );
  
  await launchUrl(uri);
}
```

## Project Structure

```
ble_ota/
├── lib/
│   ├── main.dart                    - App entry point
│   ├── models/
│   │   └── firmware_version.dart    - Version and release models
│   ├── providers/
│   │   ├── ble_provider.dart        - BLE scanning and connection
│   │   ├── dfu_provider.dart        - Nordic DFU update logic
│   │   └── firmware_provider.dart   - Firmware version management
│   ├── screens/
│   │   ├── home_screen.dart         - Main screen
│   │   ├── scan_screen.dart         - Device scanning
│   │   ├── firmware_screen.dart     - Version selection
│   │   └── update_screen.dart       - Update progress
│   └── services/
│       ├── ota_service.dart         - API for main app integration
│       └── firmware_repository.dart - Download and cache firmware
├── docs/
│   └── NORDIC_DFU_LIMITATIONS.md    - DFU limitations and considerations
├── android/
│   └── app/src/main/AndroidManifest.xml
├── ios/
│   └── Runner/Info.plist
├── pubspec.yaml
└── README.md
```

## Dependencies

| Package | Purpose |
|---------|---------|
| `flutter_blue_plus` | BLE scanning and connection |
| `nordic_dfu` | Nordic DFU protocol implementation |
| `file_picker` | Firmware file selection |
| `provider` | State management |
| `permission_handler` | Runtime permissions |
| `http` | Firmware downloads |
| `crypto` | SHA256 checksum verification |

## Firmware Version Management

The app manages multiple firmware versions for flexibility:

### Cached Versions

By default, the app caches up to **5 firmware versions** per device type:

1. **Latest stable** - For upgrades
2. **Previous stable** - For rollback if latest has issues
3. **Current version** - For reinstall/repair
4. **Critical patches** - Security updates

### Version Selection

When selecting firmware, the app shows:

- **Recommended versions** - Based on current device version
- **All available versions** - Full list from server
- **Cached versions** - Available offline

### Compatibility Checking

Before allowing installation, the app verifies:

- Bootloader version compatibility
- SoftDevice version compatibility
- Known incompatible upgrade paths
- Downgrade restrictions (security)

### Offline Support

For field use without internet:

```bash
# Pre-download firmware before going to field
1. Open app with internet connection
2. Go to Firmware screen
3. Tap "Download" on recommended versions
4. Versions are cached locally
5. Use in field without connectivity
```

## Permissions

### Android

- `BLUETOOTH_SCAN` - Scan for BLE devices
- `BLUETOOTH_CONNECT` - Connect to devices
- `ACCESS_FINE_LOCATION` - Required for BLE on older Android
- `FOREGROUND_SERVICE` - Keep DFU running in background

### iOS

- `NSBluetoothAlwaysUsageDescription` - Bluetooth access
- `NSLocationWhenInUseUsageDescription` - Location for BLE
- `bluetooth-central` background mode - Background DFU

## Firmware File Format

The app accepts Nordic DFU packages in `.zip` format. These are created using:

```bash
# From the device firmware directory
adafruit-nrfutil dfu genpkg \
  --dev-type 0x0052 \
  --application .pio/build/adafruit_feather_nrf52832/firmware.hex \
  firmware_v1.2.0.zip
```

## Troubleshooting

### Device not found

1. Ensure device is in OTA mode (press OTA button, blue LED should be on)
2. Check Bluetooth is enabled on phone
3. Grant all requested permissions
4. Try moving closer to the device

### Update fails

1. Ensure firmware file is valid (.zip format)
2. Check device battery is sufficient (>20%)
3. Stay within BLE range (~10m) during update
4. Try resetting the device and starting again

### "DFU aborted" error

This usually means the device disconnected during update. Possible causes:
- Device battery died
- Moved out of BLE range
- Firmware file corrupted

## Integration with Main App

The main AgSys Flutter app should:

1. **Check for updates**: Query the cloud backend for available firmware
2. **Download firmware**: Store .zip file locally
3. **Launch BLE OTA**: Use deep link when user needs manual update

Example integration:

```dart
// In main app's device detail screen
ElevatedButton(
  onPressed: () async {
    // Download firmware if needed
    final firmwarePath = await downloadFirmware(latestVersion);
    
    // Launch BLE OTA app
    final uri = Uri.parse('agsys-ota://update?firmware=$firmwarePath');
    await launchUrl(uri, mode: LaunchMode.externalApplication);
  },
  child: Text('Update via Bluetooth'),
)
```

## Security

- Firmware packages are signed by Nordic DFU
- Device validates signature before applying
- BLE connection is encrypted
- No sensitive data transmitted

## Version History

| Version | Changes |
|---------|---------|
| 1.0.0 | Initial release |
