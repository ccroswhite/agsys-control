# AgSys Property Controller

Property controller for the AgSys agricultural IoT system. Runs on Raspberry Pi 5 with RAK2245 LoRa HAT, acting as a gateway between field devices and the AgSys cloud.

## Overview

The property controller is the "master brains" for all devices on a customer property. It serves as a two-way gateway between:
- **Field Devices**: Soil moisture sensors, water meters, valve controllers (via LoRa)
- **AgSys Cloud**: Main UI and application backend (via gRPC)

### Responsibilities

1. **Data Collection & Forwarding**
   - Receive sensor data over LoRa from soil moisture sensors (every 2 hours)
   - Receive water meter readings (every 5 minutes)
   - Store locally in SQLite database
   - Forward to AgSys cloud when connected

2. **Configuration Management**
   - Pull/receive device configurations from AgSys:
     - Property name and UID
     - Zone names and UIDs
     - Registered device list (sensors, controllers, meters) with UIDs and aliases
   - Only process data from registered devices

3. **Valve Control**
   - **Immediate Commands**: Push open/close commands from cloud to valve controllers
   - **Schedule Updates**: Store schedules locally; valve controllers pull updates
   - **Acknowledgment Tracking**: Confirm valve state changes back to cloud

4. **Offline Operation**
   - Queue data locally when cloud is unavailable
   - Sync automatically when connection restored
   - Valve controllers continue operating on cached schedules

## Features

- **LoRa Gateway**: Communicates with soil moisture sensors, water meters, and valve controllers via LoRa (915 MHz)
- **Cloud Sync**: gRPC bidirectional streaming to AgSys cloud for real-time data flow
- **Local Storage**: SQLite database for offline operation and data caching
- **Valve Control**: Immediate command execution with acknowledgment tracking
- **Schedule Management**: Stores and distributes watering schedules to valve controllers
- **AES-128 Encryption**: Secure LoRa communication

## Hardware Requirements

- Raspberry Pi 5 (or Pi 4/3)
- RAK2245 Pi HAT (SX1301 LoRa concentrator)
- Ethernet or WiFi connectivity

## ChirpStack Concentratord Setup

The property controller uses [ChirpStack Concentratord](https://github.com/chirpstack/chirpstack-concentratord) for LoRa communication. Concentratord is a production-proven, Rust-based driver that provides:

- Full SX1301 hardware support (8-channel concurrent reception)
- Multi-spreading factor reception
- Precise timing for downlinks
- ZeroMQ API for easy integration

### Why Concentratord?

| Option | Pros | Cons |
|--------|------|------|
| **Concentratord** | Production-proven, Rust (memory-safe), clean ZMQ API, actively maintained | Separate process |
| libloragw (CGO) | Direct integration | Requires CGO, complicates cross-compilation |
| Raw SPI | No dependencies | Complex, error-prone, reinventing the wheel |

### Install Concentratord on Raspberry Pi

```bash
# Download the latest release for your architecture
# Check https://github.com/chirpstack/chirpstack-concentratord/releases
wget https://github.com/chirpstack/chirpstack-concentratord/releases/download/v4.4.0/chirpstack-concentratord_4.4.0_linux_arm64.tar.gz

# Extract
tar -xzf chirpstack-concentratord_4.4.0_linux_arm64.tar.gz

# Install binary
sudo mv chirpstack-concentratord /usr/local/bin/

# Create config directory
sudo mkdir -p /etc/chirpstack-concentratord/rak2245
```

### Configure Concentratord for RAK2245

Create `/etc/chirpstack-concentratord/rak2245/concentratord.toml`:

```toml
# Concentratord configuration for RAK2245 (SX1301)

[concentratord]
log_level = "INFO"
stats_interval = "30s"

# ZeroMQ API endpoints (must match agsys-controller config)
[concentratord.api]
event_bind = "ipc:///tmp/concentratord_event"
command_bind = "ipc:///tmp/concentratord_command"

# Gateway configuration
[gateway]
lorawan_public = false
model = "rak_2245"
region = "US915"

# SX1301 reset GPIO (RAK2245 uses GPIO 17)
reset_pin = 17

# US915 channel configuration (channels 8-15 + 65)
[[gateway.concentrator.multi_sf_channel]]
frequency = 903900000
[[gateway.concentrator.multi_sf_channel]]
frequency = 904100000
[[gateway.concentrator.multi_sf_channel]]
frequency = 904300000
[[gateway.concentrator.multi_sf_channel]]
frequency = 904500000
[[gateway.concentrator.multi_sf_channel]]
frequency = 904700000
[[gateway.concentrator.multi_sf_channel]]
frequency = 904900000
[[gateway.concentrator.multi_sf_channel]]
frequency = 905100000
[[gateway.concentrator.multi_sf_channel]]
frequency = 905300000

[gateway.concentrator.lora_std]
frequency = 904600000
bandwidth = 500000
spreading_factor = 8

[gateway.concentrator.fsk]
frequency = 904800000
bandwidth = 125000
datarate = 50000
```

### Install Concentratord Service

Create `/etc/systemd/system/chirpstack-concentratord.service`:

```ini
[Unit]
Description=ChirpStack Concentratord
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/chirpstack-concentratord -c /etc/chirpstack-concentratord/rak2245/concentratord.toml
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable chirpstack-concentratord
sudo systemctl start chirpstack-concentratord

# Check status
sudo systemctl status chirpstack-concentratord
journalctl -u chirpstack-concentratord -f
```

### Verify Concentratord is Running

```bash
# Check ZeroMQ sockets are created
ls -la /tmp/concentratord_*

# Should show:
# /tmp/concentratord_command
# /tmp/concentratord_event
```

## Installation

### Build from Source

```bash
cd property-controller

# Download dependencies
go mod tidy

# Build binaries
go build -o bin/agsys-controller ./cmd/agsys-controller
go build -o bin/agsys-db ./cmd/agsys-db

# Install (as root)
sudo cp bin/agsys-controller /usr/local/bin/
sudo cp bin/agsys-db /usr/local/bin/
```

### Configure

```bash
# Create directories
sudo mkdir -p /etc/agsys /var/lib/agsys /var/log/agsys

# Copy configuration
sudo cp configs/config.yaml /etc/agsys/controller.yaml

# Edit configuration
sudo nano /etc/agsys/controller.yaml
```

Required configuration:
- `property.uid`: Your property UID from AgSys
- `cloud.api_key`: API key from AgSys
- `lora.aes_key`: 16-byte AES key (32 hex chars) matching your devices

### Install Service

```bash
# Create service user
sudo useradd -r -s /bin/false agsys

# Set permissions
sudo chown -R agsys:agsys /var/lib/agsys /var/log/agsys

# Install systemd service
sudo cp configs/agsys-controller.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable agsys-controller
sudo systemctl start agsys-controller
```

## Usage

### Run Controller

```bash
# Foreground (for testing)
agsys-controller run --config /etc/agsys/controller.yaml

# As service
sudo systemctl start agsys-controller
sudo systemctl status agsys-controller
journalctl -u agsys-controller -f
```

### Database CLI

```bash
# List devices
agsys-db devices

# Show sensor readings
agsys-db sensor                    # All sensors
agsys-db sensor DEVICE_UID -n 50   # Specific device, 50 records

# Show water meter readings
agsys-db meter

# Show valve states
agsys-db valves

# Show valve events
agsys-db events

# Show schedules
agsys-db schedules

# Show pending commands
agsys-db pending

# Database statistics
agsys-db stats

# Raw SQL query (SELECT only)
agsys-db query "SELECT * FROM devices WHERE device_type = 1"
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                  Property Controller (Pi 5 + RAK2245)            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                    agsys-controller (Go)                    │ │
│  │                                                             │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │ │
│  │  │LoRa Handler │  │  │ Core Engine │  │ Cloud Client (gRPC) │ │ │
│  │  │ (SX1301 SPI)│◄─┤             ├─►│ - Config pull       │ │ │
│  │  │ - RX/TX     │  │ - SQLite DB │  │ - Telemetry push    │ │ │
│  │  │ - AES-128   │  │ - Routing   │  │ - Command receive   │ │ │
│  │  └─────────────┘  │ - Queuing   │  └─────────────────────┘ │ │
│  │                   └─────────────┘                          │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
         │                                         │
         ▼ LoRa 915MHz                             ▼ gRPC
   ┌───────────┐                            ┌───────────┐
   │  Devices  │                            │  AgSys    │
   │ - Sensors │                            │  Cloud    │
   │ - Valves  │                            │           │
   │ - Meters  │                            │           │
   └───────────┘                            └───────────┘
```

## Data Flow

### Device → Cloud
1. Device sends LoRa packet
2. Controller receives, decrypts, validates
3. Data stored in SQLite (synced_to_cloud = false)
4. Sync loop sends to cloud via gRPC stream
5. On success, marks synced_to_cloud = true

### Cloud → Device (Immediate Command)
1. Cloud sends valve command via gRPC stream
2. Controller creates pending command record
3. Sends encrypted LoRa packet to device
4. Device executes, sends acknowledgment
5. Controller updates pending command, notifies cloud

### Cloud → Device (Schedule)
1. Cloud sends schedule update via gRPC stream
2. Controller stores in SQLite
3. Valve controller periodically requests schedule
4. Controller sends schedule via LoRa

## LoRa Protocol

Messages use a simple binary format with AES-128-CTR encryption:

| Field | Size | Description |
|-------|------|-------------|
| Device UID | 8 bytes | MCU unique identifier |
| Message Type | 1 byte | Message type code |
| Sequence | 2 bytes | Sequence number |
| Payload | Variable | Message-specific data |

Message types:
- `0x01`: Sensor data (device → controller)
- `0x02`: Water meter data (device → controller)
- `0x03`: Valve status (device → controller)
- `0x04`: Valve acknowledgment (device → controller)
- `0x05`: Schedule request (device → controller)
- `0x10`: Valve command (controller → device)
- `0x11`: Schedule update (controller → device)
- `0x13`: Time sync (controller → device, broadcast)

## Configuration Reference

```yaml
property:
  uid: "PROP-12345"      # Property UID from AgSys
  name: "My Vineyard"

cloud:
  grpc_addr: "grpc.agsys.io:443"  # gRPC server address
  api_key: "your-api-key"
  use_tls: true                    # Use TLS for production

lora:
  # Concentratord ZeroMQ endpoints
  event_url: "ipc:///tmp/concentratord_event"
  command_url: "ipc:///tmp/concentratord_command"
  # TX parameters
  frequency: 915000000   # 915 MHz (US)
  spreading_factor: 10   # SF7-SF12
  bandwidth: 125000      # 125/250/500 kHz
  coding_rate: "4/5"     # "4/5", "4/6", "4/7", "4/8"
  tx_power: 20           # dBm
  # Encryption
  aes_key: "0123456789abcdef0123456789abcdef"  # 32 hex chars

database:
  path: "/var/lib/agsys/controller.db"

timing:
  sync_interval: 30      # Cloud sync interval (seconds)
  command_timeout: 10    # Valve command timeout (seconds)
  command_retries: 3     # Max retries for commands
  time_sync_interval: 3600  # Time broadcast interval (seconds)
```

## Development

### Project Structure

```
property-controller/
├── cmd/
│   ├── agsys-controller/   # Main controller binary
│   └── agsys-db/           # Database CLI tool
├── internal/
│   ├── cloud/              # WebSocket cloud client
│   ├── engine/             # Core routing engine
│   ├── lora/               # LoRa driver for RAK2245
│   ├── protocol/           # Message definitions
│   └── storage/            # SQLite database layer
├── configs/
│   ├── config.yaml         # Example configuration
│   └── agsys-controller.service  # systemd service
├── go.mod
└── README.md
```

### Building for Raspberry Pi

```bash
# Cross-compile for ARM64 (Pi 5, Pi 4 64-bit)
GOOS=linux GOARCH=arm64 go build -o bin/agsys-controller-arm64 ./cmd/agsys-controller
GOOS=linux GOARCH=arm64 go build -o bin/agsys-db-arm64 ./cmd/agsys-db

# Cross-compile for ARM (Pi 3, Pi 4 32-bit)
GOOS=linux GOARCH=arm GOARM=7 go build -o bin/agsys-controller-arm ./cmd/agsys-controller
```

## Design Decisions

This section documents the architectural decisions made during design.

### Why Go (not Python)?

| Aspect | Go | Python |
|--------|-----|--------|
| Deployment | Single binary, no runtime needed | Requires Python runtime + venv |
| Concurrency | Goroutines (excellent) | asyncio (good) |
| Memory | Lower footprint | Higher footprint |
| Long-running stability | Excellent | Good with proper async |
| LoRa library support | Limited (may need CGO) | Good (python-lora, RPi.GPIO) |

**Decision**: Go chosen for single-binary deployment, better concurrency model for handling simultaneous LoRa messages and WebSocket events, and lower resource usage on Pi.

### Why gRPC (not REST+WebSocket or MQTT)?

| Aspect | gRPC | REST+WebSocket | MQTT |
|--------|------|----------------|------|
| Connection model | Bidirectional streaming | Separate protocols | Pub/sub |
| Typing | Strong (protobuf) | JSON schema | None |
| Efficiency | Binary, compact | Text-based | Binary |
| Infrastructure | Single endpoint | Two endpoints | Requires broker |

**Decision**: gRPC chosen because:
- Single protocol for bidirectional communication
- Strong typing via Protocol Buffers
- Efficient binary protocol
- Built-in keepalive and reconnection semantics
- Both ends are Go - trivial integration

**API Definition**: See [agsys-api](https://github.com/ccroswhite/agsys-api) repository for the shared Protocol Buffer definitions.

### Why SQLite (not PostgreSQL)?

For scale of hundreds of sensors, few controllers/meters:
- Zero configuration, single file
- Handles concurrent reads well
- Easy backup (copy file)
- No separate service to manage

**Decision**: SQLite with `agsys-db` CLI tool for inspection (provides psql-like interaction).

### Why Raw LoRa (not LoRaWAN)?

LoRaWAN is designed for large-scale public networks with:
- Roaming between gateways
- Adaptive Data Rate (ADR)
- Gateway redundancy
- OTAA/ABP security

**Decision**: Raw LoRa with AES-128 encryption because:
- Private property network (single gateway)
- Simpler, lower latency
- Full control over protocol
- Matches existing device firmware using RFM95C

### Device Identification

- **AgSys is the authority** for registered devices
- Each device uses MCU's unique ID (UID) for identification
- Soil moisture sensors: UID + probe index (0-3)
- Water meters: UID with optional alias
- Valve controllers: UID for controller, address (0-63) for actuators

### Valve Control Flow

- **Immediate commands**: Property controller pushes open/close via LoRa (from cloud user action)
- **Schedules**: Valve controller pulls updates periodically from property controller
- **Acknowledgment**: Commands tracked with timeout and retry (default: 10s timeout, 3 retries)

### Data Priority

| Data Type | Direction | Priority | Latency |
|-----------|-----------|----------|---------|
| Valve open/close command | Cloud→Device | **Critical** | <5 seconds |
| Valve state confirmation | Device→Cloud | **Critical** | <5 seconds |
| Schedule updates | Cloud→Device | Medium | Minutes OK |
| Sensor readings | Device→Cloud | Low | Minutes OK |
| Water meter readings | Device→Cloud | Low | Minutes OK |

## Database Schema

### Tables

| Table | Purpose |
|-------|---------|
| `property` | Property UID, name, alias |
| `zones` | Watering zones within property |
| `devices` | All registered IoT devices |
| `valve_actuators` | Individual valve actuators per controller |
| `soil_moisture_readings` | Sensor data with sync status |
| `water_meter_readings` | Meter data with sync status |
| `valve_events` | Valve state changes |
| `schedules` | Watering schedule definitions |
| `schedule_entries` | Individual schedule time slots |
| `pending_commands` | Commands awaiting acknowledgment |
| `cloud_sync_queue` | Items queued for cloud sync |

### Key Indexes

- Readings indexed by `device_uid`, `timestamp`, and `synced_to_cloud`
- Pending commands indexed by `command_id` and `expires_at`

## Message Payloads

### Sensor Data (0x01)
```
Offset  Size  Field
0       1     Probe ID (0-3)
1       2     Moisture raw (ADC)
3       1     Moisture percent
4       2     Temperature (0.1°C)
6       2     Battery (mV)
```

### Water Meter (0x02)
```
Offset  Size  Field
0       4     Total liters
4       2     Flow rate (L/min × 10)
6       2     Battery (mV)
```

### Valve Status (0x03)
```
Offset  Size  Field
0       1     Actuator address
1       1     State (0=closed, 1=open, 2=opening, 3=closing)
2       2     Motor current (mA)
4       1     Status flags
```

### Valve Command (0x10)
```
Offset  Size  Field
0       1     Actuator address (0-63, 0xFF=all)
1       1     Command (0=close, 1=open, 2=stop)
2       2     Command ID (for tracking)
```

### Valve Acknowledgment (0x04)
```
Offset  Size  Field
0       1     Actuator address
1       2     Command ID
3       1     Result state
4       1     Success (0/1)
```

## Troubleshooting

### Controller won't start
```bash
# Check configuration
agsys-controller run --config /etc/agsys/controller.yaml

# Check logs
journalctl -u agsys-controller -f
```

### No data from devices
```bash
# Check if devices are registered
agsys-db devices

# Check recent readings
agsys-db sensor -n 10
agsys-db meter -n 10
```

### Commands not acknowledged
```bash
# Check pending commands
agsys-db pending

# Check valve events
agsys-db events -n 20
```

### Cloud sync issues
```bash
# Check unsynced data
agsys-db stats

# Look for sync errors in logs
journalctl -u agsys-controller | grep -i sync
```

## License

Copyright © AgSys. All rights reserved.
