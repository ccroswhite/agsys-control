# ADS131M02 Arduino Library

Arduino library for the **Texas Instruments ADS131M02** 24-bit, 2-channel, simultaneous sampling delta-sigma ADC.

## Features

- **2 differential input channels** with simultaneous sampling
- **Programmable gain amplifier (PGA)**: 1, 2, 4, 8, 16, 32, 64, 128×
- **Data rates**: 250 SPS to 32 kSPS
- **Built-in calibration registers** for offset and gain correction
- **Internal 1.2V reference**
- **Low noise**: 6.3 µVRMS at gain = 128
- Compatible with Arduino, ESP32, nRF52, STM32, and other platforms

## Installation

### Arduino Library Manager
1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for "ADS131M02"
4. Click **Install**

### Manual Installation
1. Download this repository as a ZIP file
2. In Arduino IDE, go to **Sketch → Include Library → Add .ZIP Library**
3. Select the downloaded ZIP file

### PlatformIO
Add to your `platformio.ini`:
```ini
lib_deps =
    ADS131M02
```

## Wiring

| ADS131M02 Pin | Arduino Pin | Description |
|---------------|-------------|-------------|
| AVDD | 3.3V | Analog power (2.7V - 3.6V) |
| DVDD | 3.3V | Digital power (1.65V - 3.6V) |
| AGND | GND | Analog ground |
| DGND | GND | Digital ground |
| CS | 10 | SPI chip select |
| SCLK | 13 (SCK) | SPI clock |
| DIN | 11 (MOSI) | SPI data in |
| DOUT | 12 (MISO) | SPI data out |
| DRDY | 9 | Data ready (active low) |
| SYNC/RST | 8 | Sync/Reset (optional) |

**Note:** Connect AGND and DGND at a single star ground point for best performance.

## Quick Start

```cpp
#include <SPI.h>
#include <ADS131M02.h>

ADS131M02 adc;

void setup() {
    Serial.begin(115200);
    
    // Initialize: CS=10, DRDY=9, RESET=8
    if (!adc.begin(10, 9, 8)) {
        Serial.println("ADS131M02 not found!");
        while (1);
    }
    
    // Configure
    adc.setGain(0, ADS131M02_GAIN_1);    // Channel 0 gain
    adc.setGain(1, ADS131M02_GAIN_1);    // Channel 1 gain
    adc.setOSR(ADS131M02_OSR_4096);      // 1 kSPS data rate
}

void loop() {
    if (adc.dataReady()) {
        ads131m02_data_t data;
        if (adc.readData(&data)) {
            float mv = ADS131M02::toMillivolts(data.ch0, adc.getGain(0));
            Serial.print("CH0: ");
            Serial.print(mv, 3);
            Serial.println(" mV");
        }
    }
}
```

## API Reference

### Initialization

```cpp
ADS131M02 adc;                      // Create instance
adc.begin(cs_pin, drdy_pin, reset_pin);  // Initialize (reset_pin optional, use -1)
adc.reset();                        // Reset device
uint8_t id = adc.readID();          // Read device ID (should be 0x22)
```

### Reading Data

```cpp
bool ready = adc.dataReady();       // Check if new data available
adc.readData(&data);                // Read both channels
int32_t raw = adc.readChannel(0);   // Read single channel
```

### Configuration

```cpp
// Gain settings: 1, 2, 4, 8, 16, 32, 64, 128
adc.setGain(channel, ADS131M02_GAIN_32);
ads131m02_gain_t gain = adc.getGain(channel);

// Data rate (OSR)
adc.setOSR(ADS131M02_OSR_4096);     // 1 kSPS
// Options: OSR_128 (32k), OSR_256 (16k), OSR_512 (8k), OSR_1024 (4k),
//          OSR_2048 (2k), OSR_4096 (1k), OSR_8192 (500), OSR_16384 (250)

// Power mode
adc.setPowerMode(ADS131M02_POWER_HR);  // High resolution (recommended)
// Options: POWER_VLP, POWER_LP, POWER_HR

// Enable/disable channels
adc.enableChannel(0, true);
adc.enableChannel(1, false);
```

### Calibration

```cpp
// Offset calibration (short inputs, measure, apply negative)
adc.setOffsetCal(channel, offset);
int32_t offset = adc.getOffsetCal(channel);

// Gain calibration (1.0 = 0x800000)
adc.setGainCal(channel, gain_value);
uint32_t gain = adc.getGainCal(channel);

// Save/load calibration structure
ads131m02_cal_t cal;
adc.saveCalibration(&cal);
adc.loadCalibration(&cal);
```

### Power Management

```cpp
adc.standby();    // Enter low-power standby
adc.wakeup();     // Wake from standby
```

### Conversion Utilities

```cpp
int32_t uv = ADS131M02::toMicrovolts(raw, gain);  // Convert to µV
float mv = ADS131M02::toMillivolts(raw, gain);    // Convert to mV
```

### Register Access

```cpp
uint16_t value = adc.readRegister(addr);
adc.writeRegister(addr, value);
```

## Data Structures

```cpp
// ADC data
typedef struct {
    int32_t ch0;        // Channel 0 (24-bit signed)
    int32_t ch1;        // Channel 1 (24-bit signed)
    uint16_t status;    // Status word
    bool valid;         // Data validity
} ads131m02_data_t;

// Calibration data
typedef struct {
    int32_t offset_ch0;
    int32_t offset_ch1;
    uint32_t gain_ch0;   // 1.0 = 0x800000
    uint32_t gain_ch1;
} ads131m02_cal_t;
```

## Examples

- **BasicReading** - Simple example reading both channels
- **Calibration** - Interactive offset and gain calibration

## Hardware Notes

### Decoupling
- AVDD: 1µF + 100nF to AGND
- DVDD: 1µF + 100nF to DGND
- CAP pin: 100nF to DGND (when DVDD > 2V)

### Input Protection
For sensitive applications, consider adding:
- RC filter on inputs (e.g., 47Ω + 100pF)
- TVS diodes for ESD protection

## License

MIT License - see LICENSE file for details.

## Contributing

Contributions are welcome! Please submit pull requests to the GitHub repository.

## Acknowledgments

- Texas Instruments for the excellent ADS131M02 datasheet
- Adafruit for inspiring the Arduino library ecosystem
