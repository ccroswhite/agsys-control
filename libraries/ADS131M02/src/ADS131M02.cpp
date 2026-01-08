/*!
 * @file ADS131M02.cpp
 *
 * Arduino library for the Texas Instruments ADS131M02
 * 24-bit, 2-channel, simultaneous sampling delta-sigma ADC
 *
 * Written by AgSys for the open source community.
 *
 * MIT License - see LICENSE file for details
 */

#include "ADS131M02.h"

// SPI settings: Max 25 MHz, Mode 1 (CPOL=0, CPHA=1)
#define ADS131M02_SPI_SPEED 8000000

ADS131M02::ADS131M02(SPIClass *spi) {
    _spi = spi;
    _cs_pin = -1;
    _drdy_pin = -1;
    _reset_pin = -1;
    _gain_ch0 = ADS131M02_GAIN_1;
    _gain_ch1 = ADS131M02_GAIN_1;
    _initialized = false;
}

bool ADS131M02::begin(int8_t cs_pin, int8_t drdy_pin, int8_t reset_pin) {
    _cs_pin = cs_pin;
    _drdy_pin = drdy_pin;
    _reset_pin = reset_pin;

    // Configure pins
    pinMode(_cs_pin, OUTPUT);
    csHigh();

    if (_drdy_pin >= 0) {
        pinMode(_drdy_pin, INPUT);
    }

    if (_reset_pin >= 0) {
        pinMode(_reset_pin, OUTPUT);
        digitalWrite(_reset_pin, HIGH);
    }

    // Initialize SPI
    _spi->begin();

    // Small delay for power-up
    delay(10);

    // Reset the device
    reset();

    // Verify device ID
    uint8_t id = readID();
    if (id != ADS131M02_ID_VALUE) {
        return false;
    }

    // Unlock registers for configuration
    _spi->beginTransaction(SPISettings(ADS131M02_SPI_SPEED, MSBFIRST, SPI_MODE1));
    csLow();
    spiTransferWord(ADS131M02_CMD_UNLOCK);
    spiTransferWord(0x0000);
    spiTransferWord(0x0000);
    csHigh();
    _spi->endTransaction();

    // Default configuration:
    // - Both channels enabled
    // - OSR = 4096 (1 kSPS)
    // - High resolution mode
    uint16_t clock_reg = (1 << 9) |  // CH1_EN
                         (1 << 8) |  // CH0_EN
                         (ADS131M02_OSR_4096 << 2) |
                         (ADS131M02_POWER_HR);
    writeRegister(ADS131M02_REG_CLOCK, clock_reg);

    // Set default gain = 1 for both channels
    setGain(0, ADS131M02_GAIN_1);
    setGain(1, ADS131M02_GAIN_1);

    _initialized = true;
    return true;
}

void ADS131M02::reset(void) {
    // Hardware reset if pin available
    if (_reset_pin >= 0) {
        digitalWrite(_reset_pin, LOW);
        delayMicroseconds(10);
        digitalWrite(_reset_pin, HIGH);
        delay(1);
    }

    // Software reset command
    _spi->beginTransaction(SPISettings(ADS131M02_SPI_SPEED, MSBFIRST, SPI_MODE1));
    csLow();
    spiTransferWord(ADS131M02_CMD_RESET);
    spiTransferWord(0x0000);
    spiTransferWord(0x0000);
    csHigh();
    _spi->endTransaction();

    delay(1);
}

uint8_t ADS131M02::readID(void) {
    uint16_t id_reg = readRegister(ADS131M02_REG_ID);
    return (id_reg >> 8) & 0xFF;
}

bool ADS131M02::dataReady(void) {
    if (_drdy_pin < 0) {
        return true;  // Assume ready if no DRDY pin
    }
    return digitalRead(_drdy_pin) == LOW;
}

bool ADS131M02::readData(ads131m02_data_t *data) {
    if (data == NULL) {
        return false;
    }

    data->valid = false;

    if (!dataReady()) {
        return false;
    }

    uint16_t status_word;
    uint32_t ch0_raw = 0;
    uint32_t ch1_raw = 0;

    _spi->beginTransaction(SPISettings(ADS131M02_SPI_SPEED, MSBFIRST, SPI_MODE1));
    csLow();

    // Frame format (24-bit word mode):
    // [Status/Response][CH0 data][CH1 data][CRC]

    // Word 0: Command out, Status in (24-bit)
    status_word = spiTransferWord(ADS131M02_CMD_NULL);
    _spi->transfer(0x00);  // LSB of status

    // Word 1: CH0 data (24-bit)
    ch0_raw = _spi->transfer(0x00) << 16;
    ch0_raw |= _spi->transfer(0x00) << 8;
    ch0_raw |= _spi->transfer(0x00);

    // Word 2: CH1 data (24-bit)
    ch1_raw = _spi->transfer(0x00) << 16;
    ch1_raw |= _spi->transfer(0x00) << 8;
    ch1_raw |= _spi->transfer(0x00);

    // Word 3: CRC (ignore)
    _spi->transfer(0x00);
    _spi->transfer(0x00);
    _spi->transfer(0x00);

    csHigh();
    _spi->endTransaction();

    data->ch0 = signExtend24(ch0_raw);
    data->ch1 = signExtend24(ch1_raw);
    data->status = status_word;
    data->valid = true;

    return true;
}

int32_t ADS131M02::readChannel(uint8_t channel) {
    ads131m02_data_t data;
    if (readData(&data)) {
        return (channel == 0) ? data.ch0 : data.ch1;
    }
    return 0;
}

uint16_t ADS131M02::readRegister(uint8_t addr) {
    uint16_t cmd = ADS131M02_CMD_RREG | ((addr & 0x3F) << 7);
    uint16_t response = 0;

    _spi->beginTransaction(SPISettings(ADS131M02_SPI_SPEED, MSBFIRST, SPI_MODE1));

    // Send read command
    csLow();
    spiTransferWord(cmd);
    spiTransferWord(0x0000);
    spiTransferWord(0x0000);
    csHigh();

    delayMicroseconds(1);

    // Get response
    csLow();
    spiTransferWord(ADS131M02_CMD_NULL);
    response = spiTransferWord(0x0000);
    spiTransferWord(0x0000);
    csHigh();

    _spi->endTransaction();

    return response;
}

void ADS131M02::writeRegister(uint8_t addr, uint16_t value) {
    uint16_t cmd = ADS131M02_CMD_WREG | ((addr & 0x3F) << 7);

    _spi->beginTransaction(SPISettings(ADS131M02_SPI_SPEED, MSBFIRST, SPI_MODE1));
    csLow();
    spiTransferWord(cmd);
    spiTransferWord(value);
    spiTransferWord(0x0000);
    csHigh();
    _spi->endTransaction();
}

void ADS131M02::setGain(uint8_t channel, ads131m02_gain_t gain) {
    uint16_t gain_reg = readRegister(ADS131M02_REG_GAIN);

    if (channel == 0) {
        gain_reg = (gain_reg & 0xFFF8) | (gain & 0x07);
        _gain_ch0 = gain;
    } else if (channel == 1) {
        gain_reg = (gain_reg & 0xFF8F) | ((gain & 0x07) << 4);
        _gain_ch1 = gain;
    }

    writeRegister(ADS131M02_REG_GAIN, gain_reg);
}

ads131m02_gain_t ADS131M02::getGain(uint8_t channel) {
    return (channel == 0) ? _gain_ch0 : _gain_ch1;
}

void ADS131M02::setOSR(ads131m02_osr_t osr) {
    uint16_t clock_reg = readRegister(ADS131M02_REG_CLOCK);
    clock_reg = (clock_reg & 0xFFE3) | ((osr & 0x07) << 2);
    writeRegister(ADS131M02_REG_CLOCK, clock_reg);
}

void ADS131M02::setPowerMode(ads131m02_power_t mode) {
    uint16_t clock_reg = readRegister(ADS131M02_REG_CLOCK);
    clock_reg = (clock_reg & 0xFFFC) | (mode & 0x03);
    writeRegister(ADS131M02_REG_CLOCK, clock_reg);
}

void ADS131M02::enableChannel(uint8_t channel, bool enable) {
    uint16_t clock_reg = readRegister(ADS131M02_REG_CLOCK);

    if (channel == 0) {
        if (enable) clock_reg |= (1 << 8);
        else clock_reg &= ~(1 << 8);
    } else if (channel == 1) {
        if (enable) clock_reg |= (1 << 9);
        else clock_reg &= ~(1 << 9);
    }

    writeRegister(ADS131M02_REG_CLOCK, clock_reg);
}

void ADS131M02::setOffsetCal(uint8_t channel, int32_t offset) {
    uint8_t msb_reg = (channel == 0) ? ADS131M02_REG_CH0_OCAL_MSB : ADS131M02_REG_CH1_OCAL_MSB;
    uint8_t lsb_reg = (channel == 0) ? ADS131M02_REG_CH0_OCAL_LSB : ADS131M02_REG_CH1_OCAL_LSB;

    writeRegister(msb_reg, (offset >> 8) & 0xFFFF);
    writeRegister(lsb_reg, (offset & 0xFF) << 8);
}

int32_t ADS131M02::getOffsetCal(uint8_t channel) {
    uint8_t msb_reg = (channel == 0) ? ADS131M02_REG_CH0_OCAL_MSB : ADS131M02_REG_CH1_OCAL_MSB;
    uint8_t lsb_reg = (channel == 0) ? ADS131M02_REG_CH0_OCAL_LSB : ADS131M02_REG_CH1_OCAL_LSB;

    uint16_t msb = readRegister(msb_reg);
    uint16_t lsb = readRegister(lsb_reg);

    int32_t offset = ((int32_t)msb << 8) | ((lsb >> 8) & 0xFF);
    return signExtend24(offset);
}

void ADS131M02::setGainCal(uint8_t channel, uint32_t gain) {
    uint8_t msb_reg = (channel == 0) ? ADS131M02_REG_CH0_GCAL_MSB : ADS131M02_REG_CH1_GCAL_MSB;
    uint8_t lsb_reg = (channel == 0) ? ADS131M02_REG_CH0_GCAL_LSB : ADS131M02_REG_CH1_GCAL_LSB;

    writeRegister(msb_reg, (gain >> 8) & 0xFFFF);
    writeRegister(lsb_reg, (gain & 0xFF) << 8);
}

uint32_t ADS131M02::getGainCal(uint8_t channel) {
    uint8_t msb_reg = (channel == 0) ? ADS131M02_REG_CH0_GCAL_MSB : ADS131M02_REG_CH1_GCAL_MSB;
    uint8_t lsb_reg = (channel == 0) ? ADS131M02_REG_CH0_GCAL_LSB : ADS131M02_REG_CH1_GCAL_LSB;

    uint16_t msb = readRegister(msb_reg);
    uint16_t lsb = readRegister(lsb_reg);

    return ((uint32_t)msb << 8) | ((lsb >> 8) & 0xFF);
}

void ADS131M02::loadCalibration(const ads131m02_cal_t *cal) {
    if (cal == NULL) return;

    setOffsetCal(0, cal->offset_ch0);
    setOffsetCal(1, cal->offset_ch1);
    setGainCal(0, cal->gain_ch0);
    setGainCal(1, cal->gain_ch1);
}

void ADS131M02::saveCalibration(ads131m02_cal_t *cal) {
    if (cal == NULL) return;

    cal->offset_ch0 = getOffsetCal(0);
    cal->offset_ch1 = getOffsetCal(1);
    cal->gain_ch0 = getGainCal(0);
    cal->gain_ch1 = getGainCal(1);
}

void ADS131M02::standby(void) {
    _spi->beginTransaction(SPISettings(ADS131M02_SPI_SPEED, MSBFIRST, SPI_MODE1));
    csLow();
    spiTransferWord(ADS131M02_CMD_STANDBY);
    spiTransferWord(0x0000);
    spiTransferWord(0x0000);
    csHigh();
    _spi->endTransaction();
}

void ADS131M02::wakeup(void) {
    _spi->beginTransaction(SPISettings(ADS131M02_SPI_SPEED, MSBFIRST, SPI_MODE1));
    csLow();
    spiTransferWord(ADS131M02_CMD_WAKEUP);
    spiTransferWord(0x0000);
    spiTransferWord(0x0000);
    csHigh();
    _spi->endTransaction();
}

int32_t ADS131M02::toMicrovolts(int32_t raw, ads131m02_gain_t gain) {
    // Full scale = ±1.2V (internal reference)
    // 24-bit signed: ±8388607 counts
    static const uint8_t gain_values[] = {1, 2, 4, 8, 16, 32, 64, 128};
    uint8_t gain_mult = gain_values[gain & 0x07];

    int64_t uv = ((int64_t)raw * 1200000LL) / ((int64_t)8388608 * gain_mult);
    return (int32_t)uv;
}

float ADS131M02::toMillivolts(int32_t raw, ads131m02_gain_t gain) {
    return (float)toMicrovolts(raw, gain) / 1000.0f;
}

// Private methods

void ADS131M02::csLow(void) {
    digitalWrite(_cs_pin, LOW);
}

void ADS131M02::csHigh(void) {
    digitalWrite(_cs_pin, HIGH);
}

uint16_t ADS131M02::spiTransferWord(uint16_t data) {
    uint16_t result = _spi->transfer((data >> 8) & 0xFF) << 8;
    result |= _spi->transfer(data & 0xFF);
    return result;
}

int32_t ADS131M02::signExtend24(uint32_t value) {
    if (value & 0x800000) {
        return (int32_t)(value | 0xFF000000);
    }
    return (int32_t)value;
}
