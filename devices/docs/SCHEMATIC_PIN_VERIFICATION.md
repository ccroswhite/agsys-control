# Schematic Pin Verification Report

Generated: January 11, 2026

This document compares the pin assignments in each device's `board_config.h` (source of truth) against the Eagle schematic files.

---

## 1. Soil Moisture Sensor (soilmoisture-freertos)

### board_config.h Pins:
| Signal | Pin | Description |
|--------|-----|-------------|
| LED_STATUS_PIN | 17 | Green status LED |
| SPI_SCK_PIN | 25 | SPI Clock |
| SPI_MOSI_PIN | 24 | SPI MOSI |
| SPI_MISO_PIN | 23 | SPI MISO |
| SPI_CS_LORA_PIN | 27 | RFM95C LoRa CS |
| SPI_CS_FRAM_PIN | 11 | FM25V02 FRAM CS |
| SPI_CS_FLASH_PIN | 12 | W25Q16 Flash CS |
| LORA_RESET_PIN | 30 | LoRa Reset |
| LORA_DIO0_PIN | 31 | LoRa Interrupt |
| PROBE_POWER_PIN | 16 | P-FET gate |
| PROBE_1_FREQ_PIN | 3 | Probe 1 (1ft) |
| PROBE_2_FREQ_PIN | 4 | Probe 2 (3ft) |
| PROBE_3_FREQ_PIN | 5 | Probe 3 (5ft) |
| PROBE_4_FREQ_PIN | 28 | Probe 4 (7ft) |
| PAIRING_BUTTON_PIN | 7 | BLE pairing button |

### Eagle Schematic Pins (soilmoisture_schematic.sch):
| Signal | Pin in Schematic | Status |
|--------|------------------|--------|
| SPI_SCK | P0.23 | ❌ **MISMATCH** (should be P0.25) |
| SPI_MOSI | P0.24 | ✅ Match |
| SPI_MISO | P0.25 | ❌ **MISMATCH** (should be P0.23) |
| LORA_CS | P0.27 | ✅ Match |
| LORA_RST | P0.30 | ✅ Match |
| LORA_DIO0 | P0.31 | ✅ Match |
| NVRAM_CS (FRAM) | P0.11 | ✅ Match |
| FLASH_CS | P0.12 | ✅ Match |
| PROBE_PWR | P0.16 | ✅ Match |
| PROBE1_FREQ | P0.03 | ✅ Match |
| PROBE2_FREQ | P0.04 | ✅ Match |
| PROBE3_FREQ | P0.05 | ✅ Match |
| PROBE4_FREQ | P0.28 | ✅ Match |
| LED_STATUS | P0.17 | ✅ Match |
| PAIRING_BTN | P0.07 | ✅ Match |

### Issues Found:
1. **SPI_SCK and SPI_MISO are swapped** in the schematic
   - Schematic: SCK=P0.23, MISO=P0.25
   - board_config.h: SCK=P0.25, MISO=P0.23

---

## 2. Valve Controller (valvecontrol-freertos)

### board_config.h Pins:
| Signal | Pin | Description |
|--------|-----|-------------|
| SPI_SCK_PIN | 26 | SPI Clock |
| SPI_MOSI_PIN | 27 | SPI MOSI |
| SPI_MISO_PIN | 28 | SPI MISO |
| SPI_CS_CAN_PIN | 11 | MCP2515 CAN CS |
| SPI_CS_LORA_PIN | 12 | RFM95C LoRa CS |
| SPI_CS_FRAM_PIN | 13 | FM25V02 FRAM CS |
| SPI_CS_FLASH_PIN | 29 | W25Q16 Flash CS |
| CAN_INT_PIN | 14 | MCP2515 Interrupt |
| LORA_DIO0_PIN | 15 | LoRa Interrupt |
| LORA_RESET_PIN | 16 | LoRa Reset |
| I2C_SDA_PIN | 24 | RV-3028 RTC SDA |
| I2C_SCL_PIN | 25 | RV-3028 RTC SCL |
| POWER_FAIL_PIN | 17 | 24V power fail |
| LED_3V3_PIN | 18 | 3.3V indicator |
| LED_24V_PIN | 19 | 24V indicator |
| LED_STATUS_PIN | 20 | Status LED |
| PAIRING_BUTTON_PIN | 30 | BLE pairing button |

### Eagle Schematic Pins (valvecontrol_schematic.sch):
| Signal | Pin in Schematic | Status |
|--------|------------------|--------|
| SPI_SCK | P0.14 | ❌ **MISMATCH** (should be P0.26) |
| SPI_MOSI | P0.12 | ❌ **MISMATCH** (should be P0.27) |
| SPI_MISO | P0.13 | ❌ **MISMATCH** (should be P0.28) |
| LORA_CS | P0.27 | ❌ **MISMATCH** (should be P0.12) |
| CAN_CS | P0.11 | ✅ Match |
| FRAM_CS | P0.15 | ❌ **MISMATCH** (should be P0.13) |
| FLASH_CS | P0.16 | ❌ **MISMATCH** (should be P0.29) |
| LORA_RST | P0.30 | ❌ **MISMATCH** (should be P0.16) |
| LORA_DIO0 | P0.31 | ❌ **MISMATCH** (should be P0.15) |
| CAN_INT | P0.08 | ❌ **MISMATCH** (should be P0.14) |
| I2C_SDA | P0.25 | ❌ **MISMATCH** (should be P0.24) |
| I2C_SCL | P0.26 | ❌ **MISMATCH** (should be P0.25) |
| LED_3V3 | P0.17 | ❌ **MISMATCH** (should be P0.18) |
| LED_24V | P0.19 | ✅ Match |
| LED_STATUS | P0.20 | ✅ Match |
| POWER_FAIL | P0.07 | ❌ **MISMATCH** (should be P0.17) |
| PAIRING_BTN | P0.06 | ❌ **MISMATCH** (should be P0.30) |

### Issues Found:
**MAJOR DISCREPANCY** - The schematic appears to be from an older design. Almost all pins are different from board_config.h. The schematic needs to be completely updated.

---

## 3. Valve Actuator (valveactuator-freertos)

### board_config.h Pins:
| Signal | Pin | Description |
|--------|-----|-------------|
| LED_POWER_PIN | 25 | 3.3V indicator |
| LED_24V_PIN | 26 | 24V indicator |
| LED_STATUS_PIN | 27 | Status LED |
| LED_VALVE_OPEN_PIN | 28 | Valve open indicator |
| SPI_SCK_PIN | 14 | SPI Clock |
| SPI_MOSI_PIN | 12 | SPI MOSI |
| SPI_MISO_PIN | 13 | SPI MISO |
| SPI_CS_CAN_PIN | 11 | MCP2515 CAN CS |
| SPI_CS_FRAM_PIN | 7 | FM25V02 FRAM CS |
| SPI_CS_FLASH_PIN | 29 | W25Q16 Flash CS |
| CAN_INT_PIN | 8 | MCP2515 Interrupt |
| HBRIDGE_A_PIN | 3 | Motor direction A |
| HBRIDGE_B_PIN | 4 | Motor direction B |
| HBRIDGE_EN_A_PIN | 5 | Enable A (PWM) |
| HBRIDGE_EN_B_PIN | 6 | Enable B (PWM) |
| CURRENT_SENSE_PIN | 2 | Current sense ADC |
| LIMIT_OPEN_PIN | 9 | Open limit switch |
| LIMIT_CLOSED_PIN | 10 | Closed limit switch |
| DIP_1_PIN | 15 | Address bit 1 |
| DIP_2_PIN | 16 | Address bit 2 |
| DIP_3_PIN | 17 | Address bit 3 |
| DIP_4_PIN | 18 | Address bit 4 |
| DIP_5_PIN | 19 | Address bit 5 |
| DIP_6_PIN | 20 | Address bit 6 |
| DIP_TERM_PIN | 24 | CAN termination |
| PAIRING_BUTTON_PIN | 30 | BLE pairing button |

### Eagle Schematic Pins (valveactuator_schematic.sch):
| Signal | Pin in Schematic | Status |
|--------|------------------|--------|
| SPI_SCK | P0.14 | ✅ Match |
| SPI_MOSI | P0.12 | ✅ Match |
| SPI_MISO | P0.13 | ✅ Match |
| CAN_CS | P0.11 | ✅ Match |
| CAN_INT | P0.08 | ✅ Match |
| HBRIDGE_A | P0.03 | ✅ Match |
| HBRIDGE_B | P0.04 | ✅ Match |
| HBRIDGE_EN_A | P0.05 | ✅ Match |
| HBRIDGE_EN_B | P0.06 | ✅ Match |
| CURRENT_SENSE | P0.02/AIN0 | ✅ Match |
| LIMIT_OPEN | P0.09 | ✅ Match |
| LIMIT_CLOSED | P0.10 | ✅ Match |
| DIP_1 | P0.15 | ✅ Match |
| DIP_2 | P0.16 | ✅ Match |
| DIP_3 | P0.17 | ✅ Match |
| DIP_4 | P0.18 | ✅ Match |
| DIP_5 | P0.19 | ✅ Match |
| DIP_6 | P0.20 | ✅ Match |
| DIP_10 (TERM) | P0.24 | ✅ Match |
| LED_3V3 | P0.25 | ✅ Match |
| LED_24V | P0.26 | ✅ Match |
| LED_STATUS | P0.27 | ✅ Match |
| LED_VALVE_OPEN | P0.28 | ✅ Match |

### Issues Found:
✅ **ALL PINS MATCH** - Valve actuator schematic is correct.

**Missing from schematic:**
- FRAM CS (P0.07) - not shown in schematic
- Flash CS (P0.29) - not shown in schematic
- Pairing button (P0.30) - not shown in schematic

---

## 4. Water Meter (watermeter-freertos)

### board_config.h Pins:
| Signal | Pin | Description |
|--------|-----|-------------|
| SPI0 (ADC) SCK | P0.25 | ADC SPI Clock |
| SPI0 (ADC) MOSI | P0.24 | ADC SPI MOSI |
| SPI0 (ADC) MISO | P0.23 | ADC SPI MISO |
| SPI_CS_ADC_PIN | P0.22 | ADS131M02 CS |
| SPI1 (Display) SCK | P0.19 | Display SPI Clock |
| SPI1 (Display) MOSI | P0.18 | Display SPI MOSI |
| SPI_CS_DISPLAY_PIN | P0.17 | ST7789 CS |
| DISPLAY_DC_PIN | P0.30 | Display D/C |
| DISPLAY_RESET_PIN | P0.15 | Display Reset |
| DISPLAY_BACKLIGHT_PIN | P0.14 | Backlight PWM |
| SPI2 (LoRa) SCK | P0.13 | LoRa SPI Clock |
| SPI2 (LoRa) MOSI | P0.12 | LoRa SPI MOSI |
| SPI2 (LoRa) MISO | P0.11 | LoRa SPI MISO |
| SPI_CS_LORA_PIN | P0.10 | RFM95C CS |
| LORA_DIO0_PIN | P0.08 | LoRa Interrupt |
| LORA_RESET_PIN | P0.09 | LoRa Reset |
| SPI3 (FRAM/Flash) SCK | P0.29 | Storage SPI Clock |
| SPI3 (FRAM/Flash) MOSI | P0.06 | Storage SPI MOSI |
| SPI3 (FRAM/Flash) MISO | P0.05 | Storage SPI MISO |
| SPI_CS_FRAM_PIN | P0.04 | FM25V02 CS |
| SPI_CS_FLASH_PIN | P0.03 | W25Q16 CS |
| ADC_DRDY_PIN | P0.31 | ADC Data Ready |
| ADC_SYNC_PIN | P0.20 | ADC Sync/Reset |
| COIL_GATE_PIN | P1.00 | Coil driver PWM |
| BUTTON_UP_PIN | P1.02 | Up button |
| BUTTON_DOWN_PIN | P1.03 | Down button |
| BUTTON_LEFT_PIN | P1.04 | Left button |
| BUTTON_RIGHT_PIN | P1.05 | Right button |
| BUTTON_SELECT_PIN | P1.06 | Select button |
| TIER_ID_PIN | P1.01 | Tier detection ADC |
| LED_BLE_PIN | P1.07 | BLE status LED |
| LED_LORA_PIN | P1.08 | LoRa status LED |

### Eagle Schematic (magmeter_main.sch):
The magmeter schematic shows the analog front-end (ADC, THS4551, ADA4522) but does **NOT** include:
- MCU pin assignments
- LoRa module connections
- Display connections
- Button connections
- FRAM/Flash connections

### Issues Found:
⚠️ **INCOMPLETE SCHEMATIC** - The magmeter_main.sch only shows the analog section. A complete schematic with MCU pin assignments is needed.

**Missing from schematic:**
- nRF52840 MCU with all pin assignments
- Display connector (FPC 40-pin)
- LoRa module (RFM95C)
- FRAM (FM25V02)
- Flash (W25Q16)
- Buttons (5x)
- LEDs (2x)

---

## Summary

| Device | Status | Action Required |
|--------|--------|-----------------|
| Soil Moisture | ⚠️ Minor issues | Fix SPI_SCK/MISO swap |
| Valve Controller | ❌ Major issues | Complete schematic redesign needed |
| Valve Actuator | ✅ Good | Add missing FRAM/Flash/Button |
| Water Meter | ⚠️ Incomplete | Add MCU and peripheral connections |

## Recommended Actions

1. **Soil Moisture Sensor**: Swap P0.23 and P0.25 labels in schematic (SCK↔MISO)

2. **Valve Controller**: The schematic is from an older design iteration. Need to:
   - Update all SPI pins (SCK=26, MOSI=27, MISO=28)
   - Update all chip select pins
   - Update LoRa pins (DIO0=15, RST=16)
   - Update CAN_INT (14)
   - Update I2C pins (SDA=24, SCL=25)
   - Update LED pins
   - Update button pin (30)

3. **Valve Actuator**: Add missing components:
   - FRAM CS on P0.07
   - Flash CS on P0.29
   - Pairing button on P0.30

4. **Water Meter**: Create complete schematic sheet with:
   - nRF52840 MCU
   - All 4 SPI buses with correct pin assignments
   - Display FPC connector (Hirose FH12S-40S-0.5SH(55))
   - 5 navigation buttons
   - Status LEDs
