# FreeRTOS Migration Plan for AgSys Devices

**Decision Date:** January 9, 2026  
**Status:** Approved - Pending Implementation

---

## Executive Summary

**Decision:** Migrate all AgSys IoT devices from Arduino framework to FreeRTOS using Nordic nRF5 SDK directly.

**Rationale:**
1. Water Meter has timing conflicts: 1kHz ADC sampling + LoRa TX + Display + BLE
2. Consistency across all devices reduces debugging complexity
3. No abstractions/FFI - direct SDK access preferred
4. Future-proofing for feature additions

**Devices Affected:**
- Water Meter (nRF52840) - Most complex, highest priority
- Valve Actuator (nRF52810) - Simplest, good starting point
- Valve Controller (nRF52832)
- Soil Moisture Sensor (nRF52832)

---

## Platform Choice

### nRF5 SDK + FreeRTOS (Direct)

**Why this approach:**
- Nordic provides official FreeRTOS port in nRF5 SDK
- Direct access to all peripherals (SPI, TIMER, GPIOTE, etc.)
- No hidden abstractions - you see exactly what's happening
- Same SDK used for SoftDevice (BLE) integration
- Well-documented, production-proven

**SDK Version:** nRF5 SDK 17.1.0 (latest stable, FreeRTOS 10.4.6 included)

**Development Environment Options:**
- Segger Embedded Studio (free for Nordic chips)
- GCC + Make (open source)

---

## Current State Analysis

### Water Meter (924 lines) - High Complexity

**Concurrent operations in Arduino `loop()`:**
1. ADC sampling - Hardware-triggered via callback at 1kHz
2. Flow calculation - Every 1 second
3. Trend/average updates - Every 60 seconds
4. Button handling - Continuous polling
5. Display updates - Every 100ms (LVGL timer)
6. LoRa TX/RX - Report every 60s + receive processing
7. BLE processing - Live data updates every 1s when connected
8. Pairing mode check - Continuous

**Current timing approach:** Hardware timer triggers ADC reads, but everything else is polled in `loop()` with `millis()` checks.

**Key Risk:** LoRa TX takes ~100-200ms. During this time, ADC samples could be missed or delayed. SPI bus is shared between ADC, LoRa, Display, and FRAM.

### Valve Actuator (409 lines) - Low Complexity

**Operations in Arduino `loop()`:**
1. CAN interrupt flag check - Event-driven
2. Valve state machine update - Simple state transitions
3. BLE processing - Minimal (DFU only)
4. LED updates - Simple blink patterns

**Current timing approach:** Interrupt-driven CAN, simple polling for everything else. No timing-critical operations.

---

## Water Meter Task Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        FreeRTOS Kernel                          │
├─────────────────────────────────────────────────────────────────┤
│  Priority 6 (Highest)                                           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ ADC Task                                                 │   │
│  │ - Timer-triggered at 1kHz                               │   │
│  │ - Reads ADS131M02 via SPI                               │   │
│  │ - Pushes samples to signal processing queue             │   │
│  │ - Stack: 256 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Priority 5                                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Signal Processing Task                                   │   │
│  │ - Consumes ADC samples from queue                       │   │
│  │ - Synchronous detection, flow calculation               │   │
│  │ - Updates shared flow rate variable (mutex protected)   │   │
│  │ - Stack: 512 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Priority 4                                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ LoRa Task                                                │   │
│  │ - Periodic TX (report every 60s)                        │   │
│  │ - RX polling between TX                                 │   │
│  │ - Uses SPI mutex for bus access                         │   │
│  │ - Stack: 512 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Priority 3                                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Display Task                                             │   │
│  │ - LVGL tick + timer handler                             │   │
│  │ - Updates display at 30 FPS                             │   │
│  │ - Uses SPI mutex for bus access                         │   │
│  │ - Stack: 1024 words (LVGL needs more)                   │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Priority 2                                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ BLE Task                                                 │   │
│  │ - Handles BLE events from SoftDevice                    │   │
│  │ - Updates characteristics when connected                │   │
│  │ - Stack: 256 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Priority 1 (Lowest)                                            │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ UI Task                                                  │   │
│  │ - Button polling                                        │   │
│  │ - Menu state machine                                    │   │
│  │ - Settings management                                   │   │
│  │ - Stack: 256 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Idle Task (built-in)                                           │
│  - Enters low-power mode when nothing to do                    │
└─────────────────────────────────────────────────────────────────┘
```

### Shared Resources

| Resource | Type | Used By |
|----------|------|---------|
| SPI Bus | Mutex (xSemaphore) | ADC Task, LoRa Task, Display Task, FRAM access |
| Flow Data | Mutex | Signal Task (write), Display Task (read), LoRa Task (read) |
| ADC Sample Queue | Queue (xQueue) | ADC Task → Signal Task, 1024 samples |

---

## Valve Actuator Task Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        FreeRTOS Kernel                          │
├─────────────────────────────────────────────────────────────────┤
│  Priority 4 (Highest)                                           │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ CAN Task                                                 │   │
│  │ - Interrupt-driven (notification from ISR)              │   │
│  │ - Processes commands, sends responses                   │   │
│  │ - Stack: 256 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Priority 3                                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Valve Control Task                                       │   │
│  │ - State machine (idle, opening, open, closing, closed)  │   │
│  │ - Current monitoring                                    │   │
│  │ - Timeout handling                                      │   │
│  │ - Stack: 256 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Priority 2                                                     │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ BLE Task                                                 │   │
│  │ - DFU support only                                      │   │
│  │ - Pairing mode handling                                 │   │
│  │ - Stack: 256 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  Priority 1 (Lowest)                                            │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ LED Task                                                 │   │
│  │ - Status LED patterns                                   │   │
│  │ - Stack: 128 words                                      │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Work Breakdown & Time Estimates

### Phase 0: Learning & Setup (1-2 weeks)

| Task | Description | Effort |
|------|-------------|--------|
| 0.1 | Study FreeRTOS fundamentals (tasks, queues, semaphores, mutexes) | 2-3 days |
| 0.2 | Study nRF5 SDK structure and peripheral drivers | 2-3 days |
| 0.3 | Set up development environment (Segger Embedded Studio or GCC + Make) | 1 day |
| 0.4 | Build and run a "blinky" example with FreeRTOS on nRF52840-DK | 1 day |
| 0.5 | Build example with SoftDevice + FreeRTOS (BLE + tasks) | 1-2 days |

**Deliverable:** Working dev environment, basic FreeRTOS understanding

### Phase 1: Common Library (1 week)

| Task | Description | Effort |
|------|-------------|--------|
| 1.1 | Create `agsys-rtos-common` library structure | 0.5 days |
| 1.2 | Port `agsys_crypto` (AES-128-GCM) to nRF5 SDK (use `nrf_crypto`) | 1 day |
| 1.3 | Port `agsys_protocol` (message structures, no changes needed) | 0.5 days |
| 1.4 | Create SPI bus manager with mutex | 1 day |
| 1.5 | Create FRAM driver using nRF5 SPI | 0.5 days |
| 1.6 | Port `agsys_ble` to use SoftDevice directly | 2 days |

**Deliverable:** Shared library usable by all devices

### Phase 2: Valve Actuator Port (1-2 weeks)

*Start simple to validate the approach*

| Task | Description | Effort |
|------|-------------|--------|
| 2.1 | Create project structure, Makefile/SES project | 0.5 days |
| 2.2 | Implement CAN driver using nRF5 SPI + MCP2515 | 1 day |
| 2.3 | Implement CAN Task with ISR notification | 1 day |
| 2.4 | Port H-bridge driver to nRF5 GPIO | 0.5 days |
| 2.5 | Implement Valve Control Task (state machine) | 1 day |
| 2.6 | Implement BLE Task (DFU) | 1 day |
| 2.7 | Implement LED Task | 0.5 days |
| 2.8 | Integration testing on hardware | 2 days |
| 2.9 | Compare behavior to Arduino version, fix issues | 1-2 days |

**Deliverable:** Valve Actuator running on FreeRTOS, feature-complete

### Phase 3: Water Meter Port (3-4 weeks)

*Most complex device*

| Task | Description | Effort |
|------|-------------|--------|
| 3.1 | Create project structure | 0.5 days |
| 3.2 | Port ADS131M02 driver to nRF5 SPI | 1 day |
| 3.3 | Implement hardware timer for 1kHz ADC trigger | 1 day |
| 3.4 | Implement ADC Task with timer ISR | 1 day |
| 3.5 | Port signal processing (synchronous detection) | 1 day |
| 3.6 | Implement Signal Processing Task with queue | 1 day |
| 3.7 | Port LoRa driver (RFM95) to nRF5 SPI | 1 day |
| 3.8 | Implement LoRa Task | 1 day |
| 3.9 | Port display driver (ILI9341 or similar) to nRF5 SPI | 1 day |
| 3.10 | Integrate LVGL with FreeRTOS tick | 1 day |
| 3.11 | Implement Display Task | 1 day |
| 3.12 | Port button handling to nRF5 GPIOTE | 0.5 days |
| 3.13 | Implement UI Task | 1 day |
| 3.14 | Implement BLE Task | 1 day |
| 3.15 | Integration testing - verify ADC timing | 2 days |
| 3.16 | Integration testing - verify no SPI conflicts | 2 days |
| 3.17 | Calibration verification | 1 day |
| 3.18 | Field testing | 2-3 days |

**Deliverable:** Water Meter running on FreeRTOS, timing-verified

### Phase 4: Remaining Devices (2-3 weeks)

| Task | Description | Effort |
|------|-------------|--------|
| 4.1 | Port Soil Moisture Sensor | 3-4 days |
| 4.2 | Port Valve Controller | 4-5 days |
| 4.3 | Integration testing all devices | 3-4 days |

**Deliverable:** All devices on FreeRTOS

---

## Total Effort Estimate

| Phase | Duration | Effort |
|-------|----------|--------|
| Phase 0: Learning & Setup | 1-2 weeks | Foundation |
| Phase 1: Common Library | 1 week | ~6 days |
| Phase 2: Valve Actuator | 1-2 weeks | ~9 days |
| Phase 3: Water Meter | 3-4 weeks | ~18 days |
| Phase 4: Remaining Devices | 2-3 weeks | ~12 days |
| **Total** | **8-12 weeks** | **~45 days** |

---

## Key Learning Resources

### FreeRTOS
- [Mastering the FreeRTOS Real Time Kernel](https://www.freertos.org/Documentation/RTOS_book.html) (free PDF)
- Focus on: Tasks, Queues, Semaphores/Mutexes, ISR-safe functions

### nRF5 SDK
- [Nordic DevZone](https://devzone.nordicsemi.com/)
- SDK examples in `examples/peripheral/` (SPI, TIMER, GPIOTE)
- SDK examples in `examples/ble_peripheral/` (SoftDevice integration)

### FreeRTOS + SoftDevice
- `examples/ble_peripheral/ble_app_hrs_freertos` in SDK
- Key: SoftDevice has highest priority, FreeRTOS runs below it

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| SoftDevice + FreeRTOS conflicts | Use Nordic's official integration, follow their examples exactly |
| SPI bus contention | Mutex-protected access, tested thoroughly in Phase 3 |
| ADC timing jitter | High-priority task + hardware timer, measure with scope |
| LVGL memory usage | Configure LV_MEM_SIZE appropriately, use static allocation |
| Learning curve | Start with Valve Actuator (simpler), build confidence |

---

## FreeRTOS Key Concepts (Quick Reference)

### Tasks
```c
// Create a task
xTaskCreate(
    task_function,      // Function pointer
    "TaskName",         // Debug name
    256,                // Stack size (words)
    NULL,               // Parameters
    3,                  // Priority (higher = more important)
    &task_handle        // Handle for later reference
);

// Task function signature
void task_function(void *pvParameters) {
    while (1) {
        // Do work
        vTaskDelay(pdMS_TO_TICKS(100));  // Sleep 100ms
    }
}
```

### Queues (Task-to-Task Communication)
```c
// Create queue
QueueHandle_t queue = xQueueCreate(1024, sizeof(int32_t));

// Send (from producer task or ISR)
xQueueSend(queue, &sample, portMAX_DELAY);
xQueueSendFromISR(queue, &sample, &xHigherPriorityTaskWoken);

// Receive (in consumer task)
xQueueReceive(queue, &sample, portMAX_DELAY);
```

### Mutexes (Shared Resource Protection)
```c
// Create mutex
SemaphoreHandle_t spi_mutex = xSemaphoreCreateMutex();

// Use mutex
if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Access SPI bus
    xSemaphoreGive(spi_mutex);
}
```

### Task Notifications (ISR to Task)
```c
// In ISR
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
vTaskNotifyGiveFromISR(task_handle, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

// In task
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Block until notified
```

---

## Decision Points (Before Starting)

1. **Development environment:** Segger Embedded Studio (free for Nordic) or GCC + Make?
2. **Start immediately or after current sprint?**
3. **Hardware availability:** Do you have nRF52840-DK and nRF52810-DK for development?

---

## Appendix: Arduino vs FreeRTOS Comparison

| Aspect | Arduino | FreeRTOS |
|--------|---------|----------|
| Development Speed | Fast - familiar APIs | Slower - more boilerplate |
| Code Portability | Good across Arduino boards | Excellent across FreeRTOS ports |
| Real-time Guarantees | None - cooperative | Yes - preemptive, priority-based |
| Memory Footprint | Lower baseline | ~5-10KB additional for kernel |
| BLE Stack | Adafruit Bluefruit | Nordic SDK + SoftDevice directly |
| Power Management | Basic (delay-based sleep) | Advanced (tickless idle) |
| Debugging | Serial.print | RTOS-aware debuggers, task tracing |
| Concurrent Operations | Manual state machines | Native tasks, queues, semaphores |

---

## Appendix: Power Savings Analysis

### Soil Moisture Sensor (Battery Powered)

**The key benefit:** FreeRTOS tickless idle provides ~10-15x lower sleep current.

| State | Arduino (Current) | FreeRTOS (Tickless) |
|-------|-------------------|---------------------|
| Active (reading sensors, TX) | ~5 mA | ~5 mA |
| Active duration | ~500 ms | ~500 ms |
| Sleep (2 hour interval) | ~15-50 µA | ~1.5-3 µA |

**Why Arduino sleep is inefficient:**
- `delay()` keeps SysTick running - wakes CPU every 1ms
- SoftDevice overhead not fully disabled
- No true tickless idle

**FreeRTOS tickless idle:**
- Kernel stops SysTick timer
- Calculates time until next wake
- Enters System ON sleep at 1.5 µA
- Wakes only on RTC or GPIO interrupt

### Battery Life Estimates

Assumptions: 2000 mAh battery, 2-hour wake interval, 500ms active time

| Platform | Sleep Current | Estimated Battery Life |
|----------|---------------|------------------------|
| Arduino (current) | ~30 µA | 2-3 years |
| FreeRTOS (tickless) | ~2 µA | 4-5 years |

**Limiting factors:** Self-discharge (2-3%/month), temperature, PCB leakage

### Other Devices

| Device | Power Source | FreeRTOS Power Benefit |
|--------|--------------|------------------------|
| Water Meter | Mains | Negligible (always active) |
| Valve Actuator | 24V | Moderate (sleep between commands) |
| Valve Controller | 24V | Moderate |
| **Soil Moisture** | **Battery** | **Significant (~2x battery life)** |

---

## Appendix: Memory Footprint Analysis

### Available Flash/RAM by Chip

| Chip | Flash | RAM | Used By |
|------|-------|-----|---------|
| nRF52840 | 1024 KB | 256 KB | Water Meter |
| nRF52832 | 512 KB | 64 KB | Valve Controller, Soil Moisture |
| nRF52810 | 192 KB | 24 KB | Valve Actuator |

### SoftDevice (BLE Stack) Overhead

| SoftDevice | Flash | RAM | Notes |
|------------|-------|-----|-------|
| S140 (nRF52840) | ~152 KB | ~6 KB | Full BLE 5.0 |
| S132 (nRF52832) | ~152 KB | ~6 KB | BLE 5.0 |
| S112 (nRF52810) | ~100 KB | ~4 KB | BLE peripheral only |

### FreeRTOS Kernel Overhead

| Component | Flash | RAM |
|-----------|-------|-----|
| FreeRTOS kernel | ~6-10 KB | ~1-2 KB base |
| Per task stack | - | 256-1024 words each |

### Estimated Totals by Device

| Device | Est. Total Flash | Available | Est. Total RAM | Available | Fits? |
|--------|------------------|-----------|----------------|-----------|-------|
| Water Meter | ~412 KB | 1024 KB | ~72 KB | 256 KB | ✅ Yes |
| Valve Controller | ~100 KB | 512 KB | ~20 KB | 64 KB | ✅ Yes |
| Soil Moisture | ~80 KB | 512 KB | ~16 KB | 64 KB | ✅ Yes |
| Valve Actuator | ~158 KB | 192 KB | ~14 KB | 24 KB | ✅ Yes |

**All devices fit comfortably with 20%+ headroom.**

---

## Appendix: Driver Porting Requirements

### Included in Nordic nRF5 SDK (No Work Needed)

- SPI Master (`nrfx_spim`) - DMA-capable
- GPIO (`nrfx_gpiote`) - Interrupt support
- Timer (`nrfx_timer`) - Hardware timers
- RTC (`nrfx_rtc`) - Low-power wake
- BLE (SoftDevice) - Full stack + DFU
- Crypto (`nrf_crypto`) - AES, SHA, RNG hardware accelerated
- Power Management (`nrf_pwr_mgmt`) - Tickless idle

### External Chip Drivers (Existing Libraries Found)

All external chip drivers have existing platform-agnostic C libraries available.
Only need to implement HAL functions (SPI transfer, GPIO, delays).

| Chip | Original Est. | With Library | Source | License |
|------|---------------|--------------|--------|---------|
| RFM95 (LoRa) | 2-3 days | **1-1.5 days** | [Semtech LoRaMac-node](https://github.com/Lora-net/LoRaMac-node) | BSD-3 |
| ADS131M02 (ADC) | 1 day | **0.5 day** | [Pablo-Jean/ADS131M0x](https://github.com/Pablo-Jean/ADS131M0x) | Permissive |
| MCP2515 (CAN) | 1 day | **0.5 day** | [mortenmj/FreeRTOS MCP2515](https://github.com/mortenmj/FreeRTOS/tree/master/FreeRTOS/Control/can) | BSD-3 |
| FRAM (MB85RS1MT) | 0.5 day | **0.25 day** | Custom driver (SPI FRAM) | N/A |
| Display (ST7789) | 1-2 days | 1-2 days | LVGL has drivers, need SPI backend | MIT |
| LVGL | 0.5 day | 0.5 day | Native FreeRTOS support built-in | MIT |

**Total driver porting: ~3-4 days** (reduced from ~6 days)

### HAL Implementation Pattern

Each library requires implementing a small HAL interface. Example for ADS131M0x:

```c
// Implement these function pointers for nRF5 SDK
SPITransfer(tx, rx, len)  → nrfx_spim_xfer()
CSPin(state)              → nrf_gpio_pin_write()
SYNCPin(state)            → nrf_gpio_pin_write()
DelayMs(ms)               → vTaskDelay(pdMS_TO_TICKS(ms))
Lock() / Unlock()         → xSemaphoreTake() / xSemaphoreGive()
```

---

## Appendix: Licensing

### FreeRTOS
- **License:** MIT License (since 2017)
- **Commercial use:** ✅ Allowed, no restrictions
- **Royalties:** None
- **Attribution:** Keep copyright notice in source files
- **Source disclosure:** Not required

### Nordic nRF5 SDK
- **License:** Nordic 5-Clause BSD-style
- **Commercial use:** ✅ Allowed
- **Royalties:** None
- **Requirement:** Only use with Nordic chips

**No licensing concerns for commercial AgSys deployment.**

---

*Document created: January 9, 2026*
*Last updated: January 9, 2026*
