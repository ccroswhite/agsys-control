# AgSys Device Reliability and Longevity Design

## Overview

This document outlines the design considerations for ensuring AgSys field devices can operate reliably for **10+ years** without physical access. This is critical for devices deployed in remote agricultural locations where site visits are expensive and impractical.

### Target Devices

| Device | Power Source | Expected Lifetime | Environment |
|--------|--------------|-------------------|-------------|
| Soil Moisture Sensor | Battery | 10+ years | Outdoor, buried/exposed |
| Valve Controller | Mains | 10+ years | Outdoor, weatherproof enclosure |
| Water Meter | Mains | 10+ years | Outdoor, weatherproof enclosure |
| Valve Actuator | Mains (via controller) | 10+ years | Outdoor, weatherproof enclosure |

### Scale

- Devices deployed across multiple organizations/companies
- Expected fleet size: **100,000+ devices**
- Geographic distribution: Nationwide (various climates)

---

## Hardware Reliability

### Microcontroller: nRF52832

| Specification | Value | 10-Year Impact |
|---------------|-------|----------------|
| Flash endurance | 10,000 write cycles | ✅ OTA updates are rare (~10-20 lifetime) |
| Flash retention | 10 years @ 85°C | ⚠️ Temperature dependent |
| Operating temp | -40°C to +85°C | ✅ Suitable for outdoor |
| ESD tolerance | 2kV HBM | ✅ With proper enclosure |

### FRAM: MB85RS1MT (128KB) - Growth-Buffered Layout

| Specification | Value | 10-Year Impact |
|---------------|-------|----------------|
| Write endurance | 10^14 cycles | ✅ Essentially unlimited |
| Data retention | 10 years @ 85°C | ⚠️ Temperature dependent |
| Operating temp | -40°C to +85°C | ✅ Suitable for outdoor |

**Why FRAM over EEPROM:**
- No wear-out concerns (10^14 vs 10^6 cycles)
- Faster writes (no page erase needed)
- Lower power consumption
- Ideal for frequently-updated boot state and counters

**Why 128KB (MB85RS1MT):**
- Growth-buffered layout allows regions to expand without shifting data
- 16KB ring buffer for logs (sufficient for debugging)
- 76KB unallocated for future features
- Same cost as smaller 512Kbit devices
- Standard SPI interface, same commands as other SPI FRAM devices

**Layout Versioning:**
- Layout Header at 0x0000 is FROZEN FOREVER
- Contains version number and CRC
- Firmware runs migration code when version mismatch detected
- Growth buffers minimize version changes needed

### External Flash: W25Q16 (2MB) - A/B Slots for Future MCUs

| Specification | Value | 10-Year Impact |
|---------------|-------|----------------|
| Erase endurance | 100,000 cycles | ✅ Only written during OTA (rare) |
| Data retention | 20 years | ✅ Exceeds requirement |
| Operating temp | -40°C to +85°C | ✅ Suitable for outdoor |

**Layout - Future-Proofed for Larger MCUs:**
| Region | Size | Purpose |
|--------|------|---------|
| Slot A (Header + Firmware) | 948KB | Application backup (encrypted) |
| Slot B (Header + Firmware) | 948KB | OTA staging area |
| Bootloader Backup | 16KB | Recovery Loader source |
| Reserved | 136KB | Future expansion |

**Why 944KB firmware slots:**
- Current nRF52832 firmware: ~300KB (fits easily)
- Future nRF52840 firmware: ~800KB (supported)
- Allows migration to larger MCUs without changing external flash
- Future-proofs for 10+ year device lifetime

### Critical Temperature Consideration

Both flash and FRAM specify data retention at 85°C. In direct sunlight:
- Ambient: 40°C
- Enclosure heating: +20-30°C
- Internal heating: +5-10°C
- **Total: 65-80°C** (within spec, but close to limit)

**Mitigations:**
1. White/reflective enclosure to reduce solar heating
2. Ventilation slots (with IP rating maintained)
3. Mount in shaded locations where possible
4. Avoid black enclosures

---

## Firmware Reliability

### Watchdog Timer

All devices implement hardware watchdog:
- Timeout: 30 seconds (configurable)
- Fed by main application loop
- Triggers reset if firmware hangs

**Recovery from watchdog:**
1. Device resets
2. Bootloader checks boot_count
3. If boot_count > max_attempts → rollback to backup firmware
4. Otherwise, attempt to boot again

### Boot State Machine

```
┌─────────────────────────────────────────────────────────────────┐
│                     BOOT STATE MACHINE                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  POWER_ON ──────────────────────────────────────────────────┐   │
│      │                                                       │   │
│      ▼                                                       │   │
│  ┌─────────┐                                                 │   │
│  │ NORMAL  │◀─────────────────────────────────────────────┐ │   │
│  └────┬────┘                                               │ │   │
│       │ OTA received                                       │ │   │
│       ▼                                                    │ │   │
│  ┌──────────┐                                              │ │   │
│  │ STAGED   │ New firmware in external flash               │ │   │
│  └────┬─────┘                                              │ │   │
│       │ Apply & reboot                                     │ │   │
│       ▼                                                    │ │   │
│  ┌──────────┐     boot_count > max?     ┌──────────┐       │ │   │
│  │ PENDING  │─────────YES──────────────▶│ ROLLBACK │───────┘ │   │
│  └────┬─────┘                           └──────────┘         │   │
│       │ NO - try again                                       │   │
│       │                                                      │   │
│       │ Stable for 5 min?                                    │   │
│       │                                                      │   │
│       ▼ YES                                                  │   │
│  ┌───────────┐                                               │   │
│  │ CONFIRMED │───────────────────────────────────────────────┘   │
│  └───────────┘                                                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### OTA Confirmation Criteria

Firmware is only confirmed as "good" after demonstrating stable operation:

| Device | Confirmation Criteria | Time |
|--------|----------------------|------|
| Soil Moisture | Successful sensor read + LoRa TX | 5 min |
| Valve Controller | CAN bus operational + LoRa TX | 5 min |
| Water Meter | ADC operational + display working | 5 min |

### Automatic Rollback

If new firmware fails to confirm within max_boot_attempts (default: 3):
1. Bootloader restores backup from external flash
2. Device boots with previous known-good firmware
3. Rollback event reported to backend via VERSION_REPORT

---

## Update Safety

### Three-Stage Boot Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    BOOT CHAIN                                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────┐     ┌──────────────────┐     ┌─────────────┐       │
│  │   MBR   │────▶│ Recovery Loader  │────▶│ Bootloader  │────▶  │
│  │ (4KB)   │     │ (8KB) - FROZEN   │     │ (16KB)      │       │
│  │ Nordic  │     │                  │     │             │       │
│  └─────────┘     └──────────────────┘     └─────────────┘       │
│                          │                       │               │
│                          │ Checks CRC            │ Checks state  │
│                          │ Restores if bad       │ Rolls back    │
│                          ▼                       ▼               │
│                  ┌──────────────────┐     ┌─────────────┐       │
│                  │ Bootloader       │     │ Application │       │
│                  │ Backup (ext)     │     │ Backup (ext)│       │
│                  └──────────────────┘     └─────────────┘       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

| Component | Update Strategy | Recovery Mechanism |
|-----------|-----------------|-------------------|
| MBR | Never (Nordic) | None needed |
| Recovery Loader | **NEVER** (frozen) | None - must be correct |
| Bootloader | Rare, careful rollout | Recovery Loader restores |
| Application | Normal OTA | Bootloader restores |

### Staged Rollout

All firmware updates use staged rollout to minimize risk:

**Application Updates:**
| Stage | Devices | Wait | Criteria |
|-------|---------|------|----------|
| Canary | 1% | 24h | < 1% failure |
| Early | 10% | 48h | < 0.5% failure |
| General | 100% | - | - |

**Bootloader Updates (extra cautious):**
| Stage | Devices | Wait | Criteria |
|-------|---------|------|----------|
| Internal | 5-10 | 2 weeks | Zero issues |
| Canary | 0.1% | 2 weeks | Zero issues |
| Early | 1% | 2 weeks | < 0.01% issues |
| Slow | 10% | 1 month | < 0.01% issues |
| General | 100% | - | - |

### Auto-Pause on Failure

If failure rate exceeds threshold during rollout:
1. Campaign automatically paused
2. Alert sent to administrators
3. Manual review required before resuming
4. Option to rollback already-updated devices

---

## Data Integrity

### FRAM Usage

Critical data stored in FRAM (not flash) to avoid wear:
- Boot state and counters
- Firmware versions
- Bootloader CRC
- Device configuration
- Calibration data

### CRC Protection

All critical structures include CRC32:
- Boot info (FRAM)
- Firmware header
- Backup header (external flash)
- Staged firmware header

### Corruption Recovery

| Data | Detection | Recovery |
|------|-----------|----------|
| Boot info (FRAM) | CRC mismatch | Initialize to defaults |
| Bootloader (flash) | CRC mismatch | Recovery Loader restores |
| Application (flash) | CRC mismatch | Bootloader restores |
| Backup (ext flash) | CRC mismatch | Cannot recover - log error |

---

## Power Considerations

### Battery-Powered Devices (Soil Moisture Sensor)

- Sleep current: < 5µA
- Wake interval: 2 hours (configurable)
- Battery life target: 5+ years on 2x AA lithium
- Low battery detection and reporting

**OTA Impact:**
- OTA only during wake window
- Cannot be woken remotely for updates
- Updates may take days/weeks to complete across fleet

### Mains-Powered Devices

- Always-on operation
- Brown-out detection
- Graceful shutdown on power loss
- Resume operation on power restore

---

## Communication Reliability

### LoRa

- Range: Up to 10km line-of-sight
- Retry logic with exponential backoff
- Acknowledgment required for critical messages
- Message deduplication

### BLE

- Range: ~100m
- Used for local configuration and emergency updates
- PIN authentication for pairing
- Connection timeout handling

---

## Failure Modes and Mitigations

### Hardware Failures

| Failure | Detection | Mitigation |
|---------|-----------|------------|
| Flash wear-out | Unlikely (10K cycles) | Use FRAM for frequent writes |
| FRAM failure | CRC check | Initialize to defaults |
| External flash failure | CRC check | Cannot OTA - requires service |
| Crystal drift | Unlikely | Use internal RC as backup |
| Sensor failure | Out-of-range readings | Report error, continue operation |

### Software Failures

| Failure | Detection | Mitigation |
|---------|-----------|------------|
| Firmware crash | Watchdog | Reset and retry |
| Repeated crashes | Boot counter | Automatic rollback |
| Memory leak | Watchdog (eventual) | Reset and retry |
| Deadlock | Watchdog | Reset and retry |
| Corrupted state | CRC check | Initialize to defaults |

### Communication Failures

| Failure | Detection | Mitigation |
|---------|-----------|------------|
| LoRa timeout | No ACK | Retry with backoff |
| Property controller offline | No response | Queue messages, retry |
| Backend offline | Property controller handles | Local operation continues |

---

## Testing Requirements

### Pre-Deployment Testing

1. **Accelerated Life Testing (ALT)**
   - Run at 85°C for 3 months
   - Equivalent to ~10 years at 25°C (Arrhenius)
   - Monitor for failures

2. **HALT (Highly Accelerated Life Test)**
   - Temperature cycling: -40°C to +85°C
   - Vibration testing
   - Find weak points before deployment

3. **OTA Stress Testing**
   - 100+ consecutive OTA cycles
   - Power interruption during OTA
   - Corrupted firmware handling

4. **Recovery Testing**
   - Bootloader corruption recovery
   - Application corruption recovery
   - FRAM corruption recovery

### Production Testing

1. **Functional test** - All features working
2. **Calibration** - Sensor accuracy verification
3. **Flash programming** - Recovery Loader + Bootloader + Application
4. **Burn-in** - 24-48 hours at elevated temperature

---

## Monitoring and Diagnostics

### Device Health Reporting

Devices periodically report:
- Firmware version
- Boot reason (normal, watchdog, rollback)
- Uptime
- Error counts
- Battery level (if applicable)
- Signal strength (LoRa RSSI)

### Fleet Analytics

Backend tracks:
- OTA success/failure rates
- Rollback frequency
- Device uptime statistics
- Communication reliability
- Error trends

### Alerting

Automatic alerts for:
- High failure rate during OTA rollout
- Device offline for extended period
- Repeated rollbacks on same device
- Low battery warnings
- Unusual error patterns

---

## Recommendations Summary

### Hardware

1. Use **FRAM** for all frequently-written data
2. Use **white/reflective enclosures** to reduce temperature
3. **Derate components** for extended lifetime
4. Include **watchdog timer** on all devices

### Firmware

1. Implement **three-stage boot** (Recovery Loader → Bootloader → App)
2. Use **automatic rollback** after failed OTA
3. **Confirm firmware** only after stable operation
4. Store **critical state in FRAM**, not flash

### Operations

1. Use **staged rollout** for all updates
2. **Extra caution** for bootloader updates (3-month rollout)
3. **Monitor fleet health** continuously
4. **Auto-pause** on high failure rates

### Testing

1. **Accelerated life testing** before deployment
2. **OTA stress testing** with power interruption
3. **Recovery testing** for all failure modes
4. **Production burn-in** for early failure detection

---

## Appendix: Component Specifications

### nRF52832 (Nordic Semiconductor)

- Flash: 512KB, 10,000 write cycles, 10-year retention @ 85°C
- RAM: 64KB
- Operating voltage: 1.7V - 3.6V
- Operating temperature: -40°C to +85°C
- Datasheet: [nRF52832 Product Specification](https://infocenter.nordicsemi.com/pdf/nRF52832_PS_v1.4.pdf)

### MB85RS1MT (Fujitsu/RAMXEED)

- Part Number: MB85RS1MTPNF
- Capacity: 1Mbit (128KB)
- Write endurance: 10^14 cycles
- Data retention: 10 years @ 85°C
- Operating voltage: 2.7V - 3.6V
- Operating temperature: -40°C to +85°C
- Datasheet: [MB85RS1MT Datasheet](https://www.fujitsu.com/global/documents/products/devices/semiconductor/fram/lineup/MB85RS1MT-DS501-00028-4v0-E.pdf)

> **Note:** Using MB85RS1MT (128KB) at same cost as smaller 512Kbit devices.
> Standard SPI FRAM interface. Provides extensive log capacity in FRAM,
> extending external flash lifetime.

### W25Q16 (Winbond)

- Capacity: 16Mbit (2MB)
- Erase endurance: 100,000 cycles
- Data retention: 20 years
- Operating voltage: 2.7V - 3.6V
- Operating temperature: -40°C to +85°C
- Datasheet: [W25Q16 Datasheet](https://www.winbond.com/resource-files/w25q16jv%20spi%20revg%2003222018%20plus.pdf)

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-11 | AgSys | Initial version |
