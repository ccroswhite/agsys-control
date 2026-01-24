/**
 * @file ads131m0x.h
 * @brief ADS131M0x 24-bit Delta-Sigma ADC Driver
 * 
 * Complete driver for Texas Instruments ADS131M0x family of simultaneous-sampling,
 * 24-bit, delta-sigma ADCs. Supports all variants:
 *   - ADS131M01: 1 channel
 *   - ADS131M02: 2 channels
 *   - ADS131M03: 3 channels
 *   - ADS131M04: 4 channels
 *   - ADS131M06: 6 channels
 *   - ADS131M08: 8 channels
 * 
 * Features:
 *   - 24-bit resolution
 *   - Up to 32 kSPS per channel (simultaneous sampling)
 *   - Programmable gain (1, 2, 4, 8, 16, 32, 64, 128)
 *   - Internal 1.2V reference
 *   - Global-chop mode for offset drift reduction
 *   - Per-channel offset and gain calibration
 *   - Per-channel phase delay calibration
 *   - CRC validation on communications
 *   - Platform-agnostic SPI abstraction
 * 
 * @note This driver is platform-agnostic. You must provide SPI transfer
 *       and GPIO functions via the ads131m0x_hal_t interface.
 * 
 * @author AgSys
 * @date 2026
 * @license MIT
 * 
 * Copyright (c) 2026 AgSys
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

#ifndef ADS131M0X_H
#define ADS131M0X_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * DEVICE SELECTION (compile-time)
 * ========================================================================== */

/**
 * Define ONE of the following before including this header to select your device:
 *   #define ADS131M0X_DEVICE_M01   // 1-channel
 *   #define ADS131M0X_DEVICE_M02   // 2-channel
 *   #define ADS131M0X_DEVICE_M03   // 3-channel
 *   #define ADS131M0X_DEVICE_M04   // 4-channel
 *   #define ADS131M0X_DEVICE_M06   // 6-channel
 *   #define ADS131M0X_DEVICE_M08   // 8-channel
 *
 * Example:
 *   #define ADS131M0X_DEVICE_M02
 *   #include "ads131m0x.h"
 */

#if defined(ADS131M0X_DEVICE_M01)
    #define ADS131M0X_NUM_CHANNELS  1
    #define ADS131M0X_DEVICE_ID     0x01
    #define ADS131M0X_DEVICE_NAME   "ADS131M01"
#elif defined(ADS131M0X_DEVICE_M02)
    #define ADS131M0X_NUM_CHANNELS  2
    #define ADS131M0X_DEVICE_ID     0x02
    #define ADS131M0X_DEVICE_NAME   "ADS131M02"
#elif defined(ADS131M0X_DEVICE_M03)
    #define ADS131M0X_NUM_CHANNELS  3
    #define ADS131M0X_DEVICE_ID     0x03
    #define ADS131M0X_DEVICE_NAME   "ADS131M03"
#elif defined(ADS131M0X_DEVICE_M04)
    #define ADS131M0X_NUM_CHANNELS  4
    #define ADS131M0X_DEVICE_ID     0x04
    #define ADS131M0X_DEVICE_NAME   "ADS131M04"
#elif defined(ADS131M0X_DEVICE_M06)
    #define ADS131M0X_NUM_CHANNELS  6
    #define ADS131M0X_DEVICE_ID     0x06
    #define ADS131M0X_DEVICE_NAME   "ADS131M06"
#elif defined(ADS131M0X_DEVICE_M08)
    #define ADS131M0X_NUM_CHANNELS  8
    #define ADS131M0X_DEVICE_ID     0x08
    #define ADS131M0X_DEVICE_NAME   "ADS131M08"
#else
    #error "No ADS131M0x device selected. Define one of: ADS131M0X_DEVICE_M01, ADS131M0X_DEVICE_M02, ADS131M0X_DEVICE_M03, ADS131M0X_DEVICE_M04, ADS131M0X_DEVICE_M06, ADS131M0X_DEVICE_M08"
#endif

/* Devices with more than 4 channels need GAIN2 register */
#if ADS131M0X_NUM_CHANNELS > 4
    #define ADS131M0X_HAS_GAIN2     1
#else
    #define ADS131M0X_HAS_GAIN2     0
#endif

/* ==========================================================================
 * REGISTER ADDRESSES
 * ========================================================================== */

#define ADS131M0X_REG_ID            0x00
#define ADS131M0X_REG_STATUS        0x01
#define ADS131M0X_REG_MODE          0x02
#define ADS131M0X_REG_CLOCK         0x03
#define ADS131M0X_REG_GAIN1         0x04    /**< Gain for CH0-CH3 */
#define ADS131M0X_REG_GAIN2         0x05    /**< Gain for CH4-CH7 (M06/M08 only) */
#define ADS131M0X_REG_CFG           0x06
#define ADS131M0X_REG_THRSHLD_MSB   0x07
#define ADS131M0X_REG_THRSHLD_LSB   0x08

/* Per-channel registers (base addresses, add 5*channel for CHn) */
#define ADS131M0X_REG_CH0_CFG       0x09
#define ADS131M0X_REG_CH0_OCAL_MSB  0x0A
#define ADS131M0X_REG_CH0_OCAL_LSB  0x0B
#define ADS131M0X_REG_CH0_GCAL_MSB  0x0C
#define ADS131M0X_REG_CH0_GCAL_LSB  0x0D

/* Channel register stride (5 registers per channel) */
#define ADS131M0X_CH_REG_STRIDE     5

/* Register map CRC */
#define ADS131M0X_REG_REGMAP_CRC    0x3E

/* ==========================================================================
 * COMMANDS
 * ========================================================================== */

#define ADS131M0X_CMD_NULL          0x0000
#define ADS131M0X_CMD_RESET         0x0011
#define ADS131M0X_CMD_STANDBY       0x0022
#define ADS131M0X_CMD_WAKEUP        0x0033
#define ADS131M0X_CMD_LOCK          0x0555
#define ADS131M0X_CMD_UNLOCK        0x0655
#define ADS131M0X_CMD_RREG          0xA000  /**< Read register: 0xA000 | (addr << 7) */
#define ADS131M0X_CMD_WREG          0x6000  /**< Write register: 0x6000 | (addr << 7) */

/* ==========================================================================
 * CONFIGURATION REGISTER BITS
 * ========================================================================== */

/* MODE register bits */
#define ADS131M0X_MODE_REG_CRC_EN   (1 << 13)   /**< Register map CRC enable */
#define ADS131M0X_MODE_RX_CRC_EN    (1 << 12)   /**< Input CRC enable */
#define ADS131M0X_MODE_CRC_TYPE     (1 << 11)   /**< 0=CCITT, 1=ANSI */
#define ADS131M0X_MODE_RESET        (1 << 10)   /**< Reset bit */
#define ADS131M0X_MODE_WLENGTH_16   (0 << 8)    /**< 16-bit word length */
#define ADS131M0X_MODE_WLENGTH_24   (1 << 8)    /**< 24-bit word length */
#define ADS131M0X_MODE_WLENGTH_32   (2 << 8)    /**< 32-bit word length (with CRC) */
#define ADS131M0X_MODE_WLENGTH_MASK (3 << 8)
#define ADS131M0X_MODE_TIMEOUT      (1 << 4)    /**< SPI timeout enable */
#define ADS131M0X_MODE_DRDY_SEL     (0x03 << 2) /**< DRDY source selection */
#define ADS131M0X_MODE_DRDY_HiZ     (1 << 1)    /**< DRDY high-impedance */
#define ADS131M0X_MODE_DRDY_FMT     (1 << 0)    /**< 0=logic low, 1=pulse */

/* CLOCK register bits */
#define ADS131M0X_CLK_CH7_EN        (1 << 15)
#define ADS131M0X_CLK_CH6_EN        (1 << 14)
#define ADS131M0X_CLK_CH5_EN        (1 << 13)
#define ADS131M0X_CLK_CH4_EN        (1 << 12)
#define ADS131M0X_CLK_CH3_EN        (1 << 11)
#define ADS131M0X_CLK_CH2_EN        (1 << 10)
#define ADS131M0X_CLK_CH1_EN        (1 << 9)
#define ADS131M0X_CLK_CH0_EN        (1 << 8)
#define ADS131M0X_CLK_OSR_MASK      (0x07 << 2)
#define ADS131M0X_CLK_PWR_MASK      (0x03 << 0)

/* CFG register bits */
#define ADS131M0X_CFG_GC_DLY_MASK   (0x0F << 9)  /**< Global-chop delay */
#define ADS131M0X_CFG_GC_EN         (1 << 8)     /**< Global-chop enable */
#define ADS131M0X_CFG_CD_ALLCH      (1 << 7)     /**< Current-detect all channels */
#define ADS131M0X_CFG_CD_NUM_MASK   (0x07 << 4)  /**< Current-detect number */
#define ADS131M0X_CFG_CD_LEN_MASK   (0x07 << 1)  /**< Current-detect length */
#define ADS131M0X_CFG_CD_EN         (1 << 0)     /**< Current-detect enable */

/* CHn_CFG register bits */
#define ADS131M0X_CHCFG_PHASE_MASK  0x03FF       /**< Phase delay [9:0] */
#define ADS131M0X_CHCFG_MUX_MASK    (0x03 << 10) /**< Input mux selection */

/* STATUS register bits */
#define ADS131M0X_STATUS_LOCK       (1 << 15)    /**< SPI locked */
#define ADS131M0X_STATUS_F_RESYNC   (1 << 14)    /**< Resync occurred */
#define ADS131M0X_STATUS_REG_MAP    (1 << 13)    /**< Register map CRC error */
#define ADS131M0X_STATUS_CRC_ERR    (1 << 12)    /**< Input CRC error */
#define ADS131M0X_STATUS_CRC_TYPE   (1 << 11)    /**< CRC type used */
#define ADS131M0X_STATUS_RESET      (1 << 10)    /**< Reset occurred */
#define ADS131M0X_STATUS_WLENGTH    (0x03 << 8)  /**< Word length */
#define ADS131M0X_STATUS_DRDY7      (1 << 7)     /**< CH7 data ready */
#define ADS131M0X_STATUS_DRDY6      (1 << 6)     /**< CH6 data ready */
#define ADS131M0X_STATUS_DRDY5      (1 << 5)     /**< CH5 data ready */
#define ADS131M0X_STATUS_DRDY4      (1 << 4)     /**< CH4 data ready */
#define ADS131M0X_STATUS_DRDY3      (1 << 3)     /**< CH3 data ready */
#define ADS131M0X_STATUS_DRDY2      (1 << 2)     /**< CH2 data ready */
#define ADS131M0X_STATUS_DRDY1      (1 << 1)     /**< CH1 data ready */
#define ADS131M0X_STATUS_DRDY0      (1 << 0)     /**< CH0 data ready */

/* Calibration constants */
#define ADS131M0X_OCAL_DEFAULT      0x000000     /**< Default offset: 0 */
#define ADS131M0X_GCAL_DEFAULT      0x800000     /**< Default gain: 1.0 (2^23) */

/* ==========================================================================
 * ENUMERATIONS
 * ========================================================================== */

/**
 * @brief Oversampling ratio (determines sample rate)
 * 
 * Sample rates assume 8.192 MHz clock (fCLKIN)
 */
typedef enum {
    ADS131M0X_OSR_128   = 0,    /**< 32 kSPS */
    ADS131M0X_OSR_256   = 1,    /**< 16 kSPS */
    ADS131M0X_OSR_512   = 2,    /**< 8 kSPS */
    ADS131M0X_OSR_1024  = 3,    /**< 4 kSPS */
    ADS131M0X_OSR_2048  = 4,    /**< 2 kSPS */
    ADS131M0X_OSR_4096  = 5,    /**< 1 kSPS */
    ADS131M0X_OSR_8192  = 6,    /**< 500 SPS */
    ADS131M0X_OSR_16384 = 7,    /**< 250 SPS */
} ads131m0x_osr_t;

/**
 * @brief Programmable gain amplifier settings
 */
typedef enum {
    ADS131M0X_GAIN_1X   = 0,
    ADS131M0X_GAIN_2X   = 1,
    ADS131M0X_GAIN_4X   = 2,
    ADS131M0X_GAIN_8X   = 3,
    ADS131M0X_GAIN_16X  = 4,
    ADS131M0X_GAIN_32X  = 5,
    ADS131M0X_GAIN_64X  = 6,
    ADS131M0X_GAIN_128X = 7,
} ads131m0x_gain_t;

/**
 * @brief Power mode settings
 */
typedef enum {
    ADS131M0X_PWR_VERY_LOW  = 0,    /**< Very low power */
    ADS131M0X_PWR_LOW       = 1,    /**< Low power */
    ADS131M0X_PWR_HIGH_RES  = 2,    /**< High resolution */
} ads131m0x_power_t;

/**
 * @brief Input multiplexer settings
 */
typedef enum {
    ADS131M0X_MUX_NORMAL  = 0,  /**< Normal differential input */
    ADS131M0X_MUX_SHORTED = 1,  /**< Inputs shorted (for offset cal) */
    ADS131M0X_MUX_POS_DC  = 2,  /**< Positive DC test signal */
    ADS131M0X_MUX_NEG_DC  = 3,  /**< Negative DC test signal */
} ads131m0x_mux_t;

/**
 * @brief Global-chop delay settings
 */
typedef enum {
    ADS131M0X_GC_DLY_2      = 0,    /**< 2 fMOD periods */
    ADS131M0X_GC_DLY_4      = 1,    /**< 4 fMOD periods */
    ADS131M0X_GC_DLY_8      = 2,    /**< 8 fMOD periods */
    ADS131M0X_GC_DLY_16     = 3,    /**< 16 fMOD periods */
    ADS131M0X_GC_DLY_32     = 4,    /**< 32 fMOD periods */
    ADS131M0X_GC_DLY_64     = 5,    /**< 64 fMOD periods */
    ADS131M0X_GC_DLY_128    = 6,    /**< 128 fMOD periods */
    ADS131M0X_GC_DLY_256    = 7,    /**< 256 fMOD periods */
    ADS131M0X_GC_DLY_512    = 8,    /**< 512 fMOD periods */
    ADS131M0X_GC_DLY_1024   = 9,    /**< 1024 fMOD periods */
    ADS131M0X_GC_DLY_2048   = 10,   /**< 2048 fMOD periods */
    ADS131M0X_GC_DLY_4096   = 11,   /**< 4096 fMOD periods */
    ADS131M0X_GC_DLY_8192   = 12,   /**< 8192 fMOD periods */
    ADS131M0X_GC_DLY_16384  = 13,   /**< 16384 fMOD periods */
    ADS131M0X_GC_DLY_32768  = 14,   /**< 32768 fMOD periods */
    ADS131M0X_GC_DLY_65536  = 15,   /**< 65536 fMOD periods */
} ads131m0x_gc_delay_t;

/**
 * @brief Word length for SPI communication
 */
typedef enum {
    ADS131M0X_WORD_16BIT = 0,   /**< 16-bit words */
    ADS131M0X_WORD_24BIT = 1,   /**< 24-bit words (default) */
    ADS131M0X_WORD_32BIT = 2,   /**< 32-bit words (with CRC) */
} ads131m0x_word_length_t;

/* ==========================================================================
 * HAL (HARDWARE ABSTRACTION LAYER)
 * ========================================================================== */

/**
 * @brief SPI transfer function pointer type
 * 
 * Performs a full-duplex SPI transfer with CS handling.
 * 
 * @param tx_buf Transmit buffer
 * @param rx_buf Receive buffer
 * @param len Number of bytes to transfer
 * @param user_data User-provided context pointer
 * @return true on success, false on error
 */
typedef bool (*ads131m0x_spi_xfer_fn)(const uint8_t *tx_buf, uint8_t *rx_buf,
                                       size_t len, void *user_data);

/**
 * @brief GPIO read function pointer type
 * 
 * @param pin Pin identifier (platform-specific)
 * @param user_data User-provided context pointer
 * @return true if pin is high, false if low
 */
typedef bool (*ads131m0x_gpio_read_fn)(uint8_t pin, void *user_data);

/**
 * @brief GPIO write function pointer type
 * 
 * @param pin Pin identifier (platform-specific)
 * @param value true for high, false for low
 * @param user_data User-provided context pointer
 */
typedef void (*ads131m0x_gpio_write_fn)(uint8_t pin, bool value, void *user_data);

/**
 * @brief Delay function pointer type
 * 
 * @param ms Milliseconds to delay
 * @param user_data User-provided context pointer
 */
typedef void (*ads131m0x_delay_fn)(uint32_t ms, void *user_data);

/**
 * @brief Hardware abstraction layer interface
 */
typedef struct {
    ads131m0x_spi_xfer_fn   spi_transfer;   /**< SPI transfer function */
    ads131m0x_gpio_read_fn  gpio_read;      /**< GPIO read function */
    ads131m0x_gpio_write_fn gpio_write;     /**< GPIO write function */
    ads131m0x_delay_fn      delay_ms;       /**< Millisecond delay function */
    void                   *user_data;      /**< User context passed to HAL functions */
} ads131m0x_hal_t;

/* ==========================================================================
 * DATA TYPES
 * ========================================================================== */

/**
 * @brief Sample data from ADC
 */
typedef struct {
    int32_t  ch[ADS131M0X_NUM_CHANNELS];    /**< Channel data (24-bit signed, sign-extended) */
    uint16_t status;                         /**< Status word */
    bool     valid;                          /**< Data valid flag */
    bool     crc_error;                      /**< CRC error detected */
} ads131m0x_sample_t;

/**
 * @brief Device configuration
 */
typedef struct {
    uint8_t                 sync_reset_pin; /**< SYNC/RESET pin */
    uint8_t                 drdy_pin;       /**< DRDY pin */
    ads131m0x_osr_t         osr;            /**< Oversampling ratio */
    ads131m0x_power_t       power_mode;     /**< Power mode */
    ads131m0x_word_length_t word_length;    /**< SPI word length */
    ads131m0x_gain_t        gain[ADS131M0X_NUM_CHANNELS];  /**< Per-channel gain */
    bool                    enable_crc;     /**< Enable CRC on communications */
} ads131m0x_config_t;

/**
 * @brief Device context
 */
typedef struct {
    ads131m0x_hal_t         hal;            /**< HAL interface */
    uint8_t                 sync_reset_pin; /**< SYNC/RESET pin */
    uint8_t                 drdy_pin;       /**< DRDY pin */
    ads131m0x_osr_t         osr;            /**< Current OSR setting */
    ads131m0x_power_t       power_mode;     /**< Current power mode */
    ads131m0x_word_length_t word_length;    /**< Current word length */
    ads131m0x_gain_t        gain[ADS131M0X_NUM_CHANNELS];  /**< Per-channel gain */
    uint16_t                device_id_raw;  /**< Raw device ID register (for verification) */
    uint8_t                 frame_size;     /**< SPI frame size in bytes */
    bool                    crc_enabled;    /**< CRC enabled flag */
    bool                    initialized;    /**< Device initialized flag */
} ads131m0x_ctx_t;

/* ==========================================================================
 * DEFAULT CONFIGURATION
 * ========================================================================== */

/**
 * @brief Default configuration macro
 * 
 * Usage:
 *   #define ADS131M0X_DEVICE_M02
 *   #include "ads131m0x.h"
 *   ...
 *   ads131m0x_config_t config = ADS131M0X_CONFIG_DEFAULT();
 */
#define ADS131M0X_CONFIG_DEFAULT() { \
    .sync_reset_pin = 0, \
    .drdy_pin = 0, \
    .osr = ADS131M0X_OSR_4096, \
    .power_mode = ADS131M0X_PWR_HIGH_RES, \
    .word_length = ADS131M0X_WORD_24BIT, \
    .gain = {0}, \
    .enable_crc = false \
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize ADS131M0x device
 * 
 * @param ctx Device context to initialize
 * @param hal Hardware abstraction layer interface
 * @param config Device configuration
 * @return true on success, false on error
 */
bool ads131m0x_init(ads131m0x_ctx_t *ctx, const ads131m0x_hal_t *hal,
                    const ads131m0x_config_t *config);

/**
 * @brief Reset device (hardware + software reset)
 * 
 * @param ctx Device context
 * @return true on success
 */
bool ads131m0x_reset(ads131m0x_ctx_t *ctx);

/**
 * @brief Verify device ID matches expected
 * 
 * @param ctx Device context
 * @return true if device ID matches ADS131M0X_DEVICE_ID
 */
bool ads131m0x_verify_device_id(ads131m0x_ctx_t *ctx);

/* ==========================================================================
 * SAMPLING
 * ========================================================================== */

/**
 * @brief Read sample from all enabled channels
 * 
 * @param ctx Device context
 * @param sample Output sample structure
 * @return true on success
 */
bool ads131m0x_read_sample(ads131m0x_ctx_t *ctx, ads131m0x_sample_t *sample);

/**
 * @brief Check if data is ready (DRDY pin low)
 * 
 * @param ctx Device context
 * @return true if data ready
 */
bool ads131m0x_data_ready(ads131m0x_ctx_t *ctx);

/**
 * @brief Wait for data ready with timeout
 * 
 * @param ctx Device context
 * @param timeout_ms Timeout in milliseconds
 * @return true if data ready, false if timeout
 */
bool ads131m0x_wait_data_ready(ads131m0x_ctx_t *ctx, uint32_t timeout_ms);

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

/**
 * @brief Set oversampling ratio (sample rate)
 * 
 * @param ctx Device context
 * @param osr Oversampling ratio
 * @return true on success
 */
bool ads131m0x_set_osr(ads131m0x_ctx_t *ctx, ads131m0x_osr_t osr);

/**
 * @brief Set channel gain
 * 
 * @param ctx Device context
 * @param channel Channel number (0 to num_channels-1)
 * @param gain Gain setting
 * @return true on success
 */
bool ads131m0x_set_gain(ads131m0x_ctx_t *ctx, uint8_t channel, ads131m0x_gain_t gain);

/**
 * @brief Set power mode
 * 
 * @param ctx Device context
 * @param mode Power mode
 * @return true on success
 */
bool ads131m0x_set_power_mode(ads131m0x_ctx_t *ctx, ads131m0x_power_t mode);

/**
 * @brief Enable or disable a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param enable true to enable, false to disable
 * @return true on success
 */
bool ads131m0x_set_channel_enable(ads131m0x_ctx_t *ctx, uint8_t channel, bool enable);

/**
 * @brief Set input multiplexer for a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param mux Multiplexer setting
 * @return true on success
 */
bool ads131m0x_set_input_mux(ads131m0x_ctx_t *ctx, uint8_t channel, ads131m0x_mux_t mux);

/* ==========================================================================
 * POWER MANAGEMENT
 * ========================================================================== */

/**
 * @brief Enter standby mode (low power)
 * 
 * @param ctx Device context
 * @return true on success
 */
bool ads131m0x_standby(ads131m0x_ctx_t *ctx);

/**
 * @brief Wake from standby mode
 * 
 * @param ctx Device context
 * @return true on success
 */
bool ads131m0x_wakeup(ads131m0x_ctx_t *ctx);

/* ==========================================================================
 * CALIBRATION
 * ========================================================================== */

/**
 * @brief Set offset calibration for a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param offset 24-bit signed offset value (subtracted from raw data)
 * @return true on success
 */
bool ads131m0x_set_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, int32_t offset);

/**
 * @brief Get offset calibration for a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param offset Output offset value
 * @return true on success
 */
bool ads131m0x_get_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, int32_t *offset);

/**
 * @brief Set gain calibration for a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param gain_cal 24-bit unsigned gain value (0x800000 = 1.0)
 * @return true on success
 */
bool ads131m0x_set_gain_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint32_t gain_cal);

/**
 * @brief Get gain calibration for a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param gain_cal Output gain calibration value
 * @return true on success
 */
bool ads131m0x_get_gain_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint32_t *gain_cal);

/**
 * @brief Perform automatic offset calibration (inputs shorted)
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param num_samples Number of samples to average (recommend 16-64)
 * @return true on success
 */
bool ads131m0x_auto_offset_cal(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t num_samples);

/**
 * @brief Reset calibration to defaults for a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @return true on success
 */
bool ads131m0x_reset_calibration(ads131m0x_ctx_t *ctx, uint8_t channel);

/**
 * @brief Set phase delay for a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param phase_delay 10-bit phase delay value (0-1023)
 * @return true on success
 * @note Each step is 1/fCLKIN (122ns at 8.192MHz)
 */
bool ads131m0x_set_phase_delay(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t phase_delay);

/**
 * @brief Get phase delay for a channel
 * 
 * @param ctx Device context
 * @param channel Channel number
 * @param phase_delay Output phase delay value
 * @return true on success
 */
bool ads131m0x_get_phase_delay(ads131m0x_ctx_t *ctx, uint8_t channel, uint16_t *phase_delay);

/* ==========================================================================
 * GLOBAL-CHOP MODE
 * ========================================================================== */

/**
 * @brief Enable global-chop mode
 * 
 * Global-chop reduces offset drift by periodically swapping input polarity.
 * 
 * @param ctx Device context
 * @param delay Chop delay setting
 * @return true on success
 */
bool ads131m0x_enable_global_chop(ads131m0x_ctx_t *ctx, ads131m0x_gc_delay_t delay);

/**
 * @brief Disable global-chop mode
 * 
 * @param ctx Device context
 * @return true on success
 */
bool ads131m0x_disable_global_chop(ads131m0x_ctx_t *ctx);

/**
 * @brief Check if global-chop is enabled
 * 
 * @param ctx Device context
 * @return true if enabled
 */
bool ads131m0x_is_global_chop_enabled(ads131m0x_ctx_t *ctx);

/* ==========================================================================
 * CRC
 * ========================================================================== */

/**
 * @brief Enable CRC on communications
 * 
 * @param ctx Device context
 * @param enable_input Enable CRC checking on input data
 * @param enable_output Enable CRC on output data
 * @param use_ccitt Use CCITT polynomial (true) or ANSI (false)
 * @return true on success
 */
bool ads131m0x_enable_crc(ads131m0x_ctx_t *ctx, bool enable_input, 
                          bool enable_output, bool use_ccitt);

/**
 * @brief Disable CRC on communications
 * 
 * @param ctx Device context
 * @return true on success
 */
bool ads131m0x_disable_crc(ads131m0x_ctx_t *ctx);

/**
 * @brief Read register map CRC
 * 
 * @param ctx Device context
 * @param crc Output CRC value
 * @return true on success
 */
bool ads131m0x_read_regmap_crc(ads131m0x_ctx_t *ctx, uint16_t *crc);

/* ==========================================================================
 * REGISTER ACCESS
 * ========================================================================== */

/**
 * @brief Read register
 * 
 * @param ctx Device context
 * @param reg Register address
 * @param value Output value
 * @return true on success
 */
bool ads131m0x_read_reg(ads131m0x_ctx_t *ctx, uint8_t reg, uint16_t *value);

/**
 * @brief Write register
 * 
 * @param ctx Device context
 * @param reg Register address
 * @param value Value to write
 * @return true on success
 */
bool ads131m0x_write_reg(ads131m0x_ctx_t *ctx, uint8_t reg, uint16_t value);

/* ==========================================================================
 * UTILITY FUNCTIONS
 * ========================================================================== */

/**
 * @brief Get sample rate in Hz for given OSR
 * 
 * @param osr Oversampling ratio
 * @return Sample rate in Hz (assumes 8.192 MHz clock)
 */
uint32_t ads131m0x_get_sample_rate(ads131m0x_osr_t osr);

/**
 * @brief Convert raw ADC value to voltage
 * 
 * @param raw Raw 24-bit signed value
 * @param gain Gain setting
 * @param vref Reference voltage (typically 1.2V internal)
 * @return Voltage in volts
 */
float ads131m0x_to_voltage(int32_t raw, ads131m0x_gain_t gain, float vref);

/**
 * @brief Get gain multiplier value
 * 
 * @param gain Gain setting
 * @return Gain multiplier (1, 2, 4, 8, 16, 32, 64, or 128)
 */
uint8_t ads131m0x_get_gain_multiplier(ads131m0x_gain_t gain);

/**
 * @brief Get device name string
 * 
 * @return String name (e.g., "ADS131M02") - compile-time constant
 */
static inline const char* ads131m0x_get_device_name(void) { return ADS131M0X_DEVICE_NAME; }

/**
 * @brief Get number of channels
 * 
 * @return Number of channels - compile-time constant
 */
static inline uint8_t ads131m0x_get_num_channels(void) { return ADS131M0X_NUM_CHANNELS; }

#ifdef __cplusplus
}
#endif

#endif /* ADS131M0X_H */
