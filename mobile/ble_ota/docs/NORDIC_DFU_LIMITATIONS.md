# Nordic DFU Limitations and Considerations

This document outlines the limitations, edge cases, and considerations for the Nordic DFU (Device Firmware Update) protocol used in AgSys devices.

## Overview

The nRF52832 uses Nordic's Secure DFU Bootloader for over-the-air firmware updates. Understanding its limitations is critical for reliable field updates.

## Key Limitations

### 1. Bootloader Cannot Be Updated via DFU

**Limitation**: The bootloader itself cannot be updated through the standard DFU process.

**Impact**:
- Bootloader bugs are permanent unless physical JTAG/SWD access is available
- New bootloader features cannot be added post-deployment
- Security vulnerabilities in the bootloader cannot be patched remotely

**Mitigation**:
- Thoroughly test bootloader before deployment
- Use the latest stable Nordic SDK bootloader
- Keep bootloader minimal to reduce bug surface

### 2. SoftDevice Version Compatibility

**Limitation**: The SoftDevice (Nordic's BLE stack) version must be compatible with both the bootloader and application.

**Compatibility Matrix**:
| SoftDevice | SDK Version | Bootloader | Notes |
|------------|-------------|------------|-------|
| S132 v6.1.1 | SDK 15.3 | v1.0 | Current production |
| S132 v7.0.1 | SDK 17.0 | v2.0 | Requires bootloader update |
| S132 v7.2.0 | SDK 17.1 | v2.0 | Latest |

**Impact**:
- Cannot upgrade SoftDevice without matching bootloader
- Application must be compiled against correct SoftDevice version
- Downgrading SoftDevice may require bootloader downgrade

**Mitigation**:
- Lock SoftDevice version for production fleet
- Test all firmware against deployed SoftDevice version
- Document SoftDevice version in firmware metadata

### 3. Flash Memory Constraints

**Limitation**: nRF52832 has 512KB flash, shared between bootloader, SoftDevice, and application.

**Memory Layout**:
```
0x00000000 - 0x00001000  MBR (Master Boot Record) - 4KB
0x00001000 - 0x00026000  SoftDevice S132 - 148KB
0x00026000 - 0x00074000  Application - 312KB max
0x00074000 - 0x0007E000  Bootloader - 40KB
0x0007E000 - 0x00080000  Bootloader Settings - 8KB
```

**Impact**:
- Application size limited to ~312KB
- Cannot store two application images simultaneously (no A/B partitioning)
- DFU requires temporary storage in RAM or external flash

**Mitigation**:
- Use external FRAM for DFU staging (current design)
- Optimize application size
- Consider nRF52840 (1MB flash) for future products

### 4. No Automatic Rollback

**Limitation**: Nordic DFU does not have built-in automatic rollback on boot failure.

**Impact**:
- Bricked device if new firmware fails to boot
- No "watchdog" to detect failed updates
- Manual intervention required for recovery

**Mitigation**:
- Implement application-level health check
- Store previous version info in FRAM
- BLE OTA as manual recovery option
- Consider custom bootloader with rollback support

### 5. Power Failure During Update

**Limitation**: Power loss during DFU can leave device in inconsistent state.

**Scenarios**:
| Phase | Power Loss Impact | Recovery |
|-------|-------------------|----------|
| Downloading chunks | Safe - can resume | Automatic |
| Erasing flash | May corrupt app | Enter DFU mode |
| Writing flash | May corrupt app | Enter DFU mode |
| Validating | Safe | Retry validation |
| Activating | May brick | Manual recovery |

**Mitigation**:
- Check battery level before starting DFU (require >20%)
- Use FRAM to track DFU progress
- Implement chunk-based resume
- Keep device powered during critical phases

### 6. Signature Verification

**Limitation**: Secure DFU requires signed firmware packages.

**Requirements**:
- Private key must be kept secure
- Public key burned into bootloader
- Cannot change keys without bootloader update

**Impact**:
- Lost private key = cannot update devices
- Compromised key = security breach
- Key rotation requires physical access

**Mitigation**:
- Store private key in HSM or secure vault
- Use separate keys for development/production
- Document key management procedures

## Upgrade/Downgrade Considerations

### Upgrade Path Restrictions

Some firmware versions may have **breaking changes** that prevent direct upgrades:

```
v1.0.0 → v1.1.0  ✓ Direct upgrade OK
v1.1.0 → v2.0.0  ✗ Requires intermediate v1.5.0
v2.0.0 → v2.1.0  ✓ Direct upgrade OK
```

**Reasons for restrictions**:
- FRAM data format changes
- Configuration schema changes
- Protocol version changes
- Calibration data format changes

**Implementation**:
```dart
// In FirmwareRelease model
incompatibleVersions: [
  FirmwareVersion(1, 0, 0),  // Cannot upgrade directly from 1.0.0
  FirmwareVersion(1, 1, 0),  // Cannot upgrade directly from 1.1.0
]
```

### Downgrade Restrictions

Downgrades may be restricted for:

1. **Security patches**: Prevent rollback to vulnerable versions
2. **Data format changes**: New version may have migrated data
3. **Hardware revisions**: Newer firmware may not work on older hardware

**Implementation**:
```json
{
  "version": "2.0.0",
  "min_allowed_downgrade": "1.5.0",
  "downgrade_warning": "Downgrading will reset calibration data"
}
```

### Data Migration

When upgrading between major versions:

1. **Before update**: Backup critical data to FRAM
2. **First boot**: Detect version change, run migration
3. **Verify**: Check data integrity
4. **Commit**: Mark migration complete

```cpp
// In firmware
void checkMigration() {
    uint32_t storedVersion = nvram.readFirmwareVersion();
    uint32_t currentVersion = FIRMWARE_VERSION;
    
    if (storedVersion < currentVersion) {
        migrateData(storedVersion, currentVersion);
        nvram.writeFirmwareVersion(currentVersion);
    }
}
```

## Recommended Firmware Version Strategy

### Version Numbering

```
MAJOR.MINOR.PATCH

MAJOR: Breaking changes, data migrations
MINOR: New features, backward compatible
PATCH: Bug fixes only
```

### Release Types

| Type | Stability | Auto-update | Rollback |
|------|-----------|-------------|----------|
| Stable | Production ready | Yes (LoRa) | Allowed |
| Beta | Testing | No | Allowed |
| Critical | Security fix | Forced | Blocked |

### Minimum Cached Versions

The BLE OTA app should cache:

1. **Latest stable**: For upgrades
2. **Previous stable**: For rollback
3. **Current version**: For reinstall/repair

### Version Compatibility Matrix

Maintain a server-side compatibility matrix:

```json
{
  "versions": [
    {
      "version": "2.1.0",
      "min_upgrade_from": "1.5.0",
      "max_downgrade_to": "2.0.0",
      "requires_bootloader": "1.0.0",
      "requires_softdevice": "6.1.1",
      "incompatible_from": ["1.0.0", "1.1.0"]
    }
  ]
}
```

## Recovery Procedures

### Scenario 1: Failed Update (Device Still Boots)

1. Device boots with old firmware
2. Retry OTA update
3. If persistent, try different firmware version

### Scenario 2: Failed Update (Device in DFU Mode)

1. Device automatically enters DFU bootloader
2. Use BLE OTA app to upload working firmware
3. Any version compatible with bootloader will work

### Scenario 3: Completely Bricked

1. Connect JTAG/SWD programmer
2. Erase chip
3. Flash bootloader + SoftDevice + application
4. Reconfigure device (UUID, calibration)

## Best Practices

### For Firmware Development

1. **Always test upgrade paths** from previous 2-3 versions
2. **Test downgrade paths** to previous stable
3. **Implement health check** that runs on first boot
4. **Log version changes** to FRAM for debugging
5. **Keep bootloader minimal** and well-tested

### For Fleet Management

1. **Staged rollouts**: Update 10% → 50% → 100%
2. **Monitor success rate**: Abort if >5% failures
3. **Maintain rollback version**: Always keep previous stable available
4. **Document known issues**: Track version-specific problems

### For the BLE OTA App

1. **Cache multiple versions**: Latest + previous + current
2. **Check compatibility**: Before allowing install
3. **Warn on downgrade**: User must acknowledge risks
4. **Verify after download**: Check SHA256 before install
5. **Battery check**: Require >20% before starting

## Appendix: DFU Package Structure

Nordic DFU packages are ZIP files containing:

```
firmware_v1.2.0.zip
├── manifest.json       # Package metadata
├── nrf52832_xxaa.bin   # Application binary
├── nrf52832_xxaa.dat   # Init packet (signature)
└── sd_bl.bin           # Optional: SoftDevice + Bootloader
```

### Creating DFU Packages

```bash
# Application only
nrfutil pkg generate \
  --application firmware.hex \
  --application-version 0x010200 \
  --hw-version 52 \
  --sd-req 0x00B7 \
  --key-file private.pem \
  firmware_v1.2.0.zip

# With SoftDevice (requires matching bootloader)
nrfutil pkg generate \
  --application firmware.hex \
  --softdevice s132_nrf52_6.1.1_softdevice.hex \
  --application-version 0x010200 \
  --hw-version 52 \
  --sd-req 0x00B7 \
  --key-file private.pem \
  firmware_v1.2.0_full.zip
```
