# OTA Automation Backend and UX Requirements

> **Status:** Draft - Requirements gathering
> 
> This document defines the detailed requirements for the OTA update automation backend 
> and the AgSys Flutter application UX. These requirements should be finalized before 
> implementation begins.

## Overview

The OTA system requires significant backend automation to manage firmware updates across 
potentially hundreds of field devices. The UX must provide clear visibility into update 
status while allowing operators to control the rollout process.

---

## Two OTA Paths: LoRa vs BLE

The system supports **two distinct OTA update paths** with different use cases:

### LoRa OTA (Property Controller → Device)

**Path:** Cloud → Property Controller → LoRa → Field Device

**Characteristics:**
- Managed centrally from the AgSys web/mobile app
- Property controller orchestrates updates
- Can update many devices without physical access
- Slower (LoRa bandwidth limited)
- Works for devices anywhere on the property
- Supports staged rollouts and campaigns

**Best for:**
- Fleet-wide updates
- Remote properties
- Scheduled maintenance windows
- Staged rollouts with monitoring

### BLE OTA (Phone → Device)

**Path:** Phone App → BLE → Device (direct)

**Characteristics:**
- Requires physical proximity to device (~10m range)
- Direct phone-to-device, bypasses property controller
- Faster than LoRa (BLE has higher bandwidth)
- One device at a time
- User must be on-site
- No campaign management - immediate update

**Best for:**
- Initial device provisioning/setup
- Troubleshooting a specific device
- Emergency field fix when LoRa isn't working
- Devices not yet registered with property controller
- Valve actuators (BLE only, no LoRa)

### UX Implications

The AgSys mobile app needs to support **both paths** with clear UX:

1. **Fleet Management View** (LoRa path)
   - Manage updates across all devices on a property
   - Campaign-based updates with staging
   - Progress monitoring from anywhere

2. **Device Detail View** (BLE path)
   - When connected to a device via BLE
   - Option to update this specific device directly
   - Shows current version vs available version
   - "Update Now" button for immediate BLE OTA

3. **Version Mismatch Handling**
   - If a device was updated via BLE, property controller needs to learn the new version
   - Device sends VERSION_REPORT on next LoRa communication
   - Or user can trigger "Sync Version" from app while BLE connected

### Valve Actuators - BLE Only

Valve actuators communicate via CAN bus to the valve controller, not LoRa. They can only be updated via BLE when a technician is physically present. The UX should:

- Show valve actuators in device list with "BLE Only" indicator
- Require BLE connection to initiate update
- Not include them in LoRa-based campaigns

---

## Backend Automation Requirements

### TODO: Define the following

1. **Firmware Storage and Management**
   - Where are firmware binaries stored? (Local filesystem, S3, etc.)
   - How are firmware versions uploaded and validated?
   - Retention policy for old versions
   - Firmware metadata (release notes, compatibility, etc.)

2. **Update Campaign Lifecycle**
   - How are campaigns created, started, paused, resumed, cancelled?
   - What triggers automatic pause (failure threshold)?
   - How are staged rollouts managed?
   - What happens if property controller goes offline mid-update?

3. **Device Tracking**
   - How do we know which devices need updates?
   - How do we track device wake windows (for battery devices)?
   - How do we handle devices that are offline/unreachable?
   - What is the retry policy for failed updates?

4. **Confirmation and Stability**
   - How long do we wait for VERSION_REPORT after OTA_FINISH?
   - What defines "stable" (N reports without reboot)?
   - How do we handle devices that report rollback?
   - What alerts/notifications are generated?

5. **Multi-Property Coordination**
   - Can updates be pushed to multiple properties simultaneously?
   - How does the cloud coordinate with property controllers?
   - What happens if different properties have different firmware versions?

6. **Scheduling**
   - How are scheduled updates stored and triggered?
   - Time zone handling for "off-peak" scheduling
   - Integration with growing season calendar?

---

## UX Requirements

### TODO: Define the following

### A. Fleet Management (LoRa Path)

1. **Firmware Management Screen**
   - List of device types and current/available versions
   - Upload new firmware workflow
   - Release notes display
   - Version comparison

2. **Update Configuration (Campaign)**
   - Mode selection (trickle, normal, aggressive)
   - Rollout strategy selection
   - Device selection (all, staged, specific devices)
   - Scheduling options
   - Confirmation dialogs for aggressive mode

3. **Progress Monitoring**
   - Real-time progress updates (polling vs WebSocket)
   - Per-device status view
   - Stage progress for staged rollouts
   - ETA calculation
   - Activity log/timeline

4. **Failure Handling**
   - Alert display for failures
   - Actionable options (continue, pause, skip, cancel)
   - Failure details and logs
   - Rollback notification

5. **History and Reporting**
   - Update history per device
   - Campaign history with success rates
   - Export/reporting capabilities

6. **Notifications**
   - In-app notifications for update events
   - Push notifications for failures/completions
   - Email notifications (optional)

### B. Direct Device Update (BLE Path)

1. **Device Connection Screen**
   - Show current firmware version when BLE connected
   - Show available firmware version (from cloud)
   - "Update Available" badge if newer version exists
   - Clear indication of BLE connection status

2. **BLE Update Flow**
   - "Update via BLE" button on device detail screen
   - Confirmation dialog with version info and release notes
   - Progress screen with:
     - Progress bar (BLE is faster, ~1-2 minutes)
     - "Stay within range" warning
     - Cancel option (with confirmation)
   - Success/failure result screen
   - Option to verify version after reboot

3. **Firmware Download**
   - App needs to download firmware binary from cloud before BLE transfer
   - Show download progress if not cached
   - Cache management for firmware binaries on phone

4. **Offline Considerations**
   - Can BLE OTA work if phone has no internet?
   - Pre-download firmware for field work?
   - Queue updates for when back online?

5. **Version Sync**
   - After BLE update, device version in cloud database may be stale
   - Options:
     - App pushes new version to cloud immediately
     - Wait for device's next LoRa report
     - Manual "Sync" button

### C. Valve Actuator Updates (BLE Only)

1. **Discovery and Connection**
   - Scan for nearby valve actuators
   - Show list with signal strength
   - Connect to specific actuator

2. **Update Flow**
   - Same as BLE path above
   - No LoRa alternative available
   - Must be physically present

3. **Bulk Actuator Updates**
   - If updating multiple actuators in a valve box
   - Queue updates, connect to each in sequence
   - Progress across all actuators

### D. Unified Device List

1. **Version Status Indicators**
   - ✓ Up to date
   - ↑ Update available
   - ⚠ Update failed/rolled back
   - ? Version unknown (offline)

2. **Update Method Indicators**
   - 📡 LoRa capable
   - 📶 BLE only
   - Both icons if device supports both

3. **Filtering and Sorting**
   - Filter by: device type, version, update status
   - Sort by: name, version, last seen, update status

---

## API Contract

### TODO: Define detailed API specifications

- Request/response schemas
- Error codes and handling
- WebSocket events for real-time updates
- Authentication/authorization requirements

---

## State Machine

### TODO: Define state machines for

1. **Update Campaign States**
   - scheduled → in_progress → paused → completed/cancelled

2. **Device Update States**
   - pending → in_progress → confirming → success/failed/skipped

3. **Stage States (for staged rollout)**
   - pending → in_progress → complete/failed

---

## Resolved Questions

### BLE Authentication
**Q:** Should BLE OTA require authentication/pairing first?

**A:** Yes. BLE already requires PIN authentication to pair. Once paired, the user can configure and upgrade the device. No additional OTA-specific authentication needed.

### BLE Update Interruption
**Q:** How do we handle BLE update interrupted by phone going out of range?

**A:** The update should fail gracefully and the device reverts to running normally on the current firmware. The OTA module already supports this - if chunks stop arriving, the device times out and returns to normal operation. The staged firmware in external flash is simply abandoned.

### Preventing Wrong Device Updates
**Q:** How do we prevent accidental updates to wrong device (multiple devices in BLE range)?

**A:** Multiple safeguards:

1. **Firmware Header Validation** - Each firmware binary includes a header with:
   ```c
   typedef struct {
       uint32_t magic;           // 0xAG5Y5F57 ("AGSYSFW")
       uint8_t  device_type;     // SOIL_MOISTURE, VALVE_CONTROLLER, WATER_METER, etc.
       uint8_t  hw_revision;     // Hardware revision compatibility
       uint8_t  version_major;
       uint8_t  version_minor;
       uint8_t  version_patch;
       uint8_t  reserved[3];
       uint32_t firmware_size;   // Size of firmware (excluding header)
       uint32_t firmware_crc;    // CRC32 of firmware
       char     build_id[16];    // Build identifier string
   } agsys_fw_header_t;
   ```

2. **Device Type Check** - Device validates `device_type` matches before accepting OTA. A soil moisture sensor will reject valve controller firmware.

3. **Hardware Revision Check** - Device validates `hw_revision` is compatible. Prevents flashing firmware for newer hardware onto older boards.

4. **UI Confirmation** - App shows device name/UID and firmware target before starting. User must confirm.

5. **BLE Device Name** - Include device type in BLE advertising name (e.g., "AgSoil-A1B2C3") so user can visually confirm correct device.

### Version Reporting
**Q:** How quickly must cloud reflect BLE-updated version?

**A:** Include version in regular device communications:

1. **LoRa Messages** - Add abbreviated version to message headers:
   ```c
   // Every LoRa message includes:
   struct {
       uint8_t msg_type;
       uint8_t device_uid[8];
       uint8_t fw_version[3];  // major, minor, patch - 3 bytes overhead
       // ... rest of payload
   } lora_header_t;
   ```
   This way, every sensor report, status update, etc. automatically syncs version to property controller → cloud.

2. **BLE Immediate Sync** - After successful BLE OTA, app immediately pushes new version to cloud via API:
   ```
   POST /api/devices/{device_uid}/version
   { "version": "1.3.0", "updated_via": "ble", "updated_by": "user@example.com" }
   ```

3. **VERSION_REPORT Message** - Device sends dedicated version report after OTA reboot (already planned).

**Recommendation:** Option 1 (version in every message) is most robust - no extra messages needed, version stays in sync automatically.

### Version Conflicts in Campaigns
**Q:** How do we handle version conflicts (BLE updated to v1.3, but LoRa campaign expects v1.2)?

**A:** Design principles:

1. **Backwards Compatibility** - Firmware versions should be backwards compatible. Protocol changes must support older devices. Breaking changes are exceptional and require coordinated rollout.

2. **Campaign Skips Updated Devices** - When a LoRa campaign runs, it checks each device's current version:
   - If device is already at or above target version → skip (already updated)
   - If device is below target version → include in campaign
   
   This handles the case where some devices were updated via BLE before the campaign ran.

3. **Version Ordering** - Use semantic versioning. Campaign logic:
   ```python
   if device.version >= campaign.target_version:
       skip_device("Already at or above target version")
   elif device.version < campaign.minimum_version:
       skip_device("Requires intermediate update first")
   else:
       include_in_campaign()
   ```

4. **Breaking Version Sequences** - For rare breaking changes, define upgrade paths:
   ```
   v1.x → v2.0 requires: v1.5 first (migration firmware)
   v2.x → v3.0 requires: v2.3 first
   ```
   The system enforces these sequences and guides users through multi-step upgrades.

### Update Permissions (RBAC)
**Q:** Who can initiate updates? (Admin only? Property managers?)

**A:** Property admins and property managers can control device updates for their properties.

> **TODO:** Define organization roles and RBAC for multi-property management. An organization can have multiple properties, so roles and their interaction with properties needs formal definition.

> **TODO:** Document BLE PIN auth vs user auth gap. BLE upgrades use PIN authentication (device-level), not user authentication. This breaks the RBAC model we're implementing for LoRa upgrades. The field application requires user login which provides RBAC enforcement - users can only open the app if they have configure/upgrade permissions.

### Emergency Stop / Auto-Pause
**Q:** Should we support "emergency stop" that aborts all in-progress updates?

**A:** Yes. Two mechanisms:

1. **Manual Emergency Stop** - User can cancel all in-progress updates with confirmation dialog.

2. **Auto-Pause on High Failure Rate** - System automatically pauses campaign if failure rate exceeds threshold.
   - Default threshold: **20%**
   - User-configurable per campaign
   - Alerts user when auto-pause triggers

### Firmware Downgrades
**Q:** How do we handle firmware downgrades?

**A:** Allow with confirmation. Sometimes you need to rollback to a known-good version. Show warning dialog explaining risks, require explicit confirmation.

### A/B Testing
**Q:** Should we support A/B testing of firmware versions?

**A:** Deferred. Value proposition unclear for customer sites. A/B testing should be handled on development builds and with very select customers outside normal production workflows. Staged rollout already provides some of this capability.

### Metrics and Analytics
**Q:** What metrics/analytics do we want to capture?

**A:** Capture in backend for operational excellence:
- Update success/failure rates per version
- Average update duration
- Rollback frequency
- Time from release to full fleet deployment
- Device uptime after updates

> **TODO:** Implement user opt-in/out mechanism for sharing these metrics with the company to improve the product. Users should be asked to consent to sharing anonymized metrics.

### Firmware Cache on Phone
**Q:** How large can firmware binaries be cached on the phone?

**A:** Cache up to **5 versions per device type**:
- Latest version (always cached)
- 4 previous versions from the latest

Older versions are auto-purged. This ensures users always have recent versions available while preventing accumulation of obsolete firmware that can never be used again. Since we control the application, we can enforce this policy.

### Offline BLE Updates
**Q:** Should we support offline BLE updates (pre-downloaded firmware)?

**A:** Yes, absolutely. Allow user to "Download for offline use" before going to the field. This ties into the firmware caching policy above.

### Field Worker Permissions
**Q:** Should field workers be able to update devices they don't "own" in the system?

**A:** RBAC is enforced at application login. The field application for mobile:
1. Requires user login
2. Login determines RBAC permissions
3. Users can only access the app if they have configure/upgrade permissions for the relevant properties
4. BLE pairing uses PIN authentication (device-level security)
5. User authentication (app login) provides RBAC enforcement

This means a field worker can only update devices on properties they have been granted access to.

### Audit Logging
**Q:** Should BLE updates be logged to the cloud for audit trail?

**A:** Yes. Full audit trail required:
- Device UID
- Old version → New version
- Who performed update (user ID from app login)
- Timestamp
- Success/failure status
- Update method (BLE or LoRa)

> **TODO:** Implement OTA audit logging system. This requires:
> 1. LoRa protocol update to include audit log messages
> 2. Property controller logic to receive and forward audit logs
> 3. Backend API to store audit logs
> 4. AgSys UI to display audit history
> 5. For BLE updates: app pushes audit log directly to backend

---

## Remaining Open Questions

None - all questions resolved.

---

## Next Steps

1. Review and finalize requirements with stakeholders
2. Create detailed API specifications
3. Design database schema
4. Implement backend automation
5. Implement Flutter UX
