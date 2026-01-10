/**
 * @file agsys_config.h
 * @brief Configuration template for AgSys FreeRTOS devices
 * 
 * Copy this file to your project directory as "agsys_config.h" and customize.
 */

#ifndef AGSYS_CONFIG_H
#define AGSYS_CONFIG_H

/* ==========================================================================
 * DEVICE CONFIGURATION
 * ========================================================================== */

/* Device type - uncomment one */
// #define AGSYS_DEVICE_TYPE   AGSYS_DEVICE_TYPE_SOIL_MOISTURE
// #define AGSYS_DEVICE_TYPE   AGSYS_DEVICE_TYPE_VALVE_CONTROLLER
// #define AGSYS_DEVICE_TYPE   AGSYS_DEVICE_TYPE_VALVE_ACTUATOR
// #define AGSYS_DEVICE_TYPE   AGSYS_DEVICE_TYPE_WATER_METER

/* Firmware version */
#define AGSYS_FW_VERSION_MAJOR      1
#define AGSYS_FW_VERSION_MINOR      0
#define AGSYS_FW_VERSION_PATCH      0

/* ==========================================================================
 * DEBUG CONFIGURATION
 * ========================================================================== */

/* Enable debug features (set to 0 for release builds) */
#define AGSYS_DEBUG_ENABLED         1

/* Logging backend - choose one */
#define AGSYS_USE_NRF_LOG           1   /* Use Nordic NRF_LOG */
#define AGSYS_USE_RTT               0   /* Use Segger RTT directly */

/* ==========================================================================
 * SPI CONFIGURATION
 * ========================================================================== */

/* SPI instance to use (0, 1, 2, or 3 depending on chip) */
#define AGSYS_SPI_INSTANCE          0

/* SPI pins - customize per board */
#define AGSYS_SPI_SCK_PIN           NRF_GPIO_PIN_MAP(0, 14)
#define AGSYS_SPI_MOSI_PIN          NRF_GPIO_PIN_MAP(0, 13)
#define AGSYS_SPI_MISO_PIN          NRF_GPIO_PIN_MAP(0, 12)

/* Default SPI frequency */
#define AGSYS_SPI_DEFAULT_FREQ      NRF_SPIM_FREQ_4M

/* ==========================================================================
 * FRAM CONFIGURATION
 * ========================================================================== */

/* FRAM chip select pin */
#define AGSYS_FRAM_CS_PIN           NRF_GPIO_PIN_MAP(0, 15)

/* ==========================================================================
 * LORA CONFIGURATION
 * ========================================================================== */

/* LoRa module pins */
#define AGSYS_LORA_CS_PIN           NRF_GPIO_PIN_MAP(0, 27)
#define AGSYS_LORA_RST_PIN          NRF_GPIO_PIN_MAP(0, 30)
#define AGSYS_LORA_DIO0_PIN         NRF_GPIO_PIN_MAP(0, 31)

/* LoRa frequency (US915) */
#define AGSYS_LORA_FREQUENCY        915000000

/* LoRa parameters */
#define AGSYS_LORA_SPREADING_FACTOR 7
#define AGSYS_LORA_BANDWIDTH        125000
#define AGSYS_LORA_TX_POWER         17  /* dBm */

/* ==========================================================================
 * BLE CONFIGURATION
 * ========================================================================== */

/* BLE device name prefix (full name = prefix + short ID) */
#define AGSYS_BLE_NAME_PREFIX       "AgSys-"

/* BLE advertising interval (ms) */
#define AGSYS_BLE_ADV_INTERVAL_MS   1000

/* BLE connection parameters */
#define AGSYS_BLE_MIN_CONN_INTERVAL MSEC_TO_UNITS(100, UNIT_1_25_MS)
#define AGSYS_BLE_MAX_CONN_INTERVAL MSEC_TO_UNITS(200, UNIT_1_25_MS)
#define AGSYS_BLE_SLAVE_LATENCY     0
#define AGSYS_BLE_CONN_SUP_TIMEOUT  MSEC_TO_UNITS(4000, UNIT_10_MS)

/* ==========================================================================
 * FREERTOS TASK CONFIGURATION
 * ========================================================================== */

/* Task stack sizes (in words, not bytes) */
#define AGSYS_TASK_STACK_DEFAULT    256
#define AGSYS_TASK_STACK_BLE        256
#define AGSYS_TASK_STACK_LORA       512
#define AGSYS_TASK_STACK_DISPLAY    1024    /* LVGL needs more */

/* Task priorities (higher = more important) */
#define AGSYS_TASK_PRIORITY_IDLE    1
#define AGSYS_TASK_PRIORITY_LOW     2
#define AGSYS_TASK_PRIORITY_NORMAL  3
#define AGSYS_TASK_PRIORITY_HIGH    4
#define AGSYS_TASK_PRIORITY_REALTIME 5

/* ==========================================================================
 * DEVICE-SPECIFIC CONFIGURATION
 * ========================================================================== */

/* --- Soil Moisture Sensor --- */
#if AGSYS_DEVICE_TYPE == AGSYS_DEVICE_TYPE_SOIL_MOISTURE
#define AGSYS_SENSOR_REPORT_INTERVAL_MS     (2 * 60 * 60 * 1000)  /* 2 hours */
#define AGSYS_SENSOR_HBRIDGE_PIN_A          NRF_GPIO_PIN_MAP(0, 2)
#define AGSYS_SENSOR_HBRIDGE_PIN_B          NRF_GPIO_PIN_MAP(0, 3)
#define AGSYS_SENSOR_ADC_PIN                NRF_GPIO_PIN_MAP(0, 4)
#endif

/* --- Water Meter --- */
#if AGSYS_DEVICE_TYPE == AGSYS_DEVICE_TYPE_WATER_METER
#define AGSYS_METER_ADC_CS_PIN              NRF_GPIO_PIN_MAP(0, 11)
#define AGSYS_METER_ADC_DRDY_PIN            NRF_GPIO_PIN_MAP(0, 21)
#define AGSYS_METER_DISPLAY_CS_PIN          NRF_GPIO_PIN_MAP(0, 5)
#define AGSYS_METER_DISPLAY_DC_PIN          NRF_GPIO_PIN_MAP(0, 6)
#define AGSYS_METER_DISPLAY_RST_PIN         NRF_GPIO_PIN_MAP(0, 7)
#define AGSYS_METER_SAMPLE_RATE_HZ          1000
#define AGSYS_METER_REPORT_INTERVAL_MS      (60 * 1000)  /* 1 minute */
#endif

/* --- Valve Controller --- */
#if AGSYS_DEVICE_TYPE == AGSYS_DEVICE_TYPE_VALVE_CONTROLLER
#define AGSYS_VALVE_CAN_CS_PIN              NRF_GPIO_PIN_MAP(0, 11)
#define AGSYS_VALVE_CAN_INT_PIN             NRF_GPIO_PIN_MAP(0, 8)
#define AGSYS_VALVE_MAX_ACTUATORS           64
#endif

/* --- Valve Actuator --- */
#if AGSYS_DEVICE_TYPE == AGSYS_DEVICE_TYPE_VALVE_ACTUATOR
#define AGSYS_ACTUATOR_HBRIDGE_IN1          NRF_GPIO_PIN_MAP(0, 2)
#define AGSYS_ACTUATOR_HBRIDGE_IN2          NRF_GPIO_PIN_MAP(0, 3)
#define AGSYS_ACTUATOR_CURRENT_SENSE_PIN    NRF_GPIO_PIN_MAP(0, 4)
#define AGSYS_ACTUATOR_OPEN_TIMEOUT_MS      30000
#define AGSYS_ACTUATOR_CLOSE_TIMEOUT_MS     30000
#endif

#endif /* AGSYS_CONFIG_H */
