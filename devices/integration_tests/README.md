# Integration Tests

This directory contains integration tests that verify communication between multiple devices.

## Test Categories

### 1. Valve Actuator ↔ Valve Controller (CAN Bus)

Tests the CAN bus communication between the valve controller and valve actuators.

**Requirements:**
- 1x Valve Controller board
- 1x or more Valve Actuator boards
- CAN bus wiring with proper termination (120Ω at each end)
- SWD debugger for each device

**Test Cases:**
- Discovery broadcast and UID response
- Valve open/close commands and acknowledgments
- Status query and response
- Emergency close all
- Timeout and error handling

### 2. Valve Controller ↔ Property Controller (LoRa)

Tests the LoRa communication between the valve controller and property controller.

**Requirements:**
- 1x Valve Controller board
- 1x Property Controller (Raspberry Pi with LoRa HAT)
- LoRa antennas on both devices
- Devices within LoRa range

**Test Cases:**
- Valve command from property controller
- Status report from valve controller
- Schedule update
- Discovery trigger and response
- Time synchronization

## Running Tests

### Hardware Setup

1. Connect devices according to test requirements
2. Flash test firmware to each device
3. Connect serial monitors for debug output

### Test Execution

Integration tests are run manually by observing serial output from both devices.

```bash
# Flash valve controller with integration test firmware
cd ../valvecontrol
pio run -e test_embedded -t upload

# Flash valve actuator with integration test firmware  
cd ../valveactuator
pio run -e test_embedded -t upload

# Monitor both serial ports
# Terminal 1:
pio device monitor -e test_embedded -p /dev/ttyUSB0

# Terminal 2:
pio device monitor -e test_embedded -p /dev/ttyUSB1
```

## Test Results

Test results are logged to serial output with pass/fail status.

Expected output format:
```
[INTEGRATION] Test: discovery_broadcast
[INTEGRATION] Sending discovery...
[INTEGRATION] Received UID response from address 5
[INTEGRATION] PASS: discovery_broadcast

[INTEGRATION] Test: valve_open_command
[INTEGRATION] Sending open command to address 5...
[INTEGRATION] Received status: OPENING
[INTEGRATION] Received status: OPEN
[INTEGRATION] PASS: valve_open_command
```

## Adding New Tests

1. Create test firmware in appropriate device directory
2. Add test case documentation to this README
3. Update test matrix below

## Test Matrix

| Test | Controller | Actuator | Property Ctrl | Status |
|------|------------|----------|---------------|--------|
| CAN Discovery | ✓ | ✓ | | Pending |
| CAN Valve Commands | ✓ | ✓ | | Pending |
| CAN Status Query | ✓ | ✓ | | Pending |
| LoRa Valve Command | ✓ | | ✓ | Pending |
| LoRa Status Report | ✓ | | ✓ | Pending |
| LoRa Discovery | ✓ | | ✓ | Pending |
| End-to-End Open | ✓ | ✓ | ✓ | Pending |
