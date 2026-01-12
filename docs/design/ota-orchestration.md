# Property Controller OTA Orchestration Design

## Overview

The property controller manages firmware updates for field devices (soil moisture sensors, valve controllers, water meters) over LoRa. This document addresses the key design questions for safe, reliable OTA updates.

## Device Types and Independent Updates

Each device type has its own firmware binary:
- **Soil Moisture Sensor** - `soilmoisture-vX.Y.Z.bin`
- **Valve Controller** - `valvecontrol-vX.Y.Z.bin`  
- **Water Meter (Magmeter)** - `watermeter-vX.Y.Z.bin`

Updates are **independent per device type**. You can update all soil moisture sensors while valve controllers and water meters continue normal operation.

### Firmware Storage on Property Controller

```
/data/firmware/
├── soilmoisture/
│   ├── current.bin -> v1.2.3.bin
│   ├── v1.2.3.bin
│   └── v1.2.2.bin (rollback)
├── valvecontrol/
│   ├── current.bin -> v2.1.0.bin
│   └── v2.1.0.bin
└── watermeter/
    ├── current.bin -> v1.0.5.bin
    └── v1.0.5.bin
```

---

## Update Modes

### 1. Normal Mode (Default) - "Trickle Update"

**Goal:** Update devices slowly without impacting normal operations.

**Behavior:**
- Update **one device at a time**
- Wait for device to confirm successful boot before starting next
- Insert **idle gaps** between chunk transmissions to allow normal LoRa traffic
- Respect device sleep cycles (don't wake sleeping sensors just for OTA)
- **Rate limit:** ~1 device per 15-30 minutes depending on firmware size

**Use Case:** Routine updates during growing season, non-critical improvements.

**Algorithm:**
```
for each device needing update (sorted by priority):
    wait for device's next wake window (if battery-powered)
    send OTA_START
    for each chunk:
        send OTA_CHUNK
        wait for ACK
        if other_device_needs_attention:
            pause OTA, handle other device
            resume OTA
        delay(TRICKLE_INTER_CHUNK_DELAY)  # 100-500ms
    send OTA_FINISH
    wait for device reboot and VERSION_REPORT
    if version_confirmed:
        mark device updated
        delay(INTER_DEVICE_DELAY)  # 5-15 minutes
    else:
        mark device failed, retry later
```

### 2. Aggressive Mode - "Fast Push"

**Goal:** Update all devices as quickly as possible.

**Behavior:**
- Update devices **in parallel** (up to 3-4 concurrent)
- Minimal delays between chunks
- **Mains-powered devices** (valve controllers, water meters): Update immediately
- **Battery-powered devices** (soil moisture sensors): Queue and update on next wake
- **Rate limit:** As fast as LoRa allows (~2-5 minutes per device)

**Use Cases:**
- **Emergency bug fix** - Critical security or safety issue
- **Off-season prep** - Update entire fleet before growing season
- **New installation** - Initial deployment of devices

**Device Wake Constraints:**
| Device Type | Power | Radio State | Update Timing |
|-------------|-------|-------------|---------------|
| Soil Moisture | Battery | Off 2h, On briefly | Must wait for wake window |
| Valve Controller | Mains | Always on | Immediate |
| Water Meter | Mains | Always on | Immediate |

> **Note:** Battery-powered devices cannot be woken remotely. The LoRa radio is completely off during deep sleep. Aggressive mode for these devices means "update as soon as they wake" rather than "wake them now."

**Algorithm:**
```
parallel_slots = 4
active_updates = []

while devices_needing_update:
    if len(active_updates) < parallel_slots:
        device = next_available_device()  # Skip sleeping devices
        if device.is_awake or device.is_mains_powered:
            start_ota(device)
            active_updates.append(device)
    
    for device in active_updates:
        if device.ota_complete:
            wait_for_version_confirm(device, timeout=60s)
            active_updates.remove(device)
        else:
            send_next_chunk(device)
```

### 3. Staged Rollout - "Canary Update"

**Goal:** Validate update on subset before full rollout.

**Behavior:**
- Update **canary group** first (10-20% of devices, or specific test devices)
- Monitor for 24-48 hours
- If no issues, proceed to remaining devices
- Auto-pause if failure rate exceeds threshold

**Use Case:** Major version updates, significant changes.

**Stages:**
1. **Canary (10%)** - Hand-picked or random sample
2. **Early Adopters (25%)** - Expand if canary successful
3. **Majority (50%)** - Continue rollout
4. **Remainder (100%)** - Complete rollout

---

## Firmware Confirmation Protocol

### How We Know Device Is Running New Firmware

**Problem:** Device could:
1. Fail to apply update (stays on old version)
2. Apply update but crash on boot (rollback to old version)
3. Apply update successfully (running new version)

**Solution:** Two-phase confirmation

### Shared Boot Info Library (freertos-common)

All devices (including valve actuators for future BLE OTA) will use a shared `agsys_boot_info` library that stores boot state in FRAM. This ensures boot reason survives power cycles and is consistent across all device types.

```c
// agsys_boot_info.h - Shared library for all devices

typedef enum {
    AGSYS_BOOT_REASON_NORMAL = 0x00,      // Normal boot (power cycle, watchdog)
    AGSYS_BOOT_REASON_OTA_SUCCESS = 0x01, // OTA completed, new firmware confirmed
    AGSYS_BOOT_REASON_OTA_ROLLBACK = 0x02,// OTA failed, reverted to previous
    AGSYS_BOOT_REASON_OTA_PENDING = 0x03, // OTA applied, awaiting confirmation
} agsys_boot_reason_t;

typedef struct {
    uint32_t magic;                  // 0xB007B007 - valid marker
    uint8_t  current_version[4];     // major, minor, patch, 0
    uint8_t  previous_version[4];    // version before OTA (if any)
    uint8_t  boot_reason;            // agsys_boot_reason_t
    uint8_t  boot_count;             // boots since last OTA
    uint16_t reserved;
    uint32_t ota_timestamp;          // tick count when OTA was applied
    uint32_t crc;                    // CRC of above fields
} agsys_boot_info_t;  // Stored in FRAM

// API
bool agsys_boot_info_init(agsys_fram_ctx_t *fram);
agsys_boot_reason_t agsys_boot_info_get_reason(void);
void agsys_boot_info_get_version(uint8_t *major, uint8_t *minor, uint8_t *patch);
void agsys_boot_info_get_previous_version(uint8_t *major, uint8_t *minor, uint8_t *patch);
uint32_t agsys_boot_info_get_uptime_seconds(void);

// Called by OTA module before reboot
void agsys_boot_info_set_ota_pending(uint8_t new_major, uint8_t new_minor, uint8_t new_patch);

// Called after successful boot to confirm OTA
void agsys_boot_info_confirm_ota(void);

// Called if OTA validation fails (triggers rollback flag)
void agsys_boot_info_mark_rollback(void);
```

### Uptime Tracking

FreeRTOS provides tick count since scheduler started via `xTaskGetTickCount()`. Convert to seconds:

```c
uint32_t agsys_boot_info_get_uptime_seconds(void)
{
    return xTaskGetTickCount() / configTICK_RATE_HZ;
}
```

This is already available in FreeRTOS - no additional implementation needed. The uptime is included in VERSION_REPORT to help the property controller assess device stability after an update

#### Phase 1: Post-Update Version Report

After OTA_FINISH and reboot, device sends a **VERSION_REPORT** message:

```c
// Message type 0x50: VERSION_REPORT
struct {
    uint8_t msg_type;           // 0x50
    uint8_t device_uid[8];      // Device unique ID
    uint8_t fw_major;           // Running firmware version
    uint8_t fw_minor;
    uint8_t fw_patch;
    uint8_t boot_reason;        // 0=normal, 1=ota_success, 2=ota_rollback
    uint32_t uptime_seconds;    // Time since boot
} version_report_t;
```

**Boot Reasons:**
- `0x00` - Normal boot (power cycle, watchdog)
- `0x01` - OTA success (new firmware confirmed)
- `0x02` - OTA rollback (new firmware failed, reverted)
- `0x03` - OTA pending confirm (waiting for validation timeout)

#### Phase 2: Stability Confirmation

Property controller tracks device stability after update:

```python
class DeviceUpdateStatus:
    device_uid: bytes
    target_version: str
    update_started: datetime
    update_completed: datetime | None
    version_confirmed: datetime | None
    reported_version: str | None
    boot_reason: int
    stability_checks: int  # Number of successful reports since update
    status: UpdateStatus  # PENDING, IN_PROGRESS, CONFIRMING, SUCCESS, FAILED, ROLLED_BACK
```

**Confirmation Criteria:**
1. Device reports expected version within 2 minutes of OTA_FINISH
2. Boot reason is `OTA_SUCCESS` (0x01)
3. Device sends at least 2 normal reports without rebooting

---

## Handling Update Failures

### Failure Scenarios

| Scenario | Detection | Recovery |
|----------|-----------|----------|
| Chunk transmission error | No ACK or error ACK | Retry chunk (3x), then abort |
| Device timeout during OTA | No response for 30s | Abort, retry later |
| CRC mismatch | OTA_FINISH returns error | Abort, retry from start |
| Boot failure | VERSION_REPORT shows rollback | Mark failed, investigate |
| Device offline after update | No VERSION_REPORT | Wait, then mark unknown |

### Automatic Retry Policy

```python
RETRY_POLICY = {
    'chunk_retries': 3,           # Retries per chunk
    'session_retries': 2,         # Full OTA session retries
    'retry_delay_minutes': 30,    # Wait between session retries
    'max_failures_before_skip': 3 # Skip device after N failures
}
```

### Rollback Detection

If device reports `boot_reason = OTA_ROLLBACK`:
1. Log failure with device UID and target version
2. Increment failure counter for that firmware version
3. If failure rate > 20%, **pause all updates** for that device type
4. Alert operator via API/notification

---

## LoRa Traffic Management

> **Important:** Traffic management happens entirely on the **property controller side**. 
> The field devices simply respond to whatever messages they receive - they don't need 
> any changes to support traffic prioritization. The property controller's LoRa task 
> decides when to send OTA chunks vs handle other traffic.

### Priority System (Property Controller)

```python
class LoRaMessagePriority(Enum):
    CRITICAL = 0      # Valve commands, alarms
    HIGH = 1          # Sensor reports, status
    NORMAL = 2        # Scheduled reports
    LOW = 3           # OTA chunks (normal mode)
    BACKGROUND = 4    # OTA chunks (trickle mode)
```

### Interleaving OTA with Normal Traffic (Property Controller Logic)

**Normal Mode:**
```
[OTA_CHUNK] -> [wait 200ms] -> [check rx queue, handle any incoming] -> [OTA_CHUNK] -> ...
```

**Aggressive Mode:**
```
[OTA_CHUNK] -> [OTA_CHUNK] -> [check critical queue only] -> [OTA_CHUNK] -> ...
```

### Bandwidth Allocation

| Mode | OTA Bandwidth | Normal Traffic |
|------|---------------|----------------|
| Trickle | 10-20% | 80-90% |
| Normal | 40-50% | 50-60% |
| Aggressive | 80-90% | 10-20% (critical only) |

---

## UX Design for AgSys Application

### Firmware Management Screen

```
┌─────────────────────────────────────────────────────────────┐
│  Firmware Updates                                    [⚙️]   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Soil Moisture Sensors                               │   │
│  │ Current: v1.2.3  │  Available: v1.3.0              │   │
│  │ Devices: 24 total │ 18 updated │ 6 pending         │   │
│  │ [Update All]  [Schedule]  [View Details]           │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Valve Controllers                                   │   │
│  │ Current: v2.1.0  │  Available: v2.1.0  ✓ Up to date│   │
│  │ Devices: 8 total │ 8 updated │ 0 pending           │   │
│  │ [View Details]                                      │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Water Meters                                        │   │
│  │ Current: v1.0.5  │  Available: v1.1.0              │   │
│  │ Devices: 12 total │ 0 updated │ 12 pending         │   │
│  │ [Update All]  [Schedule]  [View Details]           │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Update Configuration Dialog

```
┌─────────────────────────────────────────────────────────────┐
│  Update Soil Moisture Sensors to v1.3.0                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Update Mode:                                               │
│  ○ Trickle (Slowest, minimal impact)                       │
│  ● Normal (Balanced)                                        │
│  ○ Aggressive (Fastest, may impact operations)             │
│                                                             │
│  Rollout Strategy:                                          │
│  ○ All devices at once                                      │
│  ● Staged rollout (Canary → 25% → 50% → 100%)              │
│  ○ Selected devices only                                    │
│                                                             │
│  Schedule:                                                  │
│  ○ Start immediately                                        │
│  ● Start at: [2024-03-15 02:00 AM] (off-peak)              │
│  ○ Start after: [Growing season ends]                       │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ ⚠️ Release Notes for v1.3.0:                        │   │
│  │ - Improved battery life (est. +15%)                 │   │
│  │ - Fixed intermittent probe reading errors           │   │
│  │ - Added support for new calibration protocol        │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│              [Cancel]                    [Start Update]     │
└─────────────────────────────────────────────────────────────┘
```

### Update Progress View

```
┌─────────────────────────────────────────────────────────────┐
│  Updating Soil Moisture Sensors                      [⏸️]   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Progress: ████████████░░░░░░░░ 18/24 devices (75%)        │
│  Mode: Normal │ Started: 2h 15m ago │ ETA: 45 min          │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Stage: Majority (50%)                               │   │
│  │ ✓ Canary (3 devices) - Complete                     │   │
│  │ ✓ Early Adopters (6 devices) - Complete             │   │
│  │ ● Majority (12 devices) - 9/12 complete             │   │
│  │ ○ Remainder (3 devices) - Pending                   │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  Recent Activity:                                           │
│  │ 14:32 │ Sensor-017 │ ✓ Updated to v1.3.0            │   │
│  │ 14:28 │ Sensor-023 │ ✓ Updated to v1.3.0            │   │
│  │ 14:15 │ Sensor-008 │ ⚠️ Retry 1/3 (timeout)         │   │
│  │ 14:12 │ Sensor-011 │ ✓ Updated to v1.3.0            │   │
│                                                             │
│  [View All Devices]  [Pause Update]  [Cancel Update]        │
└─────────────────────────────────────────────────────────────┘
```

### Device Detail View (During/After Update)

```
┌─────────────────────────────────────────────────────────────┐
│  Sensor-017 (Soil Moisture)                                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Firmware Status:                                           │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Running Version: v1.3.0 ✓                           │   │
│  │ Previous Version: v1.2.3                            │   │
│  │ Updated: 2024-03-15 14:32                           │   │
│  │ Boot Reason: OTA Success                            │   │
│  │ Uptime: 2h 15m (stable)                             │   │
│  │ Stability: ✓ 5 reports since update                 │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  Update History:                                            │
│  │ 2024-03-15 │ v1.2.3 → v1.3.0 │ Success              │   │
│  │ 2024-01-20 │ v1.2.2 → v1.2.3 │ Success              │   │
│  │ 2023-11-05 │ v1.2.1 → v1.2.2 │ Rolled back          │   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Failure Alert

```
┌─────────────────────────────────────────────────────────────┐
│  ⚠️ Update Issue Detected                                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Sensor-008 rolled back to v1.2.3 after update attempt.    │
│                                                             │
│  Details:                                                   │
│  - Target version: v1.3.0                                   │
│  - Boot reason: OTA Rollback                                │
│  - Error: New firmware failed validation                    │
│                                                             │
│  This is the 2nd rollback for v1.3.0 (2/24 devices).       │
│                                                             │
│  Recommended Actions:                                       │
│  • Continue update (may be device-specific issue)          │
│  • Pause update and investigate                             │
│  • Skip this device                                         │
│                                                             │
│  [Continue]  [Pause All]  [Skip Device]  [View Logs]        │
└─────────────────────────────────────────────────────────────┘
```

---

## API Endpoints

### Firmware Management

```
GET  /api/firmware/                     # List all firmware versions
GET  /api/firmware/{device_type}/       # List versions for device type
POST /api/firmware/{device_type}/upload # Upload new firmware
GET  /api/firmware/{device_type}/{version}/download

GET  /api/devices/{device_type}/versions  # Current version per device
```

### Update Operations

```
POST /api/updates/                      # Start new update campaign
GET  /api/updates/                      # List active/recent updates
GET  /api/updates/{update_id}           # Get update status
PUT  /api/updates/{update_id}/pause     # Pause update
PUT  /api/updates/{update_id}/resume    # Resume update
DELETE /api/updates/{update_id}         # Cancel update

GET  /api/updates/{update_id}/devices   # Device-level status
```

### Example: Start Update

```json
POST /api/updates/
{
    "device_type": "soil_moisture",
    "target_version": "1.3.0",
    "mode": "normal",
    "rollout_strategy": "staged",
    "stages": [
        {"name": "canary", "percentage": 10},
        {"name": "early", "percentage": 25},
        {"name": "majority", "percentage": 50},
        {"name": "remainder", "percentage": 100}
    ],
    "schedule": {
        "start_at": "2024-03-15T02:00:00Z"
    },
    "auto_pause_threshold": 0.2  // Pause if >20% failure rate
}
```

### Example: Update Status Response

```json
{
    "update_id": "upd_abc123",
    "device_type": "soil_moisture",
    "target_version": "1.3.0",
    "mode": "normal",
    "status": "in_progress",
    "started_at": "2024-03-15T02:00:00Z",
    "progress": {
        "total_devices": 24,
        "completed": 18,
        "in_progress": 1,
        "pending": 4,
        "failed": 1,
        "percentage": 75
    },
    "current_stage": "majority",
    "stages": [
        {"name": "canary", "status": "complete", "success_rate": 1.0},
        {"name": "early", "status": "complete", "success_rate": 1.0},
        {"name": "majority", "status": "in_progress", "success_rate": 0.9},
        {"name": "remainder", "status": "pending"}
    ],
    "estimated_completion": "2024-03-15T05:00:00Z"
}
```

---

## Database Schema

```sql
-- Firmware versions available
CREATE TABLE firmware_versions (
    id SERIAL PRIMARY KEY,
    device_type VARCHAR(50) NOT NULL,
    version VARCHAR(20) NOT NULL,
    file_path VARCHAR(255) NOT NULL,
    file_size INTEGER NOT NULL,
    file_crc32 BIGINT NOT NULL,
    release_notes TEXT,
    uploaded_at TIMESTAMP DEFAULT NOW(),
    uploaded_by VARCHAR(100),
    is_current BOOLEAN DEFAULT FALSE,
    UNIQUE(device_type, version)
);

-- Update campaigns
CREATE TABLE update_campaigns (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    device_type VARCHAR(50) NOT NULL,
    target_version_id INTEGER REFERENCES firmware_versions(id),
    mode VARCHAR(20) NOT NULL,  -- trickle, normal, aggressive
    rollout_strategy VARCHAR(20) NOT NULL,  -- all, staged, selected
    status VARCHAR(20) NOT NULL,  -- scheduled, in_progress, paused, completed, cancelled
    scheduled_start TIMESTAMP,
    actual_start TIMESTAMP,
    completed_at TIMESTAMP,
    auto_pause_threshold DECIMAL(3,2),
    created_at TIMESTAMP DEFAULT NOW(),
    created_by VARCHAR(100)
);

-- Per-device update status
CREATE TABLE device_updates (
    id SERIAL PRIMARY KEY,
    campaign_id UUID REFERENCES update_campaigns(id),
    device_uid BYTEA NOT NULL,
    stage VARCHAR(20),
    status VARCHAR(20) NOT NULL,  -- pending, in_progress, confirming, success, failed, skipped
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    previous_version VARCHAR(20),
    confirmed_version VARCHAR(20),
    boot_reason INTEGER,
    retry_count INTEGER DEFAULT 0,
    error_message TEXT,
    UNIQUE(campaign_id, device_uid)
);

-- Update event log
CREATE TABLE update_events (
    id SERIAL PRIMARY KEY,
    campaign_id UUID REFERENCES update_campaigns(id),
    device_uid BYTEA,
    event_type VARCHAR(50) NOT NULL,
    event_data JSONB,
    created_at TIMESTAMP DEFAULT NOW()
);
```

---

## Implementation Phases

### Phase 1: Device-Side Infrastructure
- Implement `agsys_boot_info` shared library (FRAM-based boot reason, version tracking)
- Add VERSION_REPORT message (0x50) to LoRa protocol
- Integrate boot_info into all devices (soil moisture, valve controller, water meter, valve actuator)
- Wire up OTA modules to use boot_info for confirmation tracking

### Phase 2: Property Controller OTA Orchestrator
- Single device update via API
- Normal mode only
- VERSION_REPORT handling and confirmation tracking
- Basic progress reporting via API

### Phase 3: Campaign Management
- Multi-device campaigns
- All three update modes (trickle, normal, aggressive)
- Automatic confirmation tracking
- Pause/resume/cancel

### Phase 4: Staged Rollout
- Canary deployments
- Auto-pause on failure threshold
- Rollback detection and alerting

### Phase 5: Full UX
- Flutter UI for firmware management
- Real-time progress updates (WebSocket)
- Scheduling and notifications
- Update history and analytics

> **Note:** The UX and backend automation requirements are documented separately in 
> `docs/design/ota-automation-ux.md` to ensure we clearly define requirements before 
> implementation.

---

## Bootloader Requirements

### Software-Only Confirmation (Phase 1 - No Custom Bootloader)

Without a custom bootloader, we can still track OTA status using FRAM:

1. **Before reboot:** OTA module writes `OTA_PENDING` to FRAM with new version
2. **On boot:** Application reads FRAM, sees `OTA_PENDING`
3. **After N minutes of stable operation:** Application writes `OTA_SUCCESS` to FRAM
4. **On VERSION_REPORT:** Device reports boot_reason from FRAM

**Limitation:** If new firmware crashes immediately on boot, device will keep rebooting with bad firmware. No automatic rollback.

**Mitigation:** 
- Watchdog timer will reset device
- After 3 watchdog resets, device could check FRAM and refuse to run (but this requires bootloader support)

### Custom Bootloader (Phase 2 - Full Rollback Support)

For automatic rollback on crash, we need a custom bootloader that:

1. **Checks FRAM on every boot** for `OTA_PENDING` flag
2. **Starts validation timer** (e.g., 5 minutes)
3. **Boots application**
4. **If application confirms** (writes `OTA_SUCCESS`): Normal operation
5. **If timer expires or watchdog fires** without confirmation: 
   - Bootloader marks `OTA_ROLLBACK` in FRAM
   - Bootloader restores previous firmware from backup slot
   - Bootloader reboots into old firmware

This is documented separately in the bootloader design document.

### Recommendation

**Start with Phase 1 (software-only)** to get OTA working end-to-end. The FRAM tracking gives us visibility into update success/failure. Add custom bootloader in Phase 2 for automatic rollback safety.

---

## Open Questions

1. **Firmware signing:** Should we require signed firmware? (Recommended for production)
2. **Delta updates:** Worth implementing to reduce OTA time? (Complex, defer to later)
3. **Multi-property coordination:** How to handle updates across multiple properties?
4. **Offline property controllers:** How to sync firmware when controller reconnects?
