#include "ads131m02.h"
#include <Arduino.h>
#include <SPI.h>

// Module state
static ads131m02_pins_t _pins;
static bool _initialized = false;
static ads131m02_gain_t _gain_ch0 = ADS131M02_GAIN_1;
static ads131m02_gain_t _gain_ch1 = ADS131M02_GAIN_1;

// SPI settings for ADS131M02
// Max SCLK = 25 MHz, Mode 1 (CPOL=0, CPHA=1)
static SPISettings _spi_settings(8000000, MSBFIRST, SPI_MODE1);

// Internal helper functions
static void cs_low(void) {
    digitalWrite(_pins.pin_cs, LOW);
}

static void cs_high(void) {
    digitalWrite(_pins.pin_cs, HIGH);
}

static uint16_t spi_transfer_word(uint16_t data) {
    uint16_t result = 0;
    result = SPI.transfer((data >> 8) & 0xFF) << 8;
    result |= SPI.transfer(data & 0xFF);
    return result;
}

static int32_t sign_extend_24(uint32_t value) {
    if (value & 0x800000) {
        return (int32_t)(value | 0xFF000000);
    }
    return (int32_t)value;
}

bool ads131m02_init(const ads131m02_pins_t* pins) {
    if (pins == NULL) {
        return false;
    }
    
    _pins = *pins;
    
    // Configure pins
    pinMode(_pins.pin_cs, OUTPUT);
    pinMode(_pins.pin_drdy, INPUT);
    pinMode(_pins.pin_sync_rst, OUTPUT);
    
    // Start with CS high (inactive)
    cs_high();
    
    // SYNC/RST high (not in reset)
    digitalWrite(_pins.pin_sync_rst, HIGH);
    
    // Initialize SPI
    SPI.begin();
    
    // Small delay for power-up
    delay(10);
    
    // Reset the device
    ads131m02_reset();
    
    // Verify device ID
    uint8_t id = ads131m02_read_id();
    if (id != ADS131M02_ID_VALUE) {
        return false;
    }
    
    // Unlock registers for configuration
    ads131m02_write_reg(0x00, ADS131M02_CMD_UNLOCK >> 8);
    
    // Default configuration:
    // - Both channels enabled
    // - OSR = 4096 (1 kSPS) - good for 500Hz-2kHz excitation
    // - High resolution mode
    // - Internal reference
    
    // Set CLOCK register: enable channels, OSR = 4096
    uint16_t clock_reg = (1 << 9) |  // CH1_EN
                         (1 << 8) |  // CH0_EN
                         (ADS131M02_OSR_4096 << 2) |  // OSR
                         (ADS131M02_POWER_HR);  // Power mode
    ads131m02_write_reg(ADS131M02_REG_CLOCK, clock_reg);
    
    // Set default gain = 1 for both channels
    ads131m02_set_gain(0, ADS131M02_GAIN_1);
    ads131m02_set_gain(1, ADS131M02_GAIN_1);
    
    _initialized = true;
    return true;
}

void ads131m02_reset(void) {
    // Hardware reset via SYNC/RST pin
    digitalWrite(_pins.pin_sync_rst, LOW);
    delayMicroseconds(10);
    digitalWrite(_pins.pin_sync_rst, HIGH);
    delay(1);  // Wait for reset to complete
    
    // Also send software reset command
    SPI.beginTransaction(_spi_settings);
    cs_low();
    spi_transfer_word(ADS131M02_CMD_RESET);
    spi_transfer_word(0x0000);  // Padding
    spi_transfer_word(0x0000);  // Padding
    cs_high();
    SPI.endTransaction();
    
    delay(1);  // Wait for reset
}

uint8_t ads131m02_read_id(void) {
    uint16_t id_reg = ads131m02_read_reg(ADS131M02_REG_ID);
    return (id_reg >> 8) & 0xFF;
}

uint16_t ads131m02_read_reg(uint8_t addr) {
    uint16_t cmd = ADS131M02_CMD_RREG | ((addr & 0x3F) << 7);
    uint16_t response = 0;
    
    SPI.beginTransaction(_spi_settings);
    cs_low();
    
    // Send read command
    spi_transfer_word(cmd);
    spi_transfer_word(0x0000);  // CH0 data (ignored)
    spi_transfer_word(0x0000);  // CH1 data (ignored)
    
    cs_high();
    
    // Need another transaction to get the response
    delayMicroseconds(1);
    cs_low();
    
    spi_transfer_word(ADS131M02_CMD_NULL);  // NULL command
    response = spi_transfer_word(0x0000);   // Register data in CH0 position
    spi_transfer_word(0x0000);              // CH1 data (ignored)
    
    cs_high();
    SPI.endTransaction();
    
    return response;
}

void ads131m02_write_reg(uint8_t addr, uint16_t value) {
    uint16_t cmd = ADS131M02_CMD_WREG | ((addr & 0x3F) << 7);
    
    SPI.beginTransaction(_spi_settings);
    cs_low();
    
    // Send write command with data
    spi_transfer_word(cmd);
    spi_transfer_word(value);   // Register data in CH0 position
    spi_transfer_word(0x0000);  // CH1 position (padding)
    
    cs_high();
    SPI.endTransaction();
}

void ads131m02_set_gain(uint8_t channel, ads131m02_gain_t gain) {
    uint16_t gain_reg = ads131m02_read_reg(ADS131M02_REG_GAIN);
    
    if (channel == 0) {
        gain_reg = (gain_reg & 0xFFF8) | (gain & 0x07);
        _gain_ch0 = gain;
    } else if (channel == 1) {
        gain_reg = (gain_reg & 0xFF8F) | ((gain & 0x07) << 4);
        _gain_ch1 = gain;
    }
    
    ads131m02_write_reg(ADS131M02_REG_GAIN, gain_reg);
}

ads131m02_gain_t ads131m02_get_gain(uint8_t channel) {
    if (channel == 0) {
        return _gain_ch0;
    } else {
        return _gain_ch1;
    }
}

void ads131m02_set_osr(ads131m02_osr_t osr) {
    uint16_t clock_reg = ads131m02_read_reg(ADS131M02_REG_CLOCK);
    clock_reg = (clock_reg & 0xFFE3) | ((osr & 0x07) << 2);
    ads131m02_write_reg(ADS131M02_REG_CLOCK, clock_reg);
}

void ads131m02_set_power_mode(ads131m02_power_t mode) {
    uint16_t clock_reg = ads131m02_read_reg(ADS131M02_REG_CLOCK);
    clock_reg = (clock_reg & 0xFFFC) | (mode & 0x03);
    ads131m02_write_reg(ADS131M02_REG_CLOCK, clock_reg);
}

void ads131m02_enable_channel(uint8_t channel, bool enable) {
    uint16_t clock_reg = ads131m02_read_reg(ADS131M02_REG_CLOCK);
    
    if (channel == 0) {
        if (enable) {
            clock_reg |= (1 << 8);
        } else {
            clock_reg &= ~(1 << 8);
        }
    } else if (channel == 1) {
        if (enable) {
            clock_reg |= (1 << 9);
        } else {
            clock_reg &= ~(1 << 9);
        }
    }
    
    ads131m02_write_reg(ADS131M02_REG_CLOCK, clock_reg);
}

bool ads131m02_data_ready(void) {
    return digitalRead(_pins.pin_drdy) == LOW;
}

bool ads131m02_read_data(ads131m02_data_t* data) {
    if (data == NULL) {
        return false;
    }
    
    data->valid = false;
    
    // Check if data is ready
    if (!ads131m02_data_ready()) {
        return false;
    }
    
    uint16_t status_word;
    uint32_t ch0_raw = 0;
    uint32_t ch1_raw = 0;
    
    SPI.beginTransaction(_spi_settings);
    cs_low();
    
    // Send NULL command, receive status and data
    // Frame format: [Status/Response][CH0 MSB][CH0 LSB][CH1 MSB][CH1 LSB][CRC]
    // Using 24-bit word mode (default)
    
    // Word 0: Command out, Status in
    status_word = spi_transfer_word(ADS131M02_CMD_NULL);
    uint8_t status_lsb = SPI.transfer(0x00);
    
    // Word 1: CH0 data (24-bit)
    ch0_raw = SPI.transfer(0x00) << 16;
    ch0_raw |= SPI.transfer(0x00) << 8;
    ch0_raw |= SPI.transfer(0x00);
    
    // Word 2: CH1 data (24-bit)
    ch1_raw = SPI.transfer(0x00) << 16;
    ch1_raw |= SPI.transfer(0x00) << 8;
    ch1_raw |= SPI.transfer(0x00);
    
    // Word 3: CRC (ignore for now)
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    
    cs_high();
    SPI.endTransaction();
    
    // Sign-extend 24-bit values to 32-bit
    data->ch0 = sign_extend_24(ch0_raw);
    data->ch1 = sign_extend_24(ch1_raw);
    data->status = status_word;
    data->valid = true;
    
    return true;
}

void ads131m02_set_offset_cal(uint8_t channel, int32_t offset) {
    uint8_t msb_reg, lsb_reg;
    
    if (channel == 0) {
        msb_reg = ADS131M02_REG_CH0_OCAL_MSB;
        lsb_reg = ADS131M02_REG_CH0_OCAL_LSB;
    } else {
        msb_reg = ADS131M02_REG_CH1_OCAL_MSB;
        lsb_reg = ADS131M02_REG_CH1_OCAL_LSB;
    }
    
    // Offset is 24-bit signed, stored in two 16-bit registers
    ads131m02_write_reg(msb_reg, (offset >> 8) & 0xFFFF);
    ads131m02_write_reg(lsb_reg, (offset & 0xFF) << 8);
}

int32_t ads131m02_get_offset_cal(uint8_t channel) {
    uint8_t msb_reg, lsb_reg;
    
    if (channel == 0) {
        msb_reg = ADS131M02_REG_CH0_OCAL_MSB;
        lsb_reg = ADS131M02_REG_CH0_OCAL_LSB;
    } else {
        msb_reg = ADS131M02_REG_CH1_OCAL_MSB;
        lsb_reg = ADS131M02_REG_CH1_OCAL_LSB;
    }
    
    uint16_t msb = ads131m02_read_reg(msb_reg);
    uint16_t lsb = ads131m02_read_reg(lsb_reg);
    
    int32_t offset = ((int32_t)msb << 8) | ((lsb >> 8) & 0xFF);
    return sign_extend_24(offset);
}

void ads131m02_set_gain_cal(uint8_t channel, uint32_t gain) {
    uint8_t msb_reg, lsb_reg;
    
    if (channel == 0) {
        msb_reg = ADS131M02_REG_CH0_GCAL_MSB;
        lsb_reg = ADS131M02_REG_CH0_GCAL_LSB;
    } else {
        msb_reg = ADS131M02_REG_CH1_GCAL_MSB;
        lsb_reg = ADS131M02_REG_CH1_GCAL_LSB;
    }
    
    // Gain is 24-bit unsigned, 1.0 = 0x800000
    ads131m02_write_reg(msb_reg, (gain >> 8) & 0xFFFF);
    ads131m02_write_reg(lsb_reg, (gain & 0xFF) << 8);
}

uint32_t ads131m02_get_gain_cal(uint8_t channel) {
    uint8_t msb_reg, lsb_reg;
    
    if (channel == 0) {
        msb_reg = ADS131M02_REG_CH0_GCAL_MSB;
        lsb_reg = ADS131M02_REG_CH0_GCAL_LSB;
    } else {
        msb_reg = ADS131M02_REG_CH1_GCAL_MSB;
        lsb_reg = ADS131M02_REG_CH1_GCAL_LSB;
    }
    
    uint16_t msb = ads131m02_read_reg(msb_reg);
    uint16_t lsb = ads131m02_read_reg(lsb_reg);
    
    return ((uint32_t)msb << 8) | ((lsb >> 8) & 0xFF);
}

void ads131m02_load_calibration(const ads131m02_cal_t* cal) {
    if (cal == NULL) return;
    
    ads131m02_set_offset_cal(0, cal->offset_ch0);
    ads131m02_set_offset_cal(1, cal->offset_ch1);
    ads131m02_set_gain_cal(0, cal->gain_ch0);
    ads131m02_set_gain_cal(1, cal->gain_ch1);
}

void ads131m02_save_calibration(ads131m02_cal_t* cal) {
    if (cal == NULL) return;
    
    cal->offset_ch0 = ads131m02_get_offset_cal(0);
    cal->offset_ch1 = ads131m02_get_offset_cal(1);
    cal->gain_ch0 = ads131m02_get_gain_cal(0);
    cal->gain_ch1 = ads131m02_get_gain_cal(1);
}

void ads131m02_standby(void) {
    SPI.beginTransaction(_spi_settings);
    cs_low();
    spi_transfer_word(ADS131M02_CMD_STANDBY);
    spi_transfer_word(0x0000);
    spi_transfer_word(0x0000);
    cs_high();
    SPI.endTransaction();
}

void ads131m02_wakeup(void) {
    SPI.beginTransaction(_spi_settings);
    cs_low();
    spi_transfer_word(ADS131M02_CMD_WAKEUP);
    spi_transfer_word(0x0000);
    spi_transfer_word(0x0000);
    cs_high();
    SPI.endTransaction();
}

int32_t ads131m02_to_microvolts(int32_t raw, ads131m02_gain_t gain) {
    // Full scale = ±1.2V (internal reference)
    // 24-bit signed: ±8388607 counts
    // LSB = 1.2V / 8388608 = 143 nV (at gain = 1)
    
    // Gain multipliers
    static const uint8_t gain_values[] = {1, 2, 4, 8, 16, 32, 64, 128};
    uint8_t gain_mult = gain_values[gain & 0x07];
    
    // Calculate microvolts
    // uV = (raw * 1200000) / (8388608 * gain)
    // To avoid overflow, use: uV = (raw * 1200000 / gain) / 8388608
    // Simplified: uV = raw * (1200000 / 8388608) / gain = raw * 0.143 / gain
    
    int64_t uv = ((int64_t)raw * 1200000LL) / ((int64_t)8388608 * gain_mult);
    return (int32_t)uv;
}
