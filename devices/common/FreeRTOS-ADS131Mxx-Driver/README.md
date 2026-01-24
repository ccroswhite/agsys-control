# ADS131M0x 24-bit Delta-Sigma ADC Driver

A complete, platform-agnostic driver for the Texas Instruments ADS131M0x family of simultaneous-sampling, 24-bit, delta-sigma ADCs.

## Supported Devices

| Variant | Channels | Device ID |
|---------|----------|-----------|
| ADS131M01 | 1 | 0x01xx |
| ADS131M02 | 2 | 0x02xx |
| ADS131M03 | 3 | 0x03xx |
| ADS131M04 | 4 | 0x04xx |
| ADS131M06 | 6 | 0x06xx |
| ADS131M08 | 8 | 0x08xx |

## Features

- **24-bit Resolution**: High-precision delta-sigma conversion
- **Simultaneous Sampling**: All channels sampled at the same instant
- **Configurable Sample Rate**: 250 SPS to 32 kSPS
- **Programmable Gain**: 1x to 128x per channel
- **Global-Chop Mode**: Reduces offset drift
- **Per-Channel Calibration**: Offset and gain calibration registers
- **Phase Delay Calibration**: Align channels with different signal phases
- **CRC Validation**: Optional CRC on SPI communications
- **Compile-Time Device Selection**: Zero runtime overhead for device variant
- **Device ID Verification**: Validates hardware matches compile-time selection
- **Platform Agnostic**: Works with any SPI and GPIO implementation

## Quick Start

### 1. Select Your Device (compile-time)

```c
/* Define your device BEFORE including the header */
#define ADS131M0X_DEVICE_M02    /* For ADS131M02 (2-channel) */
#include "ads131m0x.h"
```

Available device defines:
- `ADS131M0X_DEVICE_M01` - 1 channel
- `ADS131M0X_DEVICE_M02` - 2 channels
- `ADS131M0X_DEVICE_M03` - 3 channels
- `ADS131M0X_DEVICE_M04` - 4 channels
- `ADS131M0X_DEVICE_M06` - 6 channels
- `ADS131M0X_DEVICE_M08` - 8 channels

### 2. Implement HAL Interface

```c

/* SPI transfer function - must handle CS internally */
bool my_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len, void *user_data)
{
    /* Assert CS */
    gpio_clear(SPI_CS_PIN);
    
    /* Perform full-duplex SPI transfer */
    spi_transfer(tx, rx, len);
    
    /* Deassert CS */
    gpio_set(SPI_CS_PIN);
    
    return true;
}

/* GPIO read function */
bool my_gpio_read(uint8_t pin, void *user_data)
{
    return gpio_read(pin) != 0;
}

/* GPIO write function */
void my_gpio_write(uint8_t pin, bool value, void *user_data)
{
    if (value) {
        gpio_set(pin);
    } else {
        gpio_clear(pin);
    }
}

/* Delay function */
void my_delay_ms(uint32_t ms, void *user_data)
{
    delay_milliseconds(ms);
}
```

### 3. Initialize the Driver

```c
/* Set up HAL interface */
ads131m0x_hal_t hal = {
    .spi_transfer = my_spi_transfer,
    .gpio_read = my_gpio_read,
    .gpio_write = my_gpio_write,
    .delay_ms = my_delay_ms,
    .user_data = NULL
};

/* Configure device */
ads131m0x_config_t config = ADS131M0X_CONFIG_DEFAULT();
config.sync_reset_pin = SYNC_RST_PIN;
config.drdy_pin = DRDY_PIN;
config.osr = ADS131M0X_OSR_4096;        /* 1 kSPS */
config.power_mode = ADS131M0X_PWR_HIGH_RES;
config.gain[0] = ADS131M0X_GAIN_1X;
config.gain[1] = ADS131M0X_GAIN_1X;

/* Initialize (verifies device ID matches compile-time selection) */
ads131m0x_ctx_t adc;
if (!ads131m0x_init(&adc, &hal, &config)) {
    printf("ADC init failed! Check device and connections.\n");
    return;
}

printf("Initialized: %s (%d channels)\n", 
       ads131m0x_get_device_name(),
       ads131m0x_get_num_channels());
```

### 4. Read Samples

```c
ads131m0x_sample_t sample;

/* Wait for data ready */
while (!ads131m0x_data_ready(&adc)) {
    /* Optionally sleep or do other work */
}

/* Read sample */
if (ads131m0x_read_sample(&adc, &sample)) {
    for (int ch = 0; ch < ADS131M0X_NUM_CHANNELS; ch++) {
        float voltage = ads131m0x_to_voltage(sample.ch[ch], adc.gain[ch], 1.2f);
        printf("CH%d: %d (%.6f V)\n", ch, sample.ch[ch], voltage);
    }
}
```

## API Reference

### Initialization

```c
/* Initialize device with configuration (verifies device ID) */
bool ads131m0x_init(ads131m0x_ctx_t *ctx, const ads131m0x_hal_t *hal,
                    const ads131m0x_config_t *config);

/* Reset device */
bool ads131m0x_reset(ads131m0x_ctx_t *ctx);

/* Verify device ID matches compile-time selection */
bool ads131m0x_verify_device_id(ads131m0x_ctx_t *ctx);

/* Get device name (compile-time constant) */
const char* ads131m0x_get_device_name(void);

/* Get number of channels (compile-time constant) */
uint8_t ads131m0x_get_num_channels(void);
```

### Sampling

```c
/* Read sample from all channels */
bool ads131m0x_read_sample(ads131m0x_ctx_t *ctx, ads131m0x_sample_t *sample);

/* Check if data ready */
bool ads131m0x_data_ready(ads131m0x_ctx_t *ctx);

/* Wait for data ready with timeout */
bool ads131m0x_wait_data_ready(ads131m0x_ctx_t *ctx, uint32_t timeout_ms);
```

### Configuration

```c
/* Set oversampling ratio (sample rate) */
bool ads131m0x_set_osr(ads131m0x_ctx_t *ctx, ads131m0x_osr_t osr);

/* Set channel gain */
bool ads131m0x_set_gain(ads131m0x_ctx_t *ctx, uint8_t channel, ads131m0x_gain_t gain);

/* Set power mode */
bool ads131m0x_set_power_mode(ads131m0x_ctx_t *ctx, ads131m0x_power_t mode);

/* Enable/disable channel */
bool ads131m0x_set_channel_enable(ads131m0x_ctx_t *ctx, uint8_t channel, bool enable);

/* Set input multiplexer */
bool ads131m0x_set_input_mux(ads131m0x_ctx_t *ctx, uint8_t channel, ads131m0x_mux_t mux);
```

### Calibration

```c
/* Set/get offset calibration */
bool ads131m0x_set_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, int32_t offset);
bool ads131m0x_get_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, int32_t *offset);

/* Set/get gain calibration */
bool ads131m0x_set_gain_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint32_t gain_cal);
bool ads131m0x_get_gain_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint32_t *gain_cal);

/* Automatic offset calibration (shorts inputs internally) */
bool ads131m0x_auto_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t num_samples);

/* Reset calibration to defaults */
bool ads131m0x_reset_calibration(ads131m0x_ctx_t *ctx, uint8_t channel);

/* Phase delay calibration */
bool ads131m0x_set_phase_delay(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t phase_delay);
bool ads131m0x_get_phase_delay(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t *phase_delay);
```

### Global-Chop Mode

```c
/* Enable global-chop (reduces offset drift) */
bool ads131m0x_enable_global_chop(ads131m0x_ctx_t *ctx, ads131m0x_gc_delay_t delay);

/* Disable global-chop */
bool ads131m0x_disable_global_chop(ads131m0x_ctx_t *ctx);

/* Check if enabled */
bool ads131m0x_is_global_chop_enabled(ads131m0x_ctx_t *ctx);
```

### CRC

```c
/* Enable CRC on communications */
bool ads131m0x_enable_crc(ads131m0x_ctx_t *ctx, bool enable_input, 
                          bool enable_output, bool use_ccitt);

/* Disable CRC */
bool ads131m0x_disable_crc(ads131m0x_ctx_t *ctx);

/* Read register map CRC */
bool ads131m0x_read_regmap_crc(ads131m0x_ctx_t *ctx, uint16_t *crc);
```

### Power Management

```c
/* Enter standby mode */
bool ads131m0x_standby(ads131m0x_ctx_t *ctx);

/* Wake from standby */
bool ads131m0x_wakeup(ads131m0x_ctx_t *ctx);
```

## Sample Rates

| OSR | Sample Rate | Notes |
|-----|-------------|-------|
| 128 | 32 kSPS | Highest speed |
| 256 | 16 kSPS | |
| 512 | 8 kSPS | |
| 1024 | 4 kSPS | |
| 2048 | 2 kSPS | |
| 4096 | 1 kSPS | Good balance |
| 8192 | 500 SPS | |
| 16384 | 250 SPS | Lowest noise |

*Sample rates assume 8.192 MHz clock (fCLKIN)*

## Gain Settings

| Setting | Multiplier | Full-Scale Input (±VREF/Gain) |
|---------|------------|-------------------------------|
| GAIN_1X | 1 | ±1.2V |
| GAIN_2X | 2 | ±600mV |
| GAIN_4X | 4 | ±300mV |
| GAIN_8X | 8 | ±150mV |
| GAIN_16X | 16 | ±75mV |
| GAIN_32X | 32 | ±37.5mV |
| GAIN_64X | 64 | ±18.75mV |
| GAIN_128X | 128 | ±9.375mV |

*VREF = 1.2V internal reference*

## Calibration

### Offset Calibration

The ADC has per-channel offset calibration registers. The offset is subtracted from the raw conversion result:

```
calibrated = raw - offset
```

Use `ads131m0x_auto_offset_cal()` to automatically measure and set the offset by shorting the inputs internally:

```c
/* Calibrate channel 0 with 32 samples */
ads131m0x_auto_offset_cal(&adc, 0, 32);
```

### Gain Calibration

Gain calibration is applied as:

```
calibrated = raw × (gain_cal / 2^23)
```

- `0x800000` = 1.0 (default)
- `0x400000` = 0.5
- `0xC00000` = 1.5

```c
/* Set gain to 1.001 (fine adjustment) */
ads131m0x_set_gain_cal(&adc, 0, 0x8020C5);
```

### Phase Delay Calibration

For applications measuring signals with known phase relationships, the phase delay register can align channels:

```c
/* Delay channel 1 by 100 clock cycles */
ads131m0x_set_phase_delay(&adc, 1, 100);
```

Each step is 1/fCLKIN (122ns at 8.192MHz).

## Global-Chop Mode

Global-chop periodically swaps the input polarity and averages the results, canceling offset drift:

```c
/* Enable with 16 fMOD period delay */
ads131m0x_enable_global_chop(&adc, ADS131M0X_GC_DLY_16);
```

**Benefits:**
- Reduces offset drift over temperature
- Cancels 1/f noise
- Improves DC accuracy

**Trade-offs:**
- Slightly increased settling time
- Not suitable for high-frequency signals

## Platform Examples

### nRF5 SDK (Nordic)

```c
#define ADS131M0X_DEVICE_M02  /* Select your device */
#include "ads131m0x.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

static const nrf_drv_spi_t m_spi = NRF_DRV_SPI_INSTANCE(0);

bool nrf_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len, void *user_data)
{
    nrf_gpio_pin_clear(CS_PIN);
    nrf_drv_spi_transfer(&m_spi, tx, len, rx, len);
    nrf_gpio_pin_set(CS_PIN);
    return true;
}

bool nrf_gpio_read(uint8_t pin, void *user_data)
{
    return nrf_gpio_pin_read(pin) != 0;
}

void nrf_gpio_write(uint8_t pin, bool value, void *user_data)
{
    if (value) nrf_gpio_pin_set(pin);
    else nrf_gpio_pin_clear(pin);
}

void nrf_delay(uint32_t ms, void *user_data)
{
    nrf_delay_ms(ms);
}
```

### STM32 HAL

```c
#define ADS131M0X_DEVICE_M04  /* Select your device */
#include "ads131m0x.h"
#include "stm32f4xx_hal.h"

extern SPI_HandleTypeDef hspi1;

bool stm32_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len, void *user_data)
{
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)tx, rx, len, 100);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
    return true;
}

bool stm32_gpio_read(uint8_t pin, void *user_data)
{
    return HAL_GPIO_ReadPin(DRDY_GPIO_Port, DRDY_Pin) == GPIO_PIN_SET;
}

void stm32_gpio_write(uint8_t pin, bool value, void *user_data)
{
    HAL_GPIO_WritePin(SYNC_GPIO_Port, SYNC_Pin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void stm32_delay(uint32_t ms, void *user_data)
{
    HAL_Delay(ms);
}
```

### ESP-IDF

```c
#define ADS131M0X_DEVICE_M08  /* Select your device */
#include "ads131m0x.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static spi_device_handle_t spi_handle;

bool esp_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len, void *user_data)
{
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    return spi_device_transmit(spi_handle, &t) == ESP_OK;
}

bool esp_gpio_read(uint8_t pin, void *user_data)
{
    return gpio_get_level(pin) != 0;
}

void esp_gpio_write(uint8_t pin, bool value, void *user_data)
{
    gpio_set_level(pin, value ? 1 : 0);
}

void esp_delay(uint32_t ms, void *user_data)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
```

## Hardware Connections

```
MCU                         ADS131M0x
---                         ---------
SPI_MOSI  ─────────────────  DIN
SPI_MISO  ─────────────────  DOUT
SPI_SCK   ─────────────────  SCLK
GPIO (CS) ─────────────────  CS
GPIO      ─────────────────  DRDY (input, active low)
GPIO      ─────────────────  SYNC/RESET (active low)

Power:
DVDD = 3.3V (digital)
AVDD = 3.3V (analog, separate from digital if possible)
AVSS = GND
DVSS = GND

Clock:
CLKIN = 8.192 MHz crystal or oscillator
```

## SPI Configuration

- **Mode**: SPI Mode 1 (CPOL=0, CPHA=1)
- **Bit Order**: MSB first
- **Max Clock**: 25 MHz (check datasheet for your variant)
- **Word Size**: 24-bit default (configurable to 16 or 32)

## License

MIT License - See header file for full license text.

## References

- [ADS131M02 Datasheet](https://www.ti.com/lit/ds/symlink/ads131m02.pdf)
- [ADS131M04 Datasheet](https://www.ti.com/lit/ds/symlink/ads131m04.pdf)
- [ADS131M08 Datasheet](https://www.ti.com/lit/ds/symlink/ads131m08.pdf)
- [TI Product Family Page](https://www.ti.com/product/ADS131M02)
