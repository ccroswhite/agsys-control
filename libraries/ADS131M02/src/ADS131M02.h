/*!
 * @file ADS131M02.h
 *
 * Arduino library for the Texas Instruments ADS131M02
 * 24-bit, 2-channel, simultaneous sampling delta-sigma ADC
 *
 * Features:
 * - 2 differential input channels with simultaneous sampling
 * - Programmable gain: 1, 2, 4, 8, 16, 32, 64, 128x
 * - Data rates from 250 SPS to 32 kSPS
 * - Built-in offset and gain calibration registers
 * - Internal 1.2V reference
 *
 * Written by AgSys for the open source community.
 *
 * MIT License - see LICENSE file for details
 */

#ifndef ADS131M02_H
#define ADS131M02_H

#include <Arduino.h>
#include <SPI.h>

// Register Addresses
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
#define ADS131M02_CMD_RREG          0xA000
#define ADS131M02_CMD_WREG          0x6000

// Device ID
#define ADS131M02_ID_VALUE          0x22

/*!
 * @brief PGA Gain settings
 */
typedef enum {
    ADS131M02_GAIN_1   = 0,   ///< Gain = 1
    ADS131M02_GAIN_2   = 1,   ///< Gain = 2
    ADS131M02_GAIN_4   = 2,   ///< Gain = 4
    ADS131M02_GAIN_8   = 3,   ///< Gain = 8
    ADS131M02_GAIN_16  = 4,   ///< Gain = 16
    ADS131M02_GAIN_32  = 5,   ///< Gain = 32
    ADS131M02_GAIN_64  = 6,   ///< Gain = 64
    ADS131M02_GAIN_128 = 7    ///< Gain = 128
} ads131m02_gain_t;

/*!
 * @brief Oversampling Ratio (OSR) - determines data rate
 */
typedef enum {
    ADS131M02_OSR_128   = 0,  ///< 32 kSPS
    ADS131M02_OSR_256   = 1,  ///< 16 kSPS
    ADS131M02_OSR_512   = 2,  ///< 8 kSPS
    ADS131M02_OSR_1024  = 3,  ///< 4 kSPS
    ADS131M02_OSR_2048  = 4,  ///< 2 kSPS
    ADS131M02_OSR_4096  = 5,  ///< 1 kSPS
    ADS131M02_OSR_8192  = 6,  ///< 500 SPS
    ADS131M02_OSR_16384 = 7   ///< 250 SPS
} ads131m02_osr_t;

/*!
 * @brief Power mode settings
 */
typedef enum {
    ADS131M02_POWER_VLP = 0,  ///< Very low power
    ADS131M02_POWER_LP  = 1,  ///< Low power
    ADS131M02_POWER_HR  = 2   ///< High resolution (recommended)
} ads131m02_power_t;

/*!
 * @brief ADC data structure for both channels
 */
typedef struct {
    int32_t ch0;        ///< Channel 0 raw value (24-bit signed)
    int32_t ch1;        ///< Channel 1 raw value (24-bit signed)
    uint16_t status;    ///< Status word from device
    bool valid;         ///< True if data was successfully read
} ads131m02_data_t;

/*!
 * @brief Calibration data structure
 */
typedef struct {
    int32_t offset_ch0;   ///< Channel 0 offset calibration
    int32_t offset_ch1;   ///< Channel 1 offset calibration
    uint32_t gain_ch0;    ///< Channel 0 gain calibration (1.0 = 0x800000)
    uint32_t gain_ch1;    ///< Channel 1 gain calibration (1.0 = 0x800000)
} ads131m02_cal_t;

/*!
 * @brief ADS131M02 driver class
 */
class ADS131M02 {
public:
    /*!
     * @brief Constructor
     * @param spi Pointer to SPI instance (default: &SPI)
     */
    ADS131M02(SPIClass *spi = &SPI);

    /*!
     * @brief Initialize the ADC
     * @param cs_pin Chip select pin
     * @param drdy_pin Data ready pin
     * @param reset_pin Reset/sync pin (optional, -1 to disable)
     * @return true on success, false if device not found
     */
    bool begin(int8_t cs_pin, int8_t drdy_pin, int8_t reset_pin = -1);

    /*!
     * @brief Reset the device
     */
    void reset(void);

    /*!
     * @brief Read device ID
     * @return Device ID (0x22 for ADS131M02)
     */
    uint8_t readID(void);

    /*!
     * @brief Check if new data is ready
     * @return true if DRDY pin is low
     */
    bool dataReady(void);

    /*!
     * @brief Read ADC data from both channels
     * @param data Pointer to data structure to fill
     * @return true if new data was available
     */
    bool readData(ads131m02_data_t *data);

    /*!
     * @brief Read a single channel (convenience function)
     * @param channel Channel number (0 or 1)
     * @return Raw ADC value (24-bit signed)
     */
    int32_t readChannel(uint8_t channel);

    /*!
     * @brief Set PGA gain for a channel
     * @param channel Channel number (0 or 1)
     * @param gain Gain setting
     */
    void setGain(uint8_t channel, ads131m02_gain_t gain);

    /*!
     * @brief Get current gain setting for a channel
     * @param channel Channel number (0 or 1)
     * @return Current gain setting
     */
    ads131m02_gain_t getGain(uint8_t channel);

    /*!
     * @brief Set oversampling ratio (data rate)
     * @param osr Oversampling ratio
     */
    void setOSR(ads131m02_osr_t osr);

    /*!
     * @brief Set power mode
     * @param mode Power mode
     */
    void setPowerMode(ads131m02_power_t mode);

    /*!
     * @brief Enable or disable a channel
     * @param channel Channel number (0 or 1)
     * @param enable true to enable, false to disable
     */
    void enableChannel(uint8_t channel, bool enable);

    /*!
     * @brief Set offset calibration for a channel
     * @param channel Channel number (0 or 1)
     * @param offset Offset value (24-bit signed)
     */
    void setOffsetCal(uint8_t channel, int32_t offset);

    /*!
     * @brief Get offset calibration for a channel
     * @param channel Channel number (0 or 1)
     * @return Offset value
     */
    int32_t getOffsetCal(uint8_t channel);

    /*!
     * @brief Set gain calibration for a channel
     * @param channel Channel number (0 or 1)
     * @param gain Gain value (1.0 = 0x800000)
     */
    void setGainCal(uint8_t channel, uint32_t gain);

    /*!
     * @brief Get gain calibration for a channel
     * @param channel Channel number (0 or 1)
     * @return Gain value
     */
    uint32_t getGainCal(uint8_t channel);

    /*!
     * @brief Load calibration from structure
     * @param cal Pointer to calibration structure
     */
    void loadCalibration(const ads131m02_cal_t *cal);

    /*!
     * @brief Save current calibration to structure
     * @param cal Pointer to calibration structure
     */
    void saveCalibration(ads131m02_cal_t *cal);

    /*!
     * @brief Enter standby mode (low power)
     */
    void standby(void);

    /*!
     * @brief Wake from standby mode
     */
    void wakeup(void);

    /*!
     * @brief Read a register
     * @param addr Register address
     * @return Register value
     */
    uint16_t readRegister(uint8_t addr);

    /*!
     * @brief Write a register
     * @param addr Register address
     * @param value Value to write
     */
    void writeRegister(uint8_t addr, uint16_t value);

    /*!
     * @brief Convert raw ADC value to microvolts
     * @param raw Raw ADC value
     * @param gain Gain setting used
     * @return Voltage in microvolts
     */
    static int32_t toMicrovolts(int32_t raw, ads131m02_gain_t gain);

    /*!
     * @brief Convert raw ADC value to millivolts (float)
     * @param raw Raw ADC value
     * @param gain Gain setting used
     * @return Voltage in millivolts
     */
    static float toMillivolts(int32_t raw, ads131m02_gain_t gain);

private:
    SPIClass *_spi;
    int8_t _cs_pin;
    int8_t _drdy_pin;
    int8_t _reset_pin;
    ads131m02_gain_t _gain_ch0;
    ads131m02_gain_t _gain_ch1;
    bool _initialized;

    void csLow(void);
    void csHigh(void);
    uint16_t spiTransferWord(uint16_t data);
    static int32_t signExtend24(uint32_t value);
};

#endif // ADS131M02_H
