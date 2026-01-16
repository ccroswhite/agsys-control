# Valve Actuator - Ball Valve

FreeRTOS firmware for the Ball Valve Actuator.

## Hardware

- **MCU:** Nordic nRF52832-QFAA (512KB Flash, 64KB RAM)
- **SoftDevice:** S132 v7.2.0 (BLE central + peripheral)
- **CAN:** MCP2515 via SPI
- **Motor:** Discrete H-bridge with current sensing
- **Storage:** MB85RS1MT FRAM (128KB)

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
| Flash (SD) | 0x00000000 | 152KB | S132 SoftDevice |
| Flash (App) | 0x00026000 | 264KB | Application |
| Flash (BL) | 0x0006A000 | 32KB | Bootloader |
| RAM (SD) | 0x20000000 | 22KB | SoftDevice |
| RAM (App) | 0x20005968 | 42KB | Application + FreeRTOS |

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
