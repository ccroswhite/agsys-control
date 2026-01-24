# TMP102 Digital Temperature Sensor Driver

A complete, platform-agnostic driver for the Texas Instruments TMP102 I2C temperature sensor.

## Features

- **Temperature Reading**: Celsius, Fahrenheit, or raw values
- **Resolution Modes**: 12-bit (normal) or 13-bit (extended)
- **Conversion Rates**: 0.25Hz, 1Hz, 4Hz, or 8Hz
- **Low Power**: Shutdown mode (<0.5µA) with one-shot conversion
- **Alert Output**: Configurable thresholds, polarity, and fault queue
- **Thermostat Modes**: Comparator or interrupt mode
- **Platform Agnostic**: Works with any I2C implementation

## Quick Start

### 1. Implement I2C Interface

```c
#include "tmp102.h"

/* Your platform-specific I2C read function */
bool my_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len, void *user_data)
{
    /* Implement using your platform's I2C driver */
    return true;
}

/* Your platform-specific I2C write function */
bool my_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len, void *user_data)
{
    /* Implement using your platform's I2C driver */
    return true;
}
```

### 2. Initialize the Driver

```c
/* Set up I2C interface */
tmp102_i2c_t i2c = {
    .read = my_i2c_read,
    .write = my_i2c_write,
    .user_data = NULL  /* Optional context pointer */
};

/* Configure device */
tmp102_config_t config = TMP102_CONFIG_DEFAULT(TMP102_ADDR_GND);

/* Initialize */
tmp102_ctx_t sensor;
if (!tmp102_init(&sensor, &i2c, &config)) {
    /* Handle error */
}
```

### 3. Read Temperature

```c
float temp_c;
if (tmp102_read_temp_c(&sensor, &temp_c)) {
    printf("Temperature: %.2f°C\n", temp_c);
}
```

## I2C Addresses

The TMP102 address is set by the ADD0 pin:

| ADD0 Connection | I2C Address | Macro |
|-----------------|-------------|-------|
| GND | 0x48 | `TMP102_ADDR_GND` |
| VCC | 0x49 | `TMP102_ADDR_VCC` |
| SDA | 0x4A | `TMP102_ADDR_SDA` |
| SCL | 0x4B | `TMP102_ADDR_SCL` |

## API Reference

### Initialization

```c
/* Initialize device with configuration */
bool tmp102_init(tmp102_ctx_t *ctx, const tmp102_i2c_t *i2c, const tmp102_config_t *config);

/* Check if device is present on I2C bus */
bool tmp102_is_present(const tmp102_i2c_t *i2c, uint8_t addr);

/* Reset to default configuration */
bool tmp102_reset(tmp102_ctx_t *ctx);
```

### Temperature Reading

```c
/* Read temperature in Celsius */
bool tmp102_read_temp_c(tmp102_ctx_t *ctx, float *temp_c);

/* Read temperature in Fahrenheit */
bool tmp102_read_temp_f(tmp102_ctx_t *ctx, float *temp_f);

/* Read raw 12/13-bit value */
bool tmp102_read_raw(tmp102_ctx_t *ctx, int16_t *raw);
```

### Configuration

```c
/* Set conversion rate */
bool tmp102_set_rate(tmp102_ctx_t *ctx, tmp102_rate_t rate);

/* Enable 13-bit extended mode (-55°C to +150°C) */
bool tmp102_set_extended_mode(tmp102_ctx_t *ctx, bool enable);

/* Enter/exit shutdown mode */
bool tmp102_set_shutdown(tmp102_ctx_t *ctx, bool shutdown);

/* Trigger one-shot conversion (in shutdown mode) */
bool tmp102_one_shot(tmp102_ctx_t *ctx);

/* Check if one-shot conversion is complete */
bool tmp102_conversion_ready(tmp102_ctx_t *ctx, bool *ready);
```

### Alert Configuration

```c
/* Set temperature thresholds */
bool tmp102_set_alert_thresholds(tmp102_ctx_t *ctx, float t_low, float t_high);

/* Get current thresholds */
bool tmp102_get_alert_thresholds(tmp102_ctx_t *ctx, float *t_low, float *t_high);

/* Set alert pin polarity */
bool tmp102_set_alert_polarity(tmp102_ctx_t *ctx, tmp102_alert_polarity_t polarity);

/* Set thermostat mode (comparator or interrupt) */
bool tmp102_set_thermostat_mode(tmp102_ctx_t *ctx, tmp102_thermostat_mode_t mode);

/* Set fault queue (consecutive faults before alert) */
bool tmp102_set_fault_queue(tmp102_ctx_t *ctx, tmp102_faults_t faults);

/* Read alert status */
bool tmp102_read_alert_status(tmp102_ctx_t *ctx, bool *alert);
```

## Low Power Operation

For battery-powered applications, use shutdown mode with one-shot conversions:

```c
/* Enter shutdown mode */
tmp102_set_shutdown(&sensor, true);

/* When you need a reading: */
tmp102_one_shot(&sensor);

/* Wait for conversion (26ms typical) */
delay_ms(30);

/* Check if ready (optional) */
bool ready;
tmp102_conversion_ready(&sensor, &ready);

/* Read temperature */
float temp;
tmp102_read_temp_c(&sensor, &temp);
```

Power consumption:
- Active mode: 10µA typical
- Shutdown mode: 0.5µA typical

## Extended Mode

Extended mode provides 13-bit resolution and extends the temperature range:

| Mode | Resolution | Range |
|------|------------|-------|
| Normal (12-bit) | 0.0625°C | -55°C to +128°C |
| Extended (13-bit) | 0.0625°C | -55°C to +150°C |

```c
/* Enable extended mode */
tmp102_set_extended_mode(&sensor, true);
```

## Alert/Thermostat Functionality

The TMP102 has an ALERT pin that can be used for temperature monitoring:

### Comparator Mode (Default)
- ALERT asserts when temperature exceeds T_HIGH
- ALERT deasserts when temperature falls below T_LOW
- Useful for fan control, thermal shutdown

### Interrupt Mode
- ALERT asserts when temperature crosses either threshold
- ALERT is cleared by reading the temperature register
- Useful for interrupt-driven temperature monitoring

```c
/* Configure alert thresholds */
tmp102_set_alert_thresholds(&sensor, 25.0f, 30.0f);

/* Set to interrupt mode with active-high output */
tmp102_set_thermostat_mode(&sensor, TMP102_MODE_INTERRUPT);
tmp102_set_alert_polarity(&sensor, TMP102_ALERT_ACTIVE_HIGH);

/* Require 4 consecutive faults before alerting */
tmp102_set_fault_queue(&sensor, TMP102_FAULTS_4);
```

## Platform Examples

### nRF5 SDK (Nordic)

```c
#include "nrf_drv_twi.h"

static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);

bool nrf_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len, void *user_data)
{
    (void)user_data;
    if (nrf_drv_twi_tx(&m_twi, addr, &reg, 1, true) != NRF_SUCCESS) return false;
    return nrf_drv_twi_rx(&m_twi, addr, data, len) == NRF_SUCCESS;
}

bool nrf_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len, void *user_data)
{
    (void)user_data;
    uint8_t buf[17];
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    return nrf_drv_twi_tx(&m_twi, addr, buf, len + 1, false) == NRF_SUCCESS;
}
```

### STM32 HAL

```c
#include "stm32f4xx_hal.h"

extern I2C_HandleTypeDef hi2c1;

bool stm32_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len, void *user_data)
{
    (void)user_data;
    return HAL_I2C_Mem_Read(&hi2c1, addr << 1, reg, 1, data, len, 100) == HAL_OK;
}

bool stm32_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len, void *user_data)
{
    (void)user_data;
    return HAL_I2C_Mem_Write(&hi2c1, addr << 1, reg, 1, (uint8_t*)data, len, 100) == HAL_OK;
}
```

### ESP-IDF

```c
#include "driver/i2c.h"

bool esp_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len, void *user_data)
{
    (void)user_data;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}

bool esp_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len, void *user_data)
{
    (void)user_data;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK;
}
```

## Hardware Connections

```
VCC (1.4V - 3.6V) ──┬──── TMP102 V+
                    │
                   C1 (100nF bypass)
                    │
GND ────────────────┴──── TMP102 GND

MCU SDA ──────┬───────── TMP102 SDA
              │
             R1 (4.7kΩ pull-up to VCC)

MCU SCL ──────┬───────── TMP102 SCL
              │
             R2 (4.7kΩ pull-up to VCC)

TMP102 ADD0 ──────────── GND (for address 0x48)
                         or VCC (for address 0x49)

TMP102 ALERT ─────────── MCU GPIO (optional, open-drain output)
```

## License

MIT License - See header file for full license text.

## References

- [TMP102 Datasheet](https://www.ti.com/lit/ds/symlink/tmp102.pdf)
- [TMP102 Product Page](https://www.ti.com/product/TMP102)
