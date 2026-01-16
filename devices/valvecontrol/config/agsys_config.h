/**
 * @file agsys_config.h
 * @brief Configuration for Valve Controller FreeRTOS
 */

#ifndef AGSYS_CONFIG_H
#define AGSYS_CONFIG_H

/* ==========================================================================
 * DEVICE CONFIGURATION
 * ========================================================================== */

#define AGSYS_DEVICE_TYPE           0x02  /* Valve Controller */

/* Firmware version */
#define AGSYS_FW_VERSION_MAJOR      1
#define AGSYS_FW_VERSION_MINOR      0
#define AGSYS_FW_VERSION_PATCH      0

/* ==========================================================================
 * DEBUG CONFIGURATION
 * ========================================================================== */

#define AGSYS_DEBUG_ENABLED         1
#define AGSYS_USE_NRF_LOG           0
#define AGSYS_USE_RTT               1

/* ==========================================================================
 * SPI CONFIGURATION
 * ========================================================================== */

#define AGSYS_SPI_INSTANCE          0
#define AGSYS_SPI_SCK_PIN           3
#define AGSYS_SPI_MOSI_PIN          4
#define AGSYS_SPI_MISO_PIN          5
#define AGSYS_SPI_DEFAULT_FREQ      NRF_SPIM_FREQ_4M

/* ==========================================================================
 * FRAM CONFIGURATION
 * ========================================================================== */

#define AGSYS_FRAM_CS_PIN           11

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

#define AGSYS_LORA_CS_PIN           8
#define AGSYS_LORA_RST_PIN          9
#define AGSYS_LORA_DIO0_PIN         10
#define AGSYS_LORA_FREQUENCY        915000000
#define AGSYS_LORA_SPREADING_FACTOR 10
#define AGSYS_LORA_BANDWIDTH        125000
#define AGSYS_LORA_TX_POWER         17

/* ==========================================================================
 * BLE CONFIGURATION
 * ========================================================================== */

#define AGSYS_BLE_NAME_PREFIX       "AgValve-"
#define AGSYS_BLE_ADV_INTERVAL_MS   1000

/* ==========================================================================
 * FREERTOS TASK CONFIGURATION
 * ========================================================================== */

#define AGSYS_TASK_STACK_DEFAULT    256
#define AGSYS_TASK_STACK_BLE        256
#define AGSYS_TASK_STACK_LORA       512

#define AGSYS_TASK_PRIORITY_IDLE    1
#define AGSYS_TASK_PRIORITY_LOW     2
#define AGSYS_TASK_PRIORITY_NORMAL  3
#define AGSYS_TASK_PRIORITY_HIGH    4

#endif /* AGSYS_CONFIG_H */
