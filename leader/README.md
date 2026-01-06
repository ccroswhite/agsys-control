# AgSys Leader

Leader application for the agricultural IoT system. A customer property can have **multiple leaders**, each managing a region of LoRa-connected devices. Leaders act as the local gateway between field devices and the cloud control interface.

## Leader Responsibilities

1. **Sensor Data Collection & Forwarding**
   - Receive LoRa sensor data from soil moisture sensors, water meters, etc.
   - Forward all readings to the cloud data collector
   - Local caching when cloud connectivity is unavailable

2. **Firmware Updates (OTA)**
   - Receive firmware update instructions from cloud
   - Distribute new firmware images to IoT devices via LoRa
   - Target updates by device type (soil moisture, valve control, water meter)
   - Track update progress and report status to cloud

3. **Watering Schedule Management**
   - Store watering schedules received from cloud
   - Send open/close commands to valve controllers at scheduled times
   - Receive valve state confirmations (open/closed)
   - Handle schedule changes and emergency shutoffs

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         CLOUD                                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ Data Store  │  │  Scheduler  │  │ OTA Manager │             │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘             │
└─────────┼────────────────┼────────────────┼─────────────────────┘
          │                │                │
          └────────────────┼────────────────┘
                           │ HTTPS/MQTT
          ┌────────────────┼────────────────┐
          │                │                │
     ┌────▼────┐      ┌────▼────┐      ┌────▼────┐
     │ Leader  │      │ Leader  │      │ Leader  │
     │ (Vineyard A)   │ (Vineyard B)   │ (Orchard)│
     └────┬────┘      └────┬────┘      └────┬────┘
          │ LoRa           │ LoRa           │ LoRa
     ┌────┴────┐      ┌────┴────┐      ┌────┴────┐
     │ Sensors │      │ Sensors │      │ Sensors │
     │ Valves  │      │ Valves  │      │ Valves  │
     │ Meters  │      │ Meters  │      │ Meters  │
     └─────────┘      └─────────┘      └─────────┘
```

## Features

- **LoRa Gateway**: Receives sensor data, sends commands to field devices
- **Cloud Sync**: Forwards data to cloud, receives schedules and OTA instructions
- **Device Management**: Tracks all registered devices and their status
- **Local Storage**: SQLite database for caching and offline operation
- **Watering Schedules**: Executes irrigation schedules from cloud
- **OTA Distribution**: Pushes firmware updates to devices via LoRa
- **REST API**: Local HTTP endpoints for monitoring and control
- **CLI**: Command-line interface for operations

## Hardware Requirements

- Raspberry Pi 3/4/5
- RFM95C LoRa module connected via SPI
- GPIO connections:
  - SPI0 (CE0) for chip select
  - GPIO 25 for reset
  - GPIO 24 for DIO0 (interrupt)

## Installation

```bash
cd leader

# Create virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt
```

## Usage

### Run Controller (Foreground)

```bash
python src/cli.py run
```

### List Devices

```bash
python src/cli.py devices
```

### View Sensor Data

```bash
python src/cli.py data <device-uuid> --limit 50
```

### Start OTA Update

```bash
# Update all soil moisture sensors to v1.2.0
python src/cli.py ota-start /path/to/firmware.bin 1.2.0 --device-type 1

# Update all devices
python src/cli.py ota-start /path/to/firmware.bin 1.2.0
```

### Check OTA Status

```bash
python src/cli.py ota-status
```

### Run REST API Server

```bash
python src/api.py
```

## REST API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/health` | GET | Health check |
| `/api/devices` | GET | List all devices |
| `/api/devices/<uuid>/data` | GET | Get sensor data for device |
| `/api/ota/start` | POST | Start OTA update |
| `/api/ota/stop` | POST | Stop OTA update |
| `/api/ota/progress` | GET | Get OTA progress |
| `/api/ota/devices` | GET | Get per-device OTA status |

### Start OTA via API

```bash
curl -X POST http://localhost:5000/api/ota/start \
  -H "Content-Type: application/json" \
  -d '{
    "firmware_path": "/path/to/firmware.bin",
    "version": [1, 2, 0],
    "device_type": 1
  }'
```

## OTA Update Process

```
Controller                              Devices
    │                                      │
    │──── OTA_ANNOUNCE (broadcast) ───────►│
    │     (version, size, CRC)             │
    │                                      │ (stagger 0-30 min)
    │◄──── OTA_REQUEST ────────────────────│
    │                                      │
    │──── OTA_CHUNK (0) ──────────────────►│
    │◄──── OTA_CHUNK_ACK (0) ──────────────│
    │                                      │
    │──── OTA_CHUNK (1) ──────────────────►│
    │◄──── OTA_CHUNK_ACK (1) ──────────────│
    │          ... repeat ...              │
    │                                      │
    │◄──── OTA_COMPLETE ───────────────────│
    │                                      │ (reboot)
```

### Fleet Update Timing

| Fleet Size | Approx. Time |
|------------|--------------|
| 10 devices | ~1-2 hours |
| 100 devices | ~12-24 hours |
| 500 devices | ~2-3 days |

Updates are staggered to prevent network congestion. Each device calculates a random delay (0-30 minutes) based on its UUID before requesting the update.

## Directory Structure

```
leader/
├── src/
│   ├── __init__.py
│   ├── api.py           - REST API server
│   ├── cli.py           - Command-line interface
│   ├── controller.py    - Main controller logic
│   ├── lora_driver.py   - LoRa hardware driver
│   ├── ota_manager.py   - OTA update manager
│   └── protocol.py      - Communication protocol
├── requirements.txt     - Python dependencies
└── README.md           - This file
```

## Database Schema

### devices
| Column | Type | Description |
|--------|------|-------------|
| uuid | TEXT | Device UUID (primary key) |
| device_type | INTEGER | Device type ID |
| first_seen | TEXT | First registration timestamp |
| last_seen | TEXT | Last communication timestamp |
| firmware_version | TEXT | Current firmware version |
| battery_mv | INTEGER | Last reported battery voltage |
| rssi | INTEGER | Last received signal strength |

### sensor_data
| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER | Auto-increment ID |
| device_uuid | TEXT | Foreign key to devices |
| timestamp | TEXT | Reading timestamp |
| moisture_raw | INTEGER | Raw ADC value |
| moisture_percent | INTEGER | Calculated moisture % |
| battery_mv | INTEGER | Battery voltage |
| temperature | INTEGER | Temperature (0.1°C units) |
| rssi | INTEGER | Signal strength |

### ota_history
| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER | Auto-increment ID |
| announce_id | INTEGER | OTA session ID |
| firmware_path | TEXT | Firmware file path |
| version | TEXT | Firmware version |
| start_time | TEXT | Session start time |
| end_time | TEXT | Session end time |
| devices_success | INTEGER | Successful updates |
| devices_failed | INTEGER | Failed updates |

## Configuration

LoRa parameters are configured in `controller.py`:

```python
self.lora = LoRaDriver(
    frequency=915_000_000,      # 915 MHz (US ISM)
    spreading_factor=10,        # SF10
    bandwidth=125_000,          # 125 kHz
    coding_rate=5,              # 4/5
    tx_power=20,                # +20 dBm
    sync_word=0x34              # Private network
)
```

These must match the device firmware settings.

## Running as a Service

Create `/etc/systemd/system/agsys-controller.service`:

```ini
[Unit]
Description=AgSys Controller
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/agsys-control/leader
ExecStart=/home/pi/agsys-control/leader/venv/bin/python src/cli.py run
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl enable agsys-controller
sudo systemctl start agsys-controller
```
