# Session Notes - January 12, 2026

## Summary

This session completed the hardware schematic and BOM review for all 4 field devices, ensuring they are ready for custom PCB manufacturing.

---

## Completed Work

### 1. Schematic Updates (All 4 Devices)

All schematics now include the "glue" components needed for PCB manufacturing:

| Device | File | Board Cost |
|--------|------|------------|
| Soil Moisture | `soilmoisture-freertos/docs/soilmoisture_schematic.sch` | ~$19 |
| Valve Actuator | `valveactuator-freertos/docs/valveactuator_schematic.sch` | ~$13 |
| Valve Controller | `valvecontrol-freertos/docs/valvecontrol_schematic.sch` | ~$35 |
| Water Meter | `watermeter-freertos/hardware/MAIN_BOARD_SCHEMATIC.md` | ~$48 |

### 2. Components Added to All Schematics

- **Decoupling capacitors** on all ICs (100nF, 10uF as needed)
- **Crystal circuits** with proper load capacitors (32MHz + 32.768kHz)
- **nRF52 DC-DC inductor** (10nH) and DEC pin capacitors
- **ESD/TVS protection** on external interfaces
- **Pull-up resistors** where needed (I2C, buttons, DIP switches)
- **Input protection** (reverse polarity P-FET, TVS diodes)
- **Antenna matching networks** for LoRa modules (π-filter)
- **RC filters** for ADC inputs (using C0G/NP0 for low microphonics)

### 3. BOM Updates

All BOMs updated with complete passive component lists:
- `soilmoisture-freertos/docs/BOM.md`
- `valveactuator-freertos/docs/BOM.md`
- `valvecontrol-freertos/docs/BOM.md`
- `watermeter-freertos/docs/BOM.md`

### 4. RTC Design Decision

**Confirmed:** Only the Valve Controller has an external RTC (RV-3028-C7 with CR2032 backup). All other devices use the internal nRF52 RTC with 32.768kHz crystal, syncing time via LoRa.

| Device | RTC |
|--------|-----|
| Valve Controller | External RV-3028-C7 + CR2032 backup |
| Soil Moisture | Internal nRF52832 RTC, syncs via LoRa |
| Valve Actuator | Internal nRF52810 RTC (no time-critical functions) |
| Water Meter | Internal nRF52840 RTC, syncs via LoRa |

---

## Outstanding TODOs

### Hardware (Low Priority - Schematics Complete)

- [ ] Convert water meter markdown schematic to Eagle format
- [ ] Generate Gerber files for PCB fabrication
- [ ] Order prototype PCBs

### OTA Automation & UX (High Priority)

From `docs/design/ota-automation-ux.md`:

#### Backend Automation
- [ ] Define firmware storage and management (S3, local, etc.)
- [ ] Define update campaign lifecycle (create, start, pause, resume, cancel)
- [ ] Define device tracking (wake windows, offline handling, retry policy)
- [ ] Define confirmation and stability criteria
- [ ] Define multi-property coordination
- [ ] Define scheduling (time zones, off-peak, growing season)

#### UX Requirements
- [ ] Design Fleet Management screens (LoRa path)
- [ ] Design Update Configuration / Campaign screens
- [ ] Design Progress Monitoring screens
- [ ] Design Failure Handling UX
- [ ] Design History and Reporting screens
- [ ] Design Notification system (in-app, push, email)
- [ ] Design BLE Update Flow screens
- [ ] Design Valve Actuator bulk update UX

#### API & State Machines
- [ ] Define detailed API specifications (request/response schemas)
- [ ] Define error codes and handling
- [ ] Define WebSocket events for real-time updates
- [ ] Define Update Campaign state machine
- [ ] Define Device Update state machine
- [ ] Define Stage state machine (for staged rollout)

#### RBAC & Security
- [ ] Define organization roles and RBAC for multi-property management
- [ ] Document BLE PIN auth vs user auth gap
- [ ] Implement OTA audit logging system

#### Metrics
- [ ] Implement user opt-in/out mechanism for sharing metrics

### Next Steps for OTA (from document)

1. Review and finalize requirements with stakeholders
2. Create detailed API specifications
3. Design database schema
4. Implement backend automation
5. Implement Flutter UX

---

## Key Files Modified This Session

### Schematics
- `/devices/soilmoisture-freertos/docs/soilmoisture_schematic.sch`
- `/devices/valveactuator-freertos/docs/valveactuator_schematic.sch`
- `/devices/valvecontrol-freertos/docs/valvecontrol_schematic.sch`
- `/devices/watermeter-freertos/hardware/MAIN_BOARD_SCHEMATIC.md`

### BOMs
- `/devices/soilmoisture-freertos/docs/BOM.md`
- `/devices/valveactuator-freertos/docs/BOM.md`
- `/devices/valvecontrol-freertos/docs/BOM.md`
- `/devices/watermeter-freertos/docs/BOM.md`

---

## Reference: Standard Memory Bus Pins (All Devices)

| Signal | Pin | Description |
|--------|-----|-------------|
| MEM_SCK | P0.26 | Memory SPI Clock |
| MEM_MOSI | P0.25 | Memory SPI MOSI |
| MEM_MISO | P0.24 | Memory SPI MISO |
| FRAM_CS | P0.23 | MB85RS1MTPNF Chip Select |
| FLASH_CS | P0.22 | W25Q16JVSSIQ Chip Select |

---

## Reference: Device MCU Selection

| Device | MCU | Cost | Notes |
|--------|-----|------|-------|
| Soil Moisture | nRF52832-QFAA | $2.50 | BLE, low power |
| Valve Controller | nRF52832-QFAA | $2.50 | BLE, CAN, LoRa |
| Valve Actuator | nRF52810-QFAA | $1.75 | BLE, CAN only |
| Water Meter | nRF52840-QIAA | $4.00 | BLE, USB, more GPIO |
