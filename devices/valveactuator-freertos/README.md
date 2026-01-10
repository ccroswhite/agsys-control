# Valve Actuator - FreeRTOS

FreeRTOS port of the Valve Actuator firmware for nRF52810.

## Hardware

- **MCU:** Nordic nRF52810 (192KB Flash, 24KB RAM)
- **SoftDevice:** S112 (BLE peripheral only)
- **CAN:** MCP2515 via SPI
- **Motor:** Discrete H-bridge with current sensing
- **Storage:** FM25V02 FRAM

## Tasks

| Task | Priority | Stack | Description |
|------|----------|-------|-------------|
| CAN | 4 | 256 | CAN bus message handling |
| Valve | 3 | 256 | Valve state machine, H-bridge control |
| BLE | 2 | 256 | SoftDevice event handling (DFU) |
| LED | 1 | 128 | Status LED patterns |

## Building

```bash
# Build
make

# Flash SoftDevice (first time only)
make flash_softdevice

# Flash application
make flash

# Clean
make clean

# Erase chip
make erase
```

## Memory Map

| Region | Start | Size | Usage |
|--------|-------|------|-------|
| Flash (SD) | 0x00000000 | 96KB | S112 SoftDevice |
| Flash (App) | 0x00019000 | 92KB | Application |
| RAM (SD) | 0x20000000 | 5.6KB | SoftDevice |
| RAM (App) | 0x20001668 | 18.4KB | Application + FreeRTOS |

## Pin Assignments

See `config/agsys_config.h` for full pin mapping.

## Dependencies

- nRF5 SDK 17.1.0
- ARM GCC toolchain
- Nordic command line tools (nrfjprog)

## Compared to Arduino Version

| Aspect | Arduino | FreeRTOS |
|--------|---------|----------|
| Concurrency | Polling in loop() | Preemptive tasks |
| CAN handling | Interrupt flag + poll | Task notification from ISR |
| Power | Basic delay | Tickless idle |
| Debugging | Serial.print | RTT + RTOS-aware |
