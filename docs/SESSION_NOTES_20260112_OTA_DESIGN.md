# OTA Design Session Notes - January 12, 2026

## Session Summary

This session focused on defining the OTA (Over-The-Air) update system design decisions. We worked through firmware storage, update process lifecycle, device tracking, and the LoRa OTA trigger mechanism.

---

## Completed Design Decisions

### 1. Firmware Storage and Management ✅

**Storage Architecture (Hybrid):**
- **AgSys Cloud (S3)**: Source of truth, keeps all versions, only AgSys admins can delete
- **Property Controller**: Caches last 5 versions per device type, auto-prunes older
- **Mobile App**: Downloads on demand or auto-sync, caches for offline BLE OTA

**Upload Workflow:**
1. Engineer builds firmware → Uploads via AgSys admin-only page
2. AgSys validates → Parses header, verifies SHA256, stores in S3
3. AgSys admin publishes → Triggers sync to all property controllers
4. Property controller receives → Stores locally
5. Property admin/manager → Uses property UI to deploy (LoRa OTA)
6. Mobile app → Downloads firmware, performs BLE OTA when on-site

**Database Schema:**
```sql
firmware:
  id: uuid
  device_type: enum (soil_moisture, valve_controller, valve_actuator, water_meter)
  hw_revision_min: int
  hw_revision_max: int (nullable)
  version: semver string
  size_bytes: int
  sha256: string
  storage_path: string (S3 key)
  release_notes: text (markdown)
  release_type: enum (stable, beta, development)
  published_at: timestamp (nullable)
  created_at: timestamp
  created_by: user_id
  is_active: bool
```

### 2. Update Process Lifecycle ✅

**Terminology:** Use "update" (not "upgrade") - covers both upgrades and downgrades. In UI, refer to "update process" (not "campaign").

**Workflow:**
| Action | Who | Description |
|--------|-----|-------------|
| Create | Property admin/manager | Select device type, target version, devices, rollout strategy |
| Start | Property admin/manager | Process begins |
| Pause | Admin/manager OR auto | Stops new updates, in-progress complete |
| Resume | Property admin/manager | Continues from where paused |
| Cancel | Property admin/manager | Stops, marks remaining as skipped |

**Auto-pause triggers:**
- Failure rate exceeds 20% (configurable)
- 3 consecutive failures

**Staged Rollouts:**
- Percentage-based, count-based, time-based, or manual
- Default: pause after each stage, user reviews and continues

**Property Controller Autonomy:** Operates independently, syncs with cloud when connected.

### 3. Device Tracking ✅

**Registry Synchronization:** Property controller and AgSys backend are eventually consistent. AgSys is source of truth for UI.

**Version Mismatch:** If device is newer than target, flag to user - downgrade possible but risks must be called out.

**Offline Devices:**
| Duration | Status | Action |
|----------|--------|--------|
| < 24 hours | Pending | Wait for next wake |
| 24-72 hours | Delayed | Flag in UI, keep waiting |
| > 72 hours | Unreachable | Flag in UI, allow manual skip |

**Retry Policy:**
| Failure Type | Behavior |
|--------------|----------|
| Timeout | Retry on next wake, max 3 attempts |
| Transfer error | Retry once immediately, then next wake |
| Device rejected | No retry, mark "skipped - incompatible" |
| Rollback detected | No retry, mark "failed - rolled back", alert user |

### 4. LoRa OTA Trigger Mechanism ✅

**Approach:** Flag in ACK response (minimal airtime)

```
ACK message format:
  [header][ack_type][flags]

flags byte:
  bit 0: OTA_PENDING (1 = update available, stay awake)
  bit 1-7: reserved
```

**OTA Flow:**
```
Device → Controller: SOIL_REPORT
Controller → Device: ACK (flags: OTA_PENDING=1)
Device → Controller: OTA_REQUEST
Controller → Device: OTA_ANNOUNCE (version, size, chunk_count)
Device → Controller: OTA_READY
Controller → Device: OTA_CHUNK[0]
... (chunked transfer)
Controller → Device: OTA_FINISH
Device: validates, reboots
Device → Controller: VERSION_REPORT (new version)
```

---

## Completed This Session

### All Backend Design Sections Complete 

1. **Firmware Storage and Management** - Hybrid storage (S3 + property controller + mobile app)
2. **Update Process Lifecycle** - Terminology ("update" not "campaign"), workflow, auto-pause, staged rollouts
3. **Device Tracking** - Registry sync, LoRa OTA trigger (ACK flag), retry policy
4. **Confirmation and Stability** - Boot reason field, FRAM logging, version mismatch UI
5. **Multi-Property Coordination** - Sync vs deploy, version flexibility, consent required
6. **Scheduling** - V1 start now only, time zones (UTC + NTP), ETA estimation
7. **API Contract** - Structure defined, details deferred to implementation
8. **State Machines** - Update process, device, and stage states

---

## Remaining Design TODOs

### UX Design (Need Discussion)
1. **Property Admin/Manager Deployment UX** - Pages for managing updates to devices
2. **Breaking Changes / requires_version Strategy** - How to handle upgrade paths
3. **Long-term Code Migration and Versioning** - FRAM layout changes, etc.
4. **Customer Consent for Data Reporting** - Opt-in for fleet analytics

### Implementation TODOs (COMPLETED Jan 12, 2026)
5. ✅ **Implement LoRa OTA trigger** - Added `AGSYS_ACK_FLAG_OTA_PENDING` to `agsys_lora_protocol.h`
6. ✅ **Add boot_reason field** - Added to all device reports (soil, meter, valve)
7. ✅ **Add FRAM OTA logging** - Added `agsys_ota_fram_state_t` to `agsys_memory_layout.h`
8. ✅ **Update all devices** - Updated soil moisture, water meter, valve controller

### Files Modified (Device Firmware)
- `agsys-api/gen/c/lora/v1/agsys_lora_protocol.h` - OTA messages, boot_reason, ACK flag
- `devices/freertos-common/include/agsys_memory_layout.h` - FRAM OTA state structure
- `devices/soilmoisture-freertos/src/lora_task.c` - fw_version, boot_reason, OTA_PENDING check
- `devices/watermeter-freertos/src/lora_task.c` - fw_version, boot_reason, OTA_PENDING check
- `devices/valvecontrol-freertos/src/lora_task.c` - fw_version, boot_reason in status report

### Files Modified (Property Controller - Go)
- `agsys-api/pkg/lora/protocol.go` - OTA types, ACK flags, boot reasons, OTA payloads
- `devices/property-controller/internal/protocol/messages.go` - Re-export OTA types
- `devices/property-controller/internal/ota/manager.go` - NEW: OTA manager
- `devices/property-controller/internal/engine/engine.go` - OTA integration

### Property Controller OTA Flow
1. Cloud notifies controller of new firmware available
2. Controller downloads and caches firmware locally
3. When device reports, controller checks if update needed
4. If update available, ACK includes `OTA_PENDING` flag
5. Device sees flag, sends `OTA_REQUEST`
6. Controller sends `OTA_ANNOUNCE` with firmware details
7. Device sends `OTA_READY` with starting chunk
8. Controller sends chunks via `OTA_CHUNK`
9. Controller sends `OTA_FINISH` when done
10. Device verifies CRC, applies update, reboots
11. Device reports `OTA_STATUS` with success/failure and `boot_reason`

### gRPC Firmware Service (NEW)
Added to `agsys-api/proto/controller/v1/controller.proto`:
- `FirmwareService` - Separate service for large file transfers
- `GetLatestFirmware` - Returns info about latest firmware for device type
- `DownloadFirmware` - Streams firmware binary to controller
- `ReportOTAStatus` - Reports OTA result to backend

**Note:** Run `make proto` in agsys-api to generate Go bindings after proto changes.

### Future Features (Deferred)
9. **Scheduled Updates** - "Start at 2 AM tonight", recurring windows
10. **WebSocket for real-time updates** - Currently using polling (default 10s)

---

## Other Outstanding TODOs (from previous sessions)

### BLE / Mobile
- [ ] Test BLE icon on hardware (verify appearance and flash rates)
- [ ] Add BLE pairing to other devices (valve controller, soil moisture LED only)
- [ ] Flutter app BLE integration (scan, connect, PIN entry UI)
- [ ] Device configuration via BLE (CONFIG_CHANGED, COMMAND_RECEIVED events)
- [ ] Order display connector - Hirose FH12S-40S-0.5SH(55)

### Hardware (Low Priority)
- [ ] Convert water meter markdown schematic to Eagle format
- [ ] Generate Gerber files for PCB fabrication
- [ ] Order prototype PCBs

---

## Key Files

- **OTA Design Document**: `docs/design/ota-automation-ux.md` (being updated with decisions)
- **LoRa Protocol**: `agsys-api/proto/lora/v1/lora_protocol.proto`
- **C Header**: `agsys-api/gen/c/lora/v1/agsys_lora_protocol.h`
- **Session Notes**: `docs/SESSION_NOTES_20260112.md` (general session notes)

---

## Resume Instructions

When resuming:
1. Review this file for context on completed design decisions
2. Pick up remaining design TODOs (UX, breaking changes, migration, consent)
3. Or continue with implementation TODOs (LoRa OTA trigger, boot_reason, FRAM logging)
4. Full design document: `docs/design/ota-automation-ux.md`
