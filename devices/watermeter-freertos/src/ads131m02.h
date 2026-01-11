/**
 * @file ads131m02.h
 * @brief ADS131M02 24-bit Delta-Sigma ADC Driver
 * 
 * Texas Instruments ADS131M02 - 2-channel, 24-bit, 32kSPS delta-sigma ADC
 * Used for electrode signal acquisition in the magnetic flow meter.
 * 
 * Features:
 * - 24-bit resolution
 * - Up to 32 kSPS per channel
 * - Simultaneous sampling
 * - Programmable gain (1, 2, 4, 8, 16, 32, 64, 128)
 * - Internal reference
 */

#ifndef ADS131M02_H
#define ADS131M02_H

#include <stdint.h>
#include <stdbool.h>
#include "nrf_drv_spi.h"
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * REGISTER ADDRESSES
 * ========================================================================== */

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

/* ==========================================================================
 * COMMANDS
 * ========================================================================== */

#define ADS131M02_CMD_NULL          0x0000
#define ADS131M02_CMD_RESET         0x0011
#define ADS131M02_CMD_STANDBY       0x0022
#define ADS131M02_CMD_WAKEUP        0x0033
#define ADS131M02_CMD_LOCK          0x0555
#define ADS131M02_CMD_UNLOCK        0x0655
#define ADS131M02_CMD_RREG          0xA000  /* Read register: 0xA000 | (addr << 7) */
#define ADS131M02_CMD_WREG          0x6000  /* Write register: 0x6000 | (addr << 7) */

/* ==========================================================================
 * CONFIGURATION VALUES
 * ========================================================================== */

/* MODE register bits */
#define ADS131M02_MODE_REG_CRC_EN   (1 << 13)
#define ADS131M02_MODE_RX_CRC_EN    (1 << 12)
#define ADS131M02_MODE_CRC_TYPE     (1 << 11)  /* 0=CCITT, 1=ANSI */
#define ADS131M02_MODE_RESET        (1 << 10)
#define ADS131M02_MODE_WLENGTH_16   (0 << 8)
#define ADS131M02_MODE_WLENGTH_24   (1 << 8)
#define ADS131M02_MODE_WLENGTH_32   (2 << 8)
#define ADS131M02_MODE_TIMEOUT      (1 << 4)
#define ADS131M02_MODE_DRDY_SEL     (0 << 2)   /* DRDY on most lagging channel */
#define ADS131M02_MODE_DRDY_HiZ     (1 << 1)
#define ADS131M02_MODE_DRDY_FMT     (1 << 0)   /* 0=logic low, 1=pulse */

/* CLOCK register bits */
#define ADS131M02_CLK_CH1_EN        (1 << 9)
#define ADS131M02_CLK_CH0_EN        (1 << 8)
#define ADS131M02_CLK_OSR_128       (0 << 2)   /* 32 kSPS */
#define ADS131M02_CLK_OSR_256       (1 << 2)   /* 16 kSPS */
#define ADS131M02_CLK_OSR_512       (2 << 2)   /* 8 kSPS */
#define ADS131M02_CLK_OSR_1024      (3 << 2)   /* 4 kSPS */
#define ADS131M02_CLK_OSR_2048      (4 << 2)   /* 2 kSPS */
#define ADS131M02_CLK_OSR_4096      (5 << 2)   /* 1 kSPS */
#define ADS131M02_CLK_OSR_8192      (6 << 2)   /* 500 SPS */
#define ADS131M02_CLK_OSR_16384     (7 << 2)   /* 250 SPS */
#define ADS131M02_CLK_PWR_VLP       (0 << 0)   /* Very low power */
#define ADS131M02_CLK_PWR_LP        (1 << 0)   /* Low power */
#define ADS131M02_CLK_PWR_HR        (2 << 0)   /* High resolution */

/* GAIN register bits */
#define ADS131M02_GAIN_1            0
#define ADS131M02_GAIN_2            1
#define ADS131M02_GAIN_4            2
#define ADS131M02_GAIN_8            3
#define ADS131M02_GAIN_16           4
#define ADS131M02_GAIN_32           5
#define ADS131M02_GAIN_64           6
#define ADS131M02_GAIN_128          7

/* ==========================================================================
 * DATA TYPES
 * ========================================================================== */

typedef enum {
    ADS131M02_OSR_128   = 0,    /* 32 kSPS */
    ADS131M02_OSR_256   = 1,    /* 16 kSPS */
    ADS131M02_OSR_512   = 2,    /* 8 kSPS */
    ADS131M02_OSR_1024  = 3,    /* 4 kSPS */
    ADS131M02_OSR_2048  = 4,    /* 2 kSPS */
    ADS131M02_OSR_4096  = 5,    /* 1 kSPS */
    ADS131M02_OSR_8192  = 6,    /* 500 SPS */
    ADS131M02_OSR_16384 = 7,    /* 250 SPS */
} ads131m02_osr_t;

typedef enum {
    ADS131M02_GAIN_1X   = 0,
    ADS131M02_GAIN_2X   = 1,
    ADS131M02_GAIN_4X   = 2,
    ADS131M02_GAIN_8X   = 3,
    ADS131M02_GAIN_16X  = 4,
    ADS131M02_GAIN_32X  = 5,
    ADS131M02_GAIN_64X  = 6,
    ADS131M02_GAIN_128X = 7,
} ads131m02_gain_t;

typedef enum {
    ADS131M02_PWR_VERY_LOW  = 0,
    ADS131M02_PWR_LOW       = 1,
    ADS131M02_PWR_HIGH_RES  = 2,
} ads131m02_power_t;

typedef struct {
    int32_t ch0;        /* Channel 0 (electrode signal) - 24-bit signed */
    int32_t ch1;        /* Channel 1 (coil current sense) - 24-bit signed */
    uint16_t status;    /* Status word */
    bool valid;         /* Data valid flag */
} ads131m02_sample_t;

typedef struct {
    /* SPI configuration */
    uint8_t             spi_instance;
    uint8_t             cs_pin;
    uint8_t             drdy_pin;
    uint8_t             sync_pin;
    SemaphoreHandle_t   spi_mutex;
    
    /* ADC configuration */
    ads131m02_osr_t     osr;
    ads131m02_gain_t    gain_ch0;
    ads131m02_gain_t    gain_ch1;
    ads131m02_power_t   power_mode;
    
    /* State */
    bool                initialized;
    uint16_t            device_id;
    
    /* Callback for DRDY interrupt */
    void (*drdy_callback)(ads131m02_sample_t *sample, void *user_data);
    void *callback_user_data;
} ads131m02_ctx_t;

typedef struct {
    uint8_t             spi_instance;
    uint8_t             cs_pin;
    uint8_t             drdy_pin;
    uint8_t             sync_pin;
    SemaphoreHandle_t   spi_mutex;
    ads131m02_osr_t     osr;
    ads131m02_gain_t    gain_ch0;
    ads131m02_gain_t    gain_ch1;
    ads131m02_power_t   power_mode;
} ads131m02_config_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize ADS131M02
 * @param ctx ADC context
 * @param config Configuration
 * @return true on success
 */
bool ads131m02_init(ads131m02_ctx_t *ctx, const ads131m02_config_t *config);

/**
 * @brief Reset ADC
 * @param ctx ADC context
 * @return true on success
 */
bool ads131m02_reset(ads131m02_ctx_t *ctx);

/**
 * @brief Set oversampling ratio (sample rate)
 * @param ctx ADC context
 * @param osr Oversampling ratio
 * @return true on success
 */
bool ads131m02_set_osr(ads131m02_ctx_t *ctx, ads131m02_osr_t osr);

/**
 * @brief Set channel gain
 * @param ctx ADC context
 * @param channel Channel (0 or 1)
 * @param gain Gain setting
 * @return true on success
 */
bool ads131m02_set_gain(ads131m02_ctx_t *ctx, uint8_t channel, ads131m02_gain_t gain);

/**
 * @brief Set power mode
 * @param ctx ADC context
 * @param mode Power mode
 * @return true on success
 */
bool ads131m02_set_power_mode(ads131m02_ctx_t *ctx, ads131m02_power_t mode);

/**
 * @brief Read single sample (blocking)
 * @param ctx ADC context
 * @param sample Output sample
 * @return true on success
 */
bool ads131m02_read_sample(ads131m02_ctx_t *ctx, ads131m02_sample_t *sample);

/**
 * @brief Check if data is ready
 * @param ctx ADC context
 * @return true if DRDY is asserted
 */
bool ads131m02_data_ready(ads131m02_ctx_t *ctx);

/**
 * @brief Set DRDY callback
 * @param ctx ADC context
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void ads131m02_set_drdy_callback(ads131m02_ctx_t *ctx,
                                  void (*callback)(ads131m02_sample_t*, void*),
                                  void *user_data);

/**
 * @brief Enable DRDY interrupt
 * @param ctx ADC context
 */
void ads131m02_enable_drdy_interrupt(ads131m02_ctx_t *ctx);

/**
 * @brief Disable DRDY interrupt
 * @param ctx ADC context
 */
void ads131m02_disable_drdy_interrupt(ads131m02_ctx_t *ctx);

/**
 * @brief Enter standby mode (low power)
 * @param ctx ADC context
 * @return true on success
 */
bool ads131m02_standby(ads131m02_ctx_t *ctx);

/**
 * @brief Wake from standby
 * @param ctx ADC context
 * @return true on success
 */
bool ads131m02_wakeup(ads131m02_ctx_t *ctx);

/**
 * @brief Read register
 * @param ctx ADC context
 * @param reg Register address
 * @param value Output value
 * @return true on success
 */
bool ads131m02_read_reg(ads131m02_ctx_t *ctx, uint8_t reg, uint16_t *value);

/**
 * @brief Write register
 * @param ctx ADC context
 * @param reg Register address
 * @param value Value to write
 * @return true on success
 */
bool ads131m02_write_reg(ads131m02_ctx_t *ctx, uint8_t reg, uint16_t value);

/**
 * @brief Get sample rate in Hz for given OSR
 * @param osr Oversampling ratio
 * @return Sample rate in Hz
 */
uint32_t ads131m02_get_sample_rate(ads131m02_osr_t osr);

/**
 * @brief Convert raw ADC value to voltage
 * @param raw Raw 24-bit signed value
 * @param gain Gain setting
 * @param vref Reference voltage (typically 1.2V internal)
 * @return Voltage in volts
 */
float ads131m02_to_voltage(int32_t raw, ads131m02_gain_t gain, float vref);

#ifdef __cplusplus
}
#endif

#endif /* ADS131M02_H */
