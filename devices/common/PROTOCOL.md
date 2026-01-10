# AgSys LoRa Protocol Specification

**Version:** 1.0  
**Date:** January 2026

## Overview

This document defines the LoRa communication protocol used between AgSys field devices and the property controller. All devices use the same protocol structure, encryption, and message formats.

## Architecture

```
┌─────────────────┐     LoRa (915 MHz)     ┌──────────────────────┐
│  Soil Moisture  │◄─────────────────────►│                      │
│     Sensor      │                        │                      │
└─────────────────┘                        │                      │
                                           │  Property Controller │
┌─────────────────┐     LoRa (915 MHz)     │   (Raspberry Pi +    │
│ Valve Controller│◄─────────────────────►│    RAK2245 +         │
│                 │                        │    Concentratord)    │
└─────────────────┘                        │                      │
                                           │                      │
┌─────────────────┐     LoRa (915 MHz)     │                      │
│   Water Meter   │◄─────────────────────►│                      │
└─────────────────┘                        └──────────────────────┘
```

## Encryption

### Algorithm
- **AES-128-GCM** (Galois/Counter Mode)
- Provides both confidentiality and authentication
- 4-byte truncated authentication tag

### Key Derivation
Each device has a unique encryption key derived from:

```
Key = SHA-256(SECRET_SALT || DEVICE_UID)[0:16]
```

Where:
- `SECRET_SALT`: 16-byte shared secret (same for all devices on property)
- `DEVICE_UID`: 8-byte unique device identifier (from MCU FICR registers)

This ensures:
- Each device has a unique key
- Property controller can derive any device's key from its UID
- Compromise of one device doesn't expose others' keys

### Nonce Management
- 4-byte counter, incremented for each transmission
- Must be persisted to NVRAM before each TX to survive power loss
- Counter starts at 0 and increments monotonically
- At 2-hour intervals, 4 billion packets = ~900,000 years (safe)

### Encrypted Packet Format

```
┌────────┬─────────────────────┬────────┐
│ Nonce  │     Ciphertext      │  Tag   │
│ 4 bytes│      N bytes        │ 4 bytes│
└────────┴─────────────────────┴────────┘
         │                     │
         └── Encrypted ────────┘
             (Header + Payload)
```

**Total overhead:** 8 bytes (4 nonce + 4 tag)

## Packet Header

All decrypted packets begin with a 15-byte header:

```
┌───────┬───────┬─────────┬─────────┬────────────┬────────────┬──────────┐
│ Magic │ Magic │ Version │ MsgType │ DeviceType │ Device UID │ Sequence │
│  'A'  │  'G'  │  1 byte │  1 byte │   1 byte   │  8 bytes   │ 2 bytes  │
└───────┴───────┴─────────┴─────────┴────────────┴────────────┴──────────┘
  0x41    0x47
```

| Field | Size | Description |
|-------|------|-------------|
| Magic | 2 | Protocol identifier: 0x41 0x47 ("AG") |
| Version | 1 | Protocol version (currently 1) |
| MsgType | 1 | Message type (see below) |
| DeviceType | 1 | Device type identifier |
| Device UID | 8 | Unique device ID from MCU |
| Sequence | 2 | Packet sequence number (little-endian) |

## Device Types

| Value | Device |
|-------|--------|
| 0x01 | Soil Moisture Sensor |
| 0x02 | Valve Controller |
| 0x03 | Water Meter |
| 0x04 | Valve Actuator (CAN only, not LoRa) |

## Message Types

Message types are organized by device/function for clarity:
- **0x00-0x0F**: Common messages (all devices)
- **0x10-0x1F**: Common controller→device messages
- **0x20-0x2F**: Soil moisture sensor
- **0x30-0x3F**: Water meter
- **0x40-0x4F**: Valve controller
- **0x50-0xDF**: Reserved for future devices
- **0xE0-0xEF**: OTA firmware updates

### Common Messages - All Devices (0x00 - 0x0F)

| Value | Name | Description |
|-------|------|-------------|
| 0x01 | HEARTBEAT | Device keepalive (optional, for devices without regular reports) |
| 0x02 | LOG_BATCH | Batch of stored readings (offline recovery) |
| 0x03 | CONFIG_REQUEST | Request configuration from controller |
| 0x0E | ACK | Generic acknowledgment |
| 0x0F | NACK | Negative acknowledgment |

### Common Controller → Device (0x10 - 0x1F)

| Value | Name | Description |
|-------|------|-------------|
| 0x10 | CONFIG_UPDATE | Configuration update (interval, calibration, etc.) |
| 0x11 | TIME_SYNC | Time synchronization |

### Soil Moisture Sensor (0x20 - 0x2F)

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| 0x20 | SOIL_REPORT | Device→Ctrl | Moisture/temp/battery reading |
| 0x21 | SOIL_CALIBRATE_REQ | Device→Ctrl | Request calibration data |

### Water Meter (0x30 - 0x3F)

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| 0x30 | METER_REPORT | Device→Ctrl | Flow rate, total volume, battery |
| 0x31 | METER_ALARM | Device→Ctrl | Leak/reverse flow/tamper alert (immediate) |
| 0x32 | METER_CALIBRATE_REQ | Device→Ctrl | Request calibration data |
| 0x33 | METER_RESET_TOTAL | Ctrl→Device | Reset totalizer (after meter replacement) |

### Valve Controller (0x40 - 0x4F)

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| 0x40 | VALVE_STATUS | Device→Ctrl | State change notification (only on change) |
| 0x41 | VALVE_ACK | Device→Ctrl | Command acknowledgment with commandId |
| 0x42 | VALVE_SCHEDULE_REQ | Device→Ctrl | Request schedule from controller |
| 0x43 | VALVE_COMMAND | Ctrl→Device | Open/close/stop/query |
| 0x44 | VALVE_SCHEDULE | Ctrl→Device | Schedule update |

### OTA Firmware (0xE0 - 0xEF)

| Value | Name | Direction | Description |
|-------|------|-----------|-------------|
| 0xE0 | OTA_ANNOUNCE | Ctrl→Device | Firmware available announcement |
| 0xE1 | OTA_CHUNK | Ctrl→Device | Firmware data chunk |
| 0xE2 | OTA_STATUS | Device→Ctrl | OTA progress/result |

## Payload Structures

### Sensor Report (0x20)

Sent by soil moisture sensors every 2 hours (configurable).

```
┌───────────┬────────────┬─────────────────────────────────┬───────────┬─────────────┬─────────────┬───────┐
│ Timestamp │ ProbeCount │ Probe Readings (4 × 4 bytes)    │ BatteryMv │ Temperature │ PendingLogs │ Flags │
│  4 bytes  │   1 byte   │         16 bytes                │  2 bytes  │   2 bytes   │   1 byte    │1 byte │
└───────────┴────────────┴─────────────────────────────────┴───────────┴─────────────┴─────────────┴───────┘
```

**Probe Reading (4 bytes each):**
```
┌────────────┬─────────────┬─────────────────┐
│ ProbeIndex │ FrequencyHz │ MoisturePercent │
│   1 byte   │   2 bytes   │     1 byte      │
└────────────┴─────────────┴─────────────────┘
```

**Flags:**
- Bit 0: Low battery
- Bit 1: First boot
- Bit 2: Config request
- Bit 3: Has pending logs

### Water Meter Report (0x30)

Sent by water meters every 60 seconds (configurable via CONFIG_UPDATE).

```
┌───────────┬─────────────┬─────────────┬─────────────┬───────────┬───────┐
│ Timestamp │ TotalPulses │ TotalLiters │ FlowRateLPM │ BatteryMv │ Flags │
│  4 bytes  │   4 bytes   │   4 bytes   │   2 bytes   │  2 bytes  │1 byte │
└───────────┴─────────────┴─────────────┴─────────────┴───────────┴───────┘
```

**Flags:**
- Bit 0: Low battery
- Bit 1: Reverse flow detected
- Bit 2: Leak detected
- Bit 3: Tamper detected

### Valve Status (0x40)

Sent by valve controller on state changes only (open→closed, closed→open, error).

```
┌───────────┬───────────────┬─────────────────────────────────────┐
│ Timestamp │ ActuatorCount │ Actuator Status (N × 5 bytes)       │
│  4 bytes  │    1 byte     │         variable                    │
└───────────┴───────────────┴─────────────────────────────────────┘
```

**Actuator Status (5 bytes each):**
```
┌─────────┬───────┬───────────┬───────┐
│ Address │ State │ CurrentMa │ Flags │
│  1 byte │1 byte │  2 bytes  │1 byte │
└─────────┴───────┴───────────┴───────┘
```

**Valve States:**
- 0x00: Closed
- 0x01: Open
- 0x02: Opening
- 0x03: Closing
- 0xFF: Error

**Flags:**
- Bit 0: Power fail
- Bit 1: Overcurrent
- Bit 2: Timeout
- Bit 3: On battery

### Valve Command (0x43)

Sent by property controller to open/close valves.

```
┌──────────────┬─────────┬───────────┬─────────────┐
│ ActuatorAddr │ Command │ CommandId │ DurationSec │
│    1 byte    │  1 byte │  2 bytes  │   2 bytes   │
└──────────────┴─────────┴───────────┴─────────────┘
```

**Commands:**
- 0x00: Close
- 0x01: Open
- 0x02: Stop
- 0x03: Query

**ActuatorAddr:** 1-64 for specific actuator, 0xFF for all

### Valve Ack (0x41)

Sent by valve controller to acknowledge a command.

```
┌──────────────┬───────────┬─────────────┬─────────┬───────────┐
│ ActuatorAddr │ CommandId │ ResultState │ Success │ ErrorCode │
│    1 byte    │  2 bytes  │   1 byte    │  1 byte │   1 byte  │
└──────────────┴───────────┴─────────────┴─────────┴───────────┘
```

**Error Codes:**
- 0x00: No error
- 0x01: Timeout
- 0x02: Overcurrent
- 0x03: Actuator offline
- 0x04: Power fail

### Time Sync (0x11)

Sent by property controller to synchronize device time.

```
┌───────────────┬───────────┐
│ UnixTimestamp │ UtcOffset │
│    4 bytes    │   1 byte  │
└───────────────┴───────────┘
```

### ACK (0x0E)

Generic acknowledgment for any message.

```
┌────────────────┬────────┬───────┐
│ AckedSequence  │ Status │ Flags │
│    2 bytes     │ 1 byte │1 byte │
└────────────────┴────────┴───────┘
```

**Flags:**
- Bit 0: Send logs requested
- Bit 1: Config available
- Bit 2: Time sync follows
- Bit 3: Schedule update follows

## LoRa Radio Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Frequency | 915 MHz | US ISM band |
| Bandwidth | 125 kHz | Standard |
| Spreading Factor | 9-10 | SF10 for sensors, SF9 for valve controller |
| Coding Rate | 4/5 | Good error correction |
| Sync Word | 0x34 | Private network |
| TX Power | 20 dBm | Maximum for range |
| Preamble | 8 symbols | Standard |

## Timing

| Device | Report Interval | Notes |
|--------|-----------------|-------|
| Soil Moisture | 2 hours (configurable) | Battery optimization |
| Water Meter | 60 seconds (configurable) | Flow monitoring, leak detection |
| Valve Controller | On state change only | Reduces airtime, sends on open/close/error |

All intervals are configurable via CONFIG_UPDATE (0x10) from property controller, which syncs from AgSys backend.

## Retry Logic

- **Timeout:** 500ms for ACK
- **Retries:** 3 attempts with exponential backoff
- **Backoff:** 1s → 2s → 4s (+ 0-50% jitter)

## Sequence Numbers

- 16-bit counter, wraps at 65535
- Used for:
  - Deduplication (ignore duplicate packets)
  - Ordering (detect out-of-order delivery)
  - ACK correlation

## Security Considerations

1. **Key Storage:** SECRET_SALT must be protected in firmware
2. **Nonce Persistence:** Counter must be saved to NVRAM before TX
3. **Replay Protection:** Use sequence numbers to detect replays
4. **Physical Security:** Devices should be tamper-resistant

## File Locations

- **Shared Protocol:** `devices/common/include/agsys_protocol.h`
- **Shared Crypto:** `devices/common/include/agsys_crypto.h`
- **Shared LoRa:** `devices/common/include/agsys_lora.h`
- **Property Controller:** `devices/property-controller/internal/protocol/`
