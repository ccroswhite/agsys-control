#ifndef ADS131M02_H
#define ADS131M02_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ADS131M02 Register Addresses
#define ADS131M02_REG_ID            0x00
#define ADS131M02_REG_STATUS        0x01
#define ADS131M02_REG_MODE          0x02
#define ADS131M02_REG_CLOCK         0x03
#define ADS131M02_REG_GAIN          0x04
#define ADS131M02_REG_CFG           0x06
#define ADS131M02_REG_THRSHLD_MSB   0x07
#define ADS131M02_REG_THRSHLD_LSB   0x08
#define ADS131M02_REG_CH0_CFG       0x09
#define ADS131M02_REG_CH0_OCAL_MSB  0x0A
#define ADS131M02_REG_CH0_OCAL_LSB  0x0B
#define ADS131M02_REG_CH0_GCAL_MSB  0x0C
#define ADS131M02_REG_CH0_GCAL_LSB  0x0D
#define ADS131M02_REG_CH1_CFG       0x0E
#define ADS131M02_REG_CH1_OCAL_MSB  0x0F
#define ADS131M02_REG_CH1_OCAL_LSB  0x10
#define ADS131M02_REG_CH1_GCAL_MSB  0x11
#define ADS131M02_REG_CH1_GCAL_LSB  0x12
#define ADS131M02_REG_REGMAP_CRC    0x3E

// Commands
#define ADS131M02_CMD_NULL          0x0000
#define ADS131M02_CMD_RESET         0x0011
#define ADS131M02_CMD_STANDBY       0x0022
#define ADS131M02_CMD_WAKEUP        0x0033
#define ADS131M02_CMD_LOCK          0x0555
#define ADS131M02_CMD_UNLOCK        0x0655
#define ADS131M02_CMD_RREG          0xA000  // OR with (addr << 7)
#define ADS131M02_CMD_WREG          0x6000  // OR with (addr << 7)

// Device ID
#define ADS131M02_ID_VALUE          0x22    // Expected ID for ADS131M02

// PGA Gain Settings (for GAIN register)
typedef enum {
    ADS131M02_GAIN_1   = 0x00,
    ADS131M02_GAIN_2   = 0x01,
    ADS131M02_GAIN_4   = 0x02,
    ADS131M02_GAIN_8   = 0x03,
    ADS131M02_GAIN_16  = 0x04,
    ADS131M02_GAIN_32  = 0x05,
    ADS131M02_GAIN_64  = 0x06,
    ADS131M02_GAIN_128 = 0x07
} ads131m02_gain_t;

// Oversampling Ratio (OSR) - affects data rate
typedef enum {
    ADS131M02_OSR_128   = 0x00,  // 32 kSPS
    ADS131M02_OSR_256   = 0x01,  // 16 kSPS
    ADS131M02_OSR_512   = 0x02,  // 8 kSPS
    ADS131M02_OSR_1024  = 0x03,  // 4 kSPS
    ADS131M02_OSR_2048  = 0x04,  // 2 kSPS
    ADS131M02_OSR_4096  = 0x05,  // 1 kSPS
    ADS131M02_OSR_8192  = 0x06,  // 500 SPS
    ADS131M02_OSR_16384 = 0x07   // 250 SPS
} ads131m02_osr_t;

// Power mode
typedef enum {
    ADS131M02_POWER_VLP = 0x00,  // Very low power
    ADS131M02_POWER_LP  = 0x01,  // Low power
    ADS131M02_POWER_HR  = 0x02   // High resolution
} ads131m02_power_t;

// Status register bits
#define ADS131M02_STATUS_LOCK       (1 << 15)
#define ADS131M02_STATUS_RESYNC     (1 << 14)
#define ADS131M02_STATUS_REGMAP     (1 << 13)
#define ADS131M02_STATUS_CRC_ERR    (1 << 12)
#define ADS131M02_STATUS_CRC_TYPE   (1 << 11)
#define ADS131M02_STATUS_RESET      (1 << 10)
#define ADS131M02_STATUS_WLENGTH    (3 << 8)
#define ADS131M02_STATUS_DRDY1      (1 << 1)
#define ADS131M02_STATUS_DRDY0      (1 << 0)

// Pin configuration structure
typedef struct {
    uint8_t pin_cs;
    uint8_t pin_drdy;
    uint8_t pin_sync_rst;
    uint8_t pin_sclk;
    uint8_t pin_mosi;
    uint8_t pin_miso;
} ads131m02_pins_t;

// Calibration data structure
typedef struct {
    int32_t offset_ch0;
    int32_t offset_ch1;
    uint32_t gain_ch0;      // 1.0 = 0x800000
    uint32_t gain_ch1;
} ads131m02_cal_t;

// ADC data structure
typedef struct {
    int32_t ch0;            // Channel 0 (electrode signal)
    int32_t ch1;            // Channel 1 (current sense)
    uint16_t status;        // Status word from response
    bool valid;             // Data validity flag
} ads131m02_data_t;

// Initialize the ADS131M02
// Returns true on success, false on failure
bool ads131m02_init(const ads131m02_pins_t* pins);

// Reset the device
void ads131m02_reset(void);

// Read device ID (should return 0x22 for ADS131M02)
uint8_t ads131m02_read_id(void);

// Configure PGA gain for a channel
void ads131m02_set_gain(uint8_t channel, ads131m02_gain_t gain);

// Get current gain setting for a channel
ads131m02_gain_t ads131m02_get_gain(uint8_t channel);

// Configure oversampling ratio (affects data rate)
void ads131m02_set_osr(ads131m02_osr_t osr);

// Configure power mode
void ads131m02_set_power_mode(ads131m02_power_t mode);

// Enable/disable a channel
void ads131m02_enable_channel(uint8_t channel, bool enable);

// Read ADC data (both channels simultaneously)
// Returns true if new data was available
bool ads131m02_read_data(ads131m02_data_t* data);

// Check if data is ready (DRDY pin low)
bool ads131m02_data_ready(void);

// Set offset calibration for a channel
void ads131m02_set_offset_cal(uint8_t channel, int32_t offset);

// Get offset calibration for a channel
int32_t ads131m02_get_offset_cal(uint8_t channel);

// Set gain calibration for a channel (1.0 = 0x800000)
void ads131m02_set_gain_cal(uint8_t channel, uint32_t gain);

// Get gain calibration for a channel
uint32_t ads131m02_get_gain_cal(uint8_t channel);

// Load calibration from structure
void ads131m02_load_calibration(const ads131m02_cal_t* cal);

// Save current calibration to structure
void ads131m02_save_calibration(ads131m02_cal_t* cal);

// Enter standby mode (low power)
void ads131m02_standby(void);

// Wake from standby
void ads131m02_wakeup(void);

// Read a register
uint16_t ads131m02_read_reg(uint8_t addr);

// Write a register
void ads131m02_write_reg(uint8_t addr, uint16_t value);

// Convert raw ADC value to voltage (in microvolts)
// Assumes Vref = 1.2V internal
int32_t ads131m02_to_microvolts(int32_t raw, ads131m02_gain_t gain);

#ifdef __cplusplus
}
#endif

#endif // ADS131M02_H
