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

### 1. Firmware Storage and Management

#### Storage Architecture

There are two OTA paths:
- **LoRa OTA**: Property controller initiates and controls the update process
- **BLE OTA**: Mobile application (to be developed) updates the device directly

This requires a hybrid storage approach:

```
┌─────────────────────────────────────────────────────────────┐
│                     AgSys Cloud (S3)                        │
│  - Source of truth for all firmware releases                │
│  - Keeps all versions (only AgSys admins can delete)        │
│  - Stores metadata + binary                                 │
└─────────────────────────────────────────────────────────────┘
                              │
                    "Publish" triggers sync
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────┐
│   Property Controller   │     │   Property Controller   │
│  - Caches last 5        │     │  - Caches last 5        │
│    versions per device  │     │    versions per device  │
│  - Initiates LoRa OTA   │     │  - Initiates LoRa OTA   │
└─────────────────────────┘     └─────────────────────────┘

              ┌─────────────────────────┐
              │     Mobile App          │
              │  - Downloads on demand  │
              │    or auto-sync         │
              │  - Caches for offline   │
              │  - Initiates BLE OTA    │
              └─────────────────────────┘
```

#### Upload and Validation Workflow

1. **Engineer builds firmware** → Uploads via AgSys admin-only page
2. **AgSys validates** → Parses firmware header, verifies SHA256, stores in S3
3. **AgSys admin publishes** → Triggers sync to all property controllers
4. **Property controller receives** → Stores locally (keeps last 5 per device type)
5. **Property admin/manager** → Uses property UI to deploy to devices (LoRa OTA)
6. **Mobile app** → Downloads firmware, performs BLE OTA when on-site

**Validation on upload:**
- Parse firmware header (device_type, hw_revision, version)
- Verify file size matches header
- Compute and store SHA256 hash
- Reject if device_type doesn't match upload target

**Signing:** Deferred for now, but API designed to support it later.

#### Retention Policy

| Location | Policy |
|----------|--------|
| **AgSys Cloud (S3)** | Keep all versions. Only AgSys admins can delete old or bad releases. |
| **Property Controller** | Keep last 5 versions per device type. Auto-prune older versions. |
| **Mobile App** | Keep last 5 versions per device type (as defined in Resolved Questions). |

#### Firmware Database Schema

```sql
firmware:
  id: uuid
  device_type: enum (soil_moisture, valve_controller, valve_actuator, water_meter)
  hw_revision_min: int                    -- Minimum compatible hardware revision
  hw_revision_max: int (nullable)         -- Maximum compatible (null = no limit)
  version: semver string                  -- e.g., "1.3.0"
  size_bytes: int
  sha256: string
  storage_path: string                    -- S3 key
  release_notes: text                     -- Markdown
  release_type: enum (stable, beta, development)
  published_at: timestamp (nullable)      -- null = not yet published
  created_at: timestamp
  created_by: user_id                     -- AgSys admin who uploaded
  is_active: bool                         -- false = hidden, can't be deployed
```

#### TODO: Breaking Changes Strategy

> Define how to handle breaking changes that require intermediate firmware versions.
> This includes the `requires_version` field and upgrade path validation.
> See separate discussion needed.

#### TODO: Property Admin/Manager Deployment UX

> Define the UX for property admins/managers to deploy releases to their devices.
> This is separate from the AgSys admin upload/publish workflow.

---

### 2. Update Process Lifecycle

> **Terminology Note:** We use "update" (not "upgrade") throughout because the process can move devices
> to newer OR older firmware versions. "Update" means bringing the device to the prescribed firmware.
> In user-facing UI, we refer to this as the "update process" (not "campaign").

#### What is an Update Process?

An update process is a coordinated effort to update a set of devices to a target firmware version.
It is managed at the **property level** by the property controller.

- Targets **one device type** (e.g., all soil moisture sensors on a property)
- Has **one target firmware version**
- Can include **all devices** or a **subset** (staged rollout)

#### Workflow

| Action | Who | Where | Description |
|--------|-----|-------|-------------|
| **Create** | Property admin/manager | Property UI (web/mobile) | Select device type, target version, devices to include, rollout strategy |
| **Start** | Property admin/manager | Property UI | Process begins, property controller starts updating devices |
| **Pause** | Property admin/manager OR auto | Property UI or automatic | Stops updating new devices, in-progress updates complete |
| **Resume** | Property admin/manager | Property UI | Continues from where paused |
| **Cancel** | Property admin/manager | Property UI | Stops process, marks remaining devices as skipped |

#### Automatic Pause Triggers

The update process automatically pauses when:
- **Failure rate exceeds 20%** (configurable per update process)
- **3 consecutive failures** occur

The property controller does NOT require cloud connectivity to complete the update process.
It operates autonomously and syncs status when reconnected.

#### Staged Rollouts

Users can define stages for gradual rollout. Options:

| Strategy | Description |
|----------|-------------|
| **Percentage-based** | Stage 1: 10%, Stage 2: 25%, Stage 3: 100% |
| **Count-based** | Stage 1: 5 devices, Stage 2: 20 devices, Stage 3: all |
| **Time-based** | Stage 1: first 24 hours, Stage 2: next 48 hours, Stage 3: remainder |
| **Manual** | User explicitly advances to next stage after reviewing results |

**Default behavior:** After each stage completes, the process pauses. User reviews results and
clicks "Continue to next stage" or "Cancel". Optional: auto-advance after N hours if no failures.

#### Property Controller Offline Scenarios

| Scenario | Behavior |
|----------|----------|
| **Controller loses internet** | Continues update process locally, syncs status when reconnected |
| **Controller reboots** | Resumes process from last known state (persisted to local storage) |
| **Controller loses power** | Same as reboot - resume on power restore |
| **Device mid-OTA when controller reboots** | Device times out, stays on old firmware, controller retries on next wake |

**Key principle:** Property controller operates autonomously. Cloud is informed but not required.

---

### 3. Device Tracking

#### Device Registry Synchronization

The property controller maintains a local device registry that is **eventually consistent** with the
AgSys backend. The AgSys backend is the source of truth for the UI - users see data from AgSys,
not directly from property controllers.

Registry fields per device:
- Device ID
- Device type
- Current firmware version (from last LoRa report)
- Hardware revision
- Last seen timestamp
- Update status (if in an active update process)

When an update process is created, the controller compares each device's current version against
the target version and builds the update queue.

**Version mismatch handling:** If a device is running a version *newer* than the target, this is
flagged to the property admin/manager. Downgrading is possible but the UI must clearly call out
the potential risks before proceeding.

#### LoRa OTA Trigger Mechanism

Battery-powered devices (e.g., soil moisture sensors) sleep between reports. To minimize airtime,
we use a **flag in the ACK response** to signal a pending update:

```
ACK message format:
  [header][ack_type][flags]

flags byte:
  bit 0: OTA_PENDING (1 = update available, device should stay awake)
  bit 1-7: reserved for future use
```

**OTA Flow:**
```
Device → Controller: SOIL_REPORT (or other report)
Controller → Device: ACK (flags: OTA_PENDING=1)
Device → Controller: OTA_REQUEST
Controller → Device: OTA_ANNOUNCE (version, size, chunk_count)
Device → Controller: OTA_READY
Controller → Device: OTA_CHUNK[0]
... (chunked transfer)
Controller → Device: OTA_FINISH
Device: validates, reboots
Device → Controller: VERSION_REPORT (confirms new version)
```

Common case (no update pending): ACK with `flags=0`, device sleeps normally. No extra airtime.

#### TODO: Implement LoRa OTA

> Implement the OTA_PENDING flag in the shared LoRa protocol (`agsys_lora_protocol.h`)
> and update all devices to handle the LoRa OTA process safely.

#### Offline/Unreachable Devices

Devices are tracked by last seen timestamp:

| Duration | Status | UI Display | Action |
|----------|--------|------------|--------|
| **< 24 hours** | Pending | Normal | Keep in queue, wait for next wake |
| **24-72 hours** | Delayed | Warning icon | Keep in queue, flag in UI |
| **> 72 hours** | Unreachable | Error icon | Flag in UI, allow manual skip |

Property admin/manager can manually skip unreachable devices (e.g., if unit is dead and needs replacement).

#### Retry Policy

| Failure Type | Retry Behavior |
|--------------|----------------|
| **Timeout (no response)** | Retry on next device wake, max 3 attempts |
| **Transfer error (CRC fail)** | Retry immediately once, then wait for next wake |
| **Device rejected (incompatible)** | No retry, mark as "skipped - incompatible" |
| **Device rollback detected** | No retry, mark as "failed - rolled back", alert user |

After max retries: Mark device as "failed", require manual intervention to retry.

---

### 4. Confirmation and Stability

#### Simplified Approach

No complex timeout or stability tracking needed. The property controller simply compares the
version each device reports against the expected version. Mismatches are flagged in the UI.

#### Boot Reason Field

Add a `boot_reason` field to device reports (1 byte, no extra airtime):

```
boot_reason values:
  0x00 = Normal boot
  0x01 = Power cycle
  0x02 = Watchdog reset
  0x03 = OTA success (first boot after successful update)
  0x04 = OTA rollback (reverted to previous firmware)
  0x05 = Hard fault
```

This tells the property controller exactly what happened without needing to infer from timeouts.

#### FRAM OTA Logging

Devices log OTA state in FRAM to survive reboots:

```
FRAM OTA Log Structure:
  ota_state: enum (none, in_progress, success, failed, rolled_back)
  ota_target_version: semver (version we tried to install)
  ota_chunks_received: count
  ota_error_code: error code if failed
  ota_timestamp: when OTA started
```

**On next wake after OTA:**
- If `ota_state == success`: Report new version + `boot_reason = OTA_SUCCESS`
- If `ota_state == rolled_back`: Report old version + `boot_reason = OTA_ROLLBACK` + error_code
- Clear FRAM log after successful report to property controller

**On failure:** Device sends one `OTA_STATUS` message with error code from FRAM. This is the only
extra message, and only on failure - not on success.

#### Version Mismatch Handling

The UI should show a device list with:
- Expected version (from update process)
- Reported version (from device)
- Mismatch flag if they differ
- Boot reason (especially OTA_ROLLBACK)

Property admin/manager can see at a glance which devices succeeded, which failed, and why.

#### Notifications

Notifications are **UI-only** (in-app, push, email) - NOT extra LoRa messages.

| Event | Type | Delivery |
|-------|------|----------|
| Update process started | Info | In-app |
| Stage completed | Info | In-app |
| Auto-pause triggered | Warning | In-app + Push |
| Device rollback detected | Alert | In-app + Push |
| Update process completed | Success | In-app + Push |

Email notifications are optional and configurable per user.

---

### 5. Multi-Property Coordination

#### Firmware Sync vs Deploy

**Sync (automatic):** When a new release is published, all property controllers automatically
synchronize their local storage with the latest release(s). This happens in the background.

**Deploy (manual):** When and what version to deploy is the sole discretion of the
organization/property admin/manager. The property controller has the builds available locally,
ready to push when they choose.

#### Cloud → Property Controller Communication

The cloud pushes firmware updates to property controllers. Property controllers:
- Receive and cache firmware binaries
- Report device registry and update process status back to cloud
- Operate autonomously for the actual update process

#### Version Flexibility

Each property can set their own target version for their devices. This is normal and expected:
- Properties update at their own pace
- UI shows reported version vs target version, flags mismatches
- No enforcement of version uniformity across properties

#### Fleet Reporting (Consent Required)

AgSys can provide reporting showing:
- Version distribution across properties
- Device versions per property
- Update success rates

**Important:** This reporting is only available if the customer has provided consent for AgSys
to use their data. Consent must be explicitly granted per organization.

#### TODO: Migration Versioning Strategy

> Some firmware versions include breaking changes (e.g., FRAM layout changes). Skipping
> intermediate versions could cause data corruption. Need to define:
> - How to mark versions that require migration
> - How to enforce upgrade paths (prevent skipping required versions)
> - How to handle long-term versioning across many releases
>
> See separate discussion needed.

---

### 6. Scheduling

#### V1: Start Now Only

For the initial version, scheduling is not implemented. Updates are started manually by the
property admin/manager when they choose. Scheduling (e.g., "start at 2 AM") is a future feature.

#### Time Zone Handling

- **AgSys backend**: All timestamps stored and reported in UTC
- **UI**: Converts to user's chosen time zone for display
- **Property controller**: Runs NTP daemon with correct time zone configured

#### Update Duration Estimation

When starting an update process, the UI should show an **estimated duration** to complete all
selected devices. Calculation factors:

- Number of devices selected
- Device type (mains-powered vs battery)
- For battery devices: average wake interval (e.g., 2 hours for soil moisture)
- Firmware size and estimated transfer time per device
- Historical data on update success rate (if available)

Example: "Updating 20 soil moisture sensors. Estimated completion: 4-6 hours (devices wake every 2 hours)"

#### Growing Season / Blackout Periods

Not implemented for v1. Property admin/manager decides when it's best to start updates based on
their operational knowledge.

#### Future: Scheduled Updates

> For a future version, add ability to schedule updates:
> - "Start at 2 AM tonight"
> - "Start after irrigation cycle completes"
> - Recurring update windows

---

## API Contract

### API Structure (Details Deferred)

Detailed request/response schemas will be defined during implementation. This section outlines
the APIs needed.

```
┌─────────────┐     ┌─────────────┐     ┌─────────────────────┐
│  Flutter    │────▶│   AgSys     │────▶│ Property Controller │
│  App (UI)   │◀────│   Backend   │◀────│                     │
└─────────────┘     └─────────────┘     └─────────────────────┘
                          │
                          ▼
                    ┌───────────┐
                    │    S3     │
                    │ (firmware)│
                    └───────────┘
```

### Firmware Management API (Flutter ↔ AgSys)

**AgSys Admin only:**
- `POST /api/firmware` - Upload new firmware binary
- `GET /api/firmware` - List all firmware versions
- `GET /api/firmware/{id}` - Get firmware details
- `POST /api/firmware/{id}/publish` - Publish firmware (triggers sync to controllers)
- `DELETE /api/firmware/{id}` - Delete firmware (soft delete, mark inactive)

### Update Process API (Flutter ↔ AgSys)

**Property Admin/Manager:**
- `POST /api/properties/{id}/updates` - Create new update process
- `GET /api/properties/{id}/updates` - List update processes
- `GET /api/properties/{id}/updates/{id}` - Get update process details
- `POST /api/properties/{id}/updates/{id}/start` - Start update process
- `POST /api/properties/{id}/updates/{id}/pause` - Pause update process
- `POST /api/properties/{id}/updates/{id}/resume` - Resume update process
- `POST /api/properties/{id}/updates/{id}/cancel` - Cancel update process
- `POST /api/properties/{id}/updates/{id}/next-stage` - Advance to next stage

### Device Status API (Flutter ↔ AgSys)

- `GET /api/properties/{id}/devices` - List devices with version info
- `GET /api/properties/{id}/devices/{id}` - Get device details including update history

### Sync API (AgSys ↔ Property Controller)

- Property controller polls or receives push for new firmware
- Property controller reports device registry updates
- Property controller reports update process status changes

### Real-Time Updates

**V1: Polling** - UI polls for status updates during active update process.

| Frequency | Use Case |
|-----------|----------|
| 1 second | Watching active transfer |
| 5 seconds | Monitoring progress |
| 10 seconds | Default, background monitoring |

Polling frequency is user-selectable. Default: 10 seconds.

### TODO: Detailed API Schemas

> Define request/response schemas, error codes, and authentication requirements during
> implementation phase.

---

## State Machines

### Update Process States

```
                    ┌─────────┐
                    │ created │
                    └────┬────┘
                         │ start
                         ▼
┌──────────┐       ┌─────────────┐       ┌───────────┐
│  paused  │◀─────▶│ in_progress │──────▶│ completed │
└──────────┘       └──────┬──────┘       └───────────┘
     │                    │
     │                    │ cancel
     │                    ▼
     │              ┌───────────┐
     └─────────────▶│ cancelled │
                    └───────────┘
```

| State | Description |
|-------|-------------|
| `created` | Update process defined but not started |
| `in_progress` | Actively updating devices |
| `paused` | Stopped (manual or auto), can resume |
| `completed` | All devices processed (success, failed, or skipped) |
| `cancelled` | Manually cancelled, remaining devices skipped |

### Device Update States

Per device within an update process:

```
┌─────────┐     ┌─────────────┐     ┌───────────┐
│ pending │────▶│ in_progress │────▶│  success  │
└─────────┘     └──────┬──────┘     └───────────┘
     │                 │
     │                 ├───────────▶┌──────────┐
     │                 │            │  failed  │
     │                 │            └──────────┘
     │                 │
     │                 └───────────▶┌─────────────┐
     │                              │ rolled_back │
     │                              └─────────────┘
     │
     └────────────────────────────▶┌─────────┐
                                   │ skipped │
                                   └─────────┘
```

| State | Description |
|-------|-------------|
| `pending` | Waiting to be updated |
| `in_progress` | OTA transfer in progress |
| `success` | Device reports expected version |
| `failed` | Max retries exceeded |
| `rolled_back` | Device reverted to old firmware |
| `skipped` | Manually skipped or incompatible |

### Stage States (Staged Rollouts)

```
┌─────────┐     ┌─────────────┐     ┌───────────┐
│ pending │────▶│ in_progress │────▶│ completed │
└─────────┘     └─────────────┘     └───────────┘
```

| State | Description |
|-------|-------------|
| `pending` | Stage not yet started |
| `in_progress` | Devices in this stage being updated |
| `completed` | All devices in stage processed |

---

## UX Requirements

> **Note:** UX wireframes and detailed flows are TODO. This section outlines requirements based on
> backend design decisions. Use "update process" terminology (not "campaign") in all UI.

### A. Fleet Management (LoRa Path)

#### 1. Firmware Management Screen (AgSys Admin Only)

- List firmware by device type with version, release date, release type (stable/beta/dev)
- Upload new firmware with validation feedback
- Release notes display (markdown rendered)
- Publish button to sync to all property controllers
- Delete/deactivate old versions

#### 2. Update Process Configuration (Property Admin/Manager)

- Select device type and target version
- Show devices with version mismatch (current vs target)
- **Flag devices running newer version** - warn about downgrade risks
- Rollout strategy selection:
  - All at once
  - Staged: percentage-based, count-based, time-based, or manual
- Show **estimated duration** based on device count and wake intervals
- No scheduling in v1 - "Start Now" only

#### 3. Progress Monitoring

- Polling-based updates (user-selectable: 1s, 5s, 10s - default 10s)
- Per-device status view with state icons (pending, in_progress, success, failed, rolled_back, skipped)
- Stage progress for staged rollouts
- ETA calculation based on remaining devices and wake intervals
- Activity log/timeline of events

#### 4. Failure Handling

- Alert display for auto-pause triggers (20% failure rate, 3 consecutive failures)
- Actionable options: Resume, Skip device, Cancel process
- Device detail view showing:
  - Expected version vs reported version
  - Boot reason (especially OTA_ROLLBACK)
  - Error code from FRAM log
- Rollback notification with alert

#### 5. History and Reporting

- Update history per device
- Update process history with success rates
- Export capabilities (consent required for fleet-wide analytics)

#### 6. Notifications

- In-app: All events
- Push: Auto-pause, rollback, completion
- Email: Optional, user-configurable

### B. Direct Device Update (BLE Path)

#### 1. Device Connection Screen

- Show current firmware version when BLE connected
- Show available firmware version (from cloud/cache)
- "Update Available" badge if newer version exists
- Clear indication of BLE connection status

#### 2. BLE Update Flow

- "Update via BLE" button on device detail screen
- Confirmation dialog with version info and release notes
- Progress screen with:
  - Progress bar (BLE is faster, ~1-2 minutes)
  - "Stay within range" warning
  - Cancel option (with confirmation)
- Success/failure result screen
- Option to verify version after reboot

#### 3. Firmware Download and Caching

- App downloads firmware binary from cloud before BLE transfer
- Show download progress if not cached
- Cache last 5 versions per device type (as defined in Resolved Questions)
- Pre-download firmware for offline field work supported

#### 4. Offline BLE Updates

- Yes, BLE OTA works without internet if firmware is cached
- Field workers can pre-download firmware before going to remote sites
- After offline update, app queues version sync for when back online

#### 5. Version Sync After BLE Update

- App immediately pushes new version to cloud via API
- If offline, queued until connectivity restored
- Device's next LoRa report also confirms version (redundant sync)

### C. Valve Actuator Updates (BLE Only)

#### 1. Discovery and Connection

- Scan for nearby valve actuators (BLE advertising)
- Show list with signal strength and device address
- Connect to specific actuator via selection

#### 2. Update Flow

- Same as BLE path above
- No LoRa alternative available (valve actuators are CAN bus only)
- Must be physically present at valve box

#### 3. Bulk Actuator Updates

- Queue multiple actuators for sequential update
- Connect to each in sequence automatically
- Show overall progress across all actuators
- Handle individual failures without stopping entire queue

### D. Unified Device List

#### 1. Version Status Indicators

| Icon | Meaning |
|------|---------|
| ✓ | Up to date (matches target version) |
| ↑ | Update available |
| ⚠ | Update failed or rolled back |
| ? | Version unknown (device offline > 72 hours) |

#### 2. Update Method Indicators

| Icon | Meaning |
|------|---------|
| 📡 | LoRa OTA capable |
| 📶 | BLE only |
| Both | Device supports both methods |

#### 3. Filtering and Sorting

- Filter by: device type, version, update status, last seen
- Sort by: name, version, last seen, update status

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
