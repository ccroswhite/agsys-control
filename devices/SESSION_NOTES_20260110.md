# AgSys Development Session Notes - January 10-11, 2026

## Session Summary

This session focused on migrating the AgSys IoT devices from Arduino to FreeRTOS and establishing a single canonical LoRa protocol definition.

**Update (Jan 11):** All LoRa protocol conformance work completed. All three FreeRTOS devices now build successfully with canonical protocol.

---

## Completed Work

### 1. Arduino to FreeRTOS Migration
- ✅ Removed all Arduino device directories:
  - `devices/common/` (Arduino shared library)
  - `devices/soilmoisture/`
  - `devices/valvecontrol/`
  - `devices/valveactuator/`
  - `devices/watermeter/`
  - `devices/integration_tests/`

### 2. Canonical Protocol Definition
- ✅ Created protobuf definition: `agsys-api/proto/lora/v1/lora_protocol.proto`
- ✅ Generated C header: `agsys-api/gen/c/lora/v1/agsys_lora_protocol.h`
- ✅ Generated Go code: `agsys-api/gen/go/proto/lora/v1/lora_protocol.pb.go`
- ✅ Updated `freertos-common/include/agsys_protocol.h` to include canonical header

### 3. FreeRTOS Device Makefiles
- ✅ All Makefiles updated to include `agsys-api/gen/c/lora/v1`:
  - `soilmoisture-freertos/Makefile`
  - `valvecontrol-freertos/Makefile`
  - `watermeter-freertos/Makefile`
  - `valveactuator-freertos/Makefile`

### 4. Watermeter-FreeRTOS Menu System
- ✅ Implemented full LVGL menu system (~700 lines in `display.c`):
  - Main Menu (7 items)
  - PIN Entry (6-digit)
  - Display Settings (Units, Trend Period, Avg Period)
  - Flow Settings (Max Flow)
  - Alarm Settings (Leak Threshold, Duration, High Flow)
  - LoRa Config (Report Interval, Spreading Factor, Test Ping)
  - Calibration (Zero Cal, Reset Totalizer)
  - Diagnostics (LoRa Status, ADC Values)
  - About screen
- ✅ Button navigation implemented
- ✅ Build succeeds with LVGL stub

---

## Outstanding TODO Items

### Completed (Jan 11, 2026)

| # | Task | Device | Status |
|---|------|--------|--------|
| 1 | Migrate from local `agsys_header_t` to canonical protocol | valvecontrol-freertos | ✅ Done |
| 2 | Use canonical message types from `agsys_lora_protocol.h` | valvecontrol-freertos | ✅ Done |
| 3 | Implement LoRa task (currently TODO stubs only) | watermeter-freertos | ✅ Done |
| 4 | Add `lora_task.c` with RFM95 driver | watermeter-freertos | ✅ Done |
| 5 | Verify uses canonical protocol types | soilmoisture-freertos | ✅ Confirmed |

### Remaining

| # | Task | Notes |
|---|------|-------|
| 6 | Discuss BLE status icon and pairing mode | Design/implement visual BLE indicator and pairing UX |

---

## Current Device LoRa Status

| Device | Uses Canonical Protocol | Notes |
|--------|------------------------|-------|
| soilmoisture-freertos | ✅ Yes | Uses `agsys_header_t`, `agsys_soil_report_t`, `AGSYS_MSG_*` from canonical header |
| valvecontrol-freertos | ✅ Yes | Migrated to canonical protocol (Jan 11) |
| watermeter-freertos | ✅ Yes | New `lora_task.c` with RFM95 driver and canonical protocol (Jan 11) |
| valveactuator-freertos | N/A | CAN bus only, no LoRa |

---

## Key Files Modified This Session

### agsys-api
- `proto/lora/v1/lora_protocol.proto` - Created canonical protobuf
- `gen/c/lora/v1/agsys_lora_protocol.h` - Generated C header
- `gen/go/proto/lora/v1/lora_protocol.pb.go` - Generated Go code

### agsys-control/devices/freertos-common
- `include/agsys_protocol.h` - Now includes canonical header from agsys-api

### agsys-control/devices/watermeter-freertos
- `src/display.c` - Full menu system implementation
- `src/display.h` - Menu API
- `src/ui_types.h` - Updated PIN to 6 digits
- `src/lvgl.h` - Expanded LVGL stub for compilation
- `src/main.c` - Added `agsys_protocol.h` include, global flow data for LoRa
- `src/lora_task.c` - **NEW (Jan 11)** Full RFM95 driver with canonical protocol
- `src/lora_task.h` - **NEW (Jan 11)** LoRa task API
- `Makefile` - Added LVGL_STUB_ONLY, agsys-api include path, lora_task.c

### agsys-control/devices/soilmoisture-freertos
- `Makefile` - Added agsys-api include path

### agsys-control/devices/valvecontrol-freertos
- `src/lora_task.c` - **UPDATED (Jan 11)** Migrated to canonical protocol, removed local header
- `src/main.c` - Added `agsys_protocol.h` include
- `Makefile` - Added agsys-api include path, removed agsys_lora.c and OTA files

### agsys-control/devices/valveactuator-freertos
- `Makefile` - Added agsys-api include path

---

## Protocol Message Types (Canonical - from agsys_lora_protocol.h)

```
0x00-0x0F: Common messages (all devices)
  0x01 HEARTBEAT
  0x02 LOG_BATCH
  0x03 CONFIG_REQUEST
  0x0E ACK
  0x0F NACK

0x10-0x1F: Controller→Device
  0x10 CONFIG_UPDATE
  0x11 TIME_SYNC

0x20-0x2F: Soil Moisture
  0x20 SOIL_REPORT
  0x21 SOIL_CALIBRATE_REQ

0x30-0x3F: Water Meter
  0x30 METER_REPORT
  0x31 METER_ALARM
  0x32 METER_CALIBRATE_REQ
  0x33 METER_RESET_TOTAL

0x40-0x4F: Valve Controller
  0x40 VALVE_STATUS
  0x41 VALVE_ACK
  0x42 VALVE_SCHEDULE_REQ
  0x43 VALVE_COMMAND
  0x44 VALVE_SCHEDULE

0xE0-0xEF: OTA
  0xE0 OTA_ANNOUNCE
  0xE1 OTA_CHUNK
  0xE2 OTA_STATUS
```

---

## Next Steps (Suggested Order)

1. **Verify soilmoisture-freertos** - Quick check that it uses canonical types
2. **Migrate valvecontrol-freertos** - Replace local header/message definitions with canonical
3. **Implement watermeter-freertos LoRa** - Create lora_task.c with RFM95 driver
4. **BLE status icon** - Design and implement

---

## Build Commands

```bash
# Soilmoisture
cd /Users/chrisc/src/agsys-control/devices/soilmoisture-freertos && make

# Valvecontrol
cd /Users/chrisc/src/agsys-control/devices/valvecontrol-freertos && make

# Watermeter
cd /Users/chrisc/src/agsys-control/devices/watermeter-freertos && make

# Generate Go from proto
cd /Users/chrisc/src/agsys-api && buf generate
```

---

## Notes

- LVGL is currently stubbed (`LVGL_STUB_ONLY` defined in watermeter Makefile) for compilation testing
- For actual display functionality, integrate real LVGL v8.3.x library
- All FreeRTOS devices use nRF52840 with SoftDevice S140
