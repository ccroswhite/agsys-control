/**
 * @file main.c
 * @brief Valve Controller FreeRTOS Application (nRF52832)
 * 
 * Task Architecture:
 * - CAN Task (Priority 5): Manages MCP2515 CAN bus to valve actuators
 * - LoRa Task (Priority 4): Communication with property controller
 * - Schedule Task (Priority 3): Time-based irrigation scheduling
 * - BLE Task (Priority 2): Local configuration via mobile app
 * - LED Task (Priority 1): Status indicators
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_freertos.h"

#include "SEGGER_RTT.h"

#include "board_config.h"
#include "agsys_spi.h"
#include "agsys_pins.h"
#include "can_task.h"
#include "lora_task.h"
#include "schedule_task.h"
#include "agsys_device.h"
#include "agsys_protocol.h"
#include "agsys_approtect.h"
#include "agsys_ble_ui.h"
#include "agsys_ota.h"
#include "agsys_ble_ota.h"
#include "agsys_flash.h"
#include "agsys_flash_backup.h"

/* ==========================================================================
 * SHARED RESOURCES
 * ========================================================================== */

/* SPI bus mutex - now managed by agsys_spi module */
/* Note: g_spi_mutex removed - agsys_spi handles mutex internally */

/* Device context (BLE, FRAM, Flash, auth) - non-static for logging access */
agsys_device_ctx_t m_device_ctx;

/* Power state */
volatile bool g_on_battery_power = false;
volatile bool g_power_fail_flag = false;

/* Pairing mode */
bool g_pairing_mode_active = false;
TickType_t g_pairing_start_tick = 0;

/* BLE UI state (shared component for consistent UX) */
static agsys_ble_ui_ctx_t m_ble_ui;

/* OTA contexts */
static agsys_flash_ctx_t m_flash_ctx;
static agsys_backup_ctx_t m_backup_ctx;
static agsys_ota_ctx_t m_ota_ctx;
static agsys_ble_ota_t m_ble_ota_ctx;
static bool m_ota_in_progress = false;

/* Task handles */
static TaskHandle_t m_can_task_handle = NULL;
static TaskHandle_t m_lora_task_handle = NULL;
static TaskHandle_t m_schedule_task_handle = NULL;
static TaskHandle_t m_led_task_handle = NULL;

/* Forward declarations */
static void exit_pairing_mode(void);

/* ==========================================================================
 * BLE EVENT HANDLER
 * ========================================================================== */

static void ble_event_handler(const agsys_ble_evt_t *evt)
{
    if (evt == NULL) return;

    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* Update shared BLE UI state */
    agsys_ble_ui_on_event(&m_ble_ui, evt->type, now_ms);

    switch (evt->type) {
        case AGSYS_BLE_EVT_CONNECTED:
            SEGGER_RTT_printf(0, "BLE: Connected\n");
            break;

        case AGSYS_BLE_EVT_DISCONNECTED:
            SEGGER_RTT_printf(0, "BLE: Disconnected\n");
            /* If still in pairing mode, return to advertising */
            if (g_pairing_mode_active) {
                agsys_ble_ui_set_advertising(&m_ble_ui, now_ms);
            }
            break;

        case AGSYS_BLE_EVT_AUTHENTICATED:
            SEGGER_RTT_printf(0, "BLE: Authenticated\n");
            break;

        case AGSYS_BLE_EVT_AUTH_FAILED:
            SEGGER_RTT_printf(0, "BLE: Auth failed\n");
            break;

        case AGSYS_BLE_EVT_AUTH_TIMEOUT:
            SEGGER_RTT_printf(0, "BLE: Auth timeout\n");
            break;

        case AGSYS_BLE_EVT_CONFIG_CHANGED:
            SEGGER_RTT_printf(0, "BLE: Config changed\n");
            break;

        case AGSYS_BLE_EVT_COMMAND_RECEIVED:
            SEGGER_RTT_printf(0, "BLE: Command received (cmd=%d)\n", evt->command.cmd_id);
            break;

        default:
            break;
    }
}

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    SEGGER_RTT_printf(0, "LED task started\n");
    
    nrf_gpio_cfg_output(LED_3V3_PIN);
    nrf_gpio_cfg_output(LED_24V_PIN);
    nrf_gpio_cfg_output(LED_STATUS_PIN);
    
    /* 3.3V LED always on */
    nrf_gpio_pin_set(LED_3V3_PIN);
    
    for (;;) {
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        /* 24V LED - on when not on battery */
        if (g_on_battery_power) {
            nrf_gpio_pin_clear(LED_24V_PIN);
        } else {
            nrf_gpio_pin_set(LED_24V_PIN);
        }
        
        /* Check for pairing timeout */
        if (g_pairing_mode_active) {
            TickType_t now = xTaskGetTickCount();
            if ((now - g_pairing_start_tick) >= pdMS_TO_TICKS(BLE_PAIRING_TIMEOUT_MS)) {
                exit_pairing_mode();
            }
        }
        
        /* BLE UI has priority when active */
        if (agsys_ble_ui_is_active(&m_ble_ui)) {
            /* Tick the BLE UI animation */
            if (agsys_ble_ui_tick(&m_ble_ui, now_ms)) {
                /* Visibility changed - update LED */
                if (agsys_ble_ui_is_visible(&m_ble_ui)) {
                    nrf_gpio_pin_set(LED_STATUS_PIN);   /* LED on */
                } else {
                    nrf_gpio_pin_clear(LED_STATUS_PIN); /* LED off */
                }
            }
            
            /* If BLE UI returned to idle, check if we should exit pairing */
            if (!agsys_ble_ui_is_active(&m_ble_ui) && !g_pairing_mode_active) {
                nrf_gpio_pin_clear(LED_STATUS_PIN);  /* Ensure LED off */
            }
            
            vTaskDelay(pdMS_TO_TICKS(20));  /* 50 Hz update for smooth animation */
        } else if (g_on_battery_power) {
            /* Slow blink on battery (1000ms) - different from BLE patterns */
            static bool battery_led_state = false;
            static TickType_t last_battery_toggle = 0;
            TickType_t now = xTaskGetTickCount();
            if (now - last_battery_toggle >= pdMS_TO_TICKS(1000)) {
                battery_led_state = !battery_led_state;
                nrf_gpio_pin_write(LED_STATUS_PIN, battery_led_state);
                last_battery_toggle = now;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            /* Off in normal operation */
            nrf_gpio_pin_clear(LED_STATUS_PIN);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/* ==========================================================================
 * POWER FAIL HANDLING
 * ========================================================================== */

static void power_fail_check(void)
{
    /* Check power fail pin */
    if (nrf_gpio_pin_read(POWER_FAIL_PIN) == 0) {
        if (!g_on_battery_power) {
            SEGGER_RTT_printf(0, "POWER FAIL: Switching to battery\n");
            g_on_battery_power = true;
            
            /* Emergency close all valves via CAN */
            can_emergency_close_all();
        }
    } else {
        if (g_on_battery_power) {
            SEGGER_RTT_printf(0, "POWER RESTORED\n");
            g_on_battery_power = false;
        }
    }
}

/* ==========================================================================
 * OTA CALLBACKS AND INITIALIZATION
 * ========================================================================== */

static void ota_progress_callback(agsys_ota_status_t status, uint8_t progress, void *user_data)
{
    (void)user_data;
    SEGGER_RTT_printf(0, "OTA: Status=%d, Progress=%d%%\n", status, progress);
    
    if (!m_ota_in_progress && status != AGSYS_OTA_STATUS_IDLE) {
        m_ota_in_progress = true;
    }
}

static void ota_complete_callback(bool success, agsys_ota_error_t error, void *user_data)
{
    (void)user_data;
    
    if (success) {
        SEGGER_RTT_printf(0, "OTA: Complete, rebooting...\n");
    } else {
        SEGGER_RTT_printf(0, "OTA: Failed (error=%d)\n", error);
        m_ota_in_progress = false;
    }
}

static bool init_ota(void)
{
    /* Initialize external flash */
    if (!agsys_flash_init(&m_flash_ctx, SPI_CS_FLASH_PIN)) {
        SEGGER_RTT_printf(0, "OTA: Flash init failed\n");
        return false;
    }
    
    /* Initialize backup system */
    if (!agsys_backup_init(&m_backup_ctx, &m_flash_ctx)) {
        SEGGER_RTT_printf(0, "OTA: Backup init failed\n");
        return false;
    }
    
    /* Check for rollback from previous failed update */
    if (agsys_backup_check_rollback(&m_backup_ctx)) {
        SEGGER_RTT_printf(0, "OTA: Rollback occurred from failed update\n");
    }
    
    /* Initialize OTA module */
    if (!agsys_ota_init(&m_ota_ctx, &m_flash_ctx, &m_backup_ctx)) {
        SEGGER_RTT_printf(0, "OTA: OTA init failed\n");
        return false;
    }
    
    /* Set callbacks */
    agsys_ota_set_progress_callback(&m_ota_ctx, ota_progress_callback, NULL);
    agsys_ota_set_complete_callback(&m_ota_ctx, ota_complete_callback, NULL);
    
    /* Register tasks to suspend during OTA apply phase */
    agsys_ota_register_task(m_can_task_handle);
    agsys_ota_register_task(m_lora_task_handle);
    agsys_ota_register_task(m_schedule_task_handle);
    agsys_ota_register_task(m_led_task_handle);
    
    /* Initialize BLE OTA service */
    uint32_t err_code = agsys_ble_ota_init(&m_ble_ota_ctx, &m_ota_ctx);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "OTA: BLE OTA init failed (err=%lu)\n", err_code);
    } else {
        SEGGER_RTT_printf(0, "OTA: BLE OTA enabled\n");
    }
    
    /* Confirm firmware if pending from previous OTA */
    if (agsys_ota_is_confirm_pending(&m_ota_ctx)) {
        SEGGER_RTT_printf(0, "OTA: Confirming firmware after successful boot\n");
        agsys_ota_confirm(&m_ota_ctx);
    }
    
    SEGGER_RTT_printf(0, "OTA: Initialized\n");
    return true;
}

/* ==========================================================================
 * LORA OTA MESSAGE HANDLER (called from lora_task)
 * ========================================================================== */

/**
 * @brief Handle incoming LoRa OTA message
 * 
 * Called by lora_task when an OTA message (0x40-0x45) is received.
 * Returns response data to send back to controller.
 * 
 * @param msg_type Message type (0x40-0x45)
 * @param data Message payload
 * @param len Payload length
 * @param response Output buffer for response (at least 4 bytes)
 * @param response_len Output: response length
 * @return true if response should be sent
 */
bool ota_handle_lora_message(uint8_t msg_type, const uint8_t *data, size_t len,
                              uint8_t *response, size_t *response_len)
{
    if (data == NULL || response == NULL || response_len == NULL) {
        return false;
    }
    
    *response_len = 0;
    
    switch (msg_type) {
        case 0x40: {  /* OTA_START */
            if (len < 12) {
                SEGGER_RTT_printf(0, "OTA: Invalid START message\n");
                response[0] = 0x80;  /* ACK_ERROR */
                response[1] = 0;
                *response_len = 2;
                return true;
            }
            
            uint32_t fw_size = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
            uint32_t fw_crc = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
            uint8_t major = data[8];
            uint8_t minor = data[9];
            uint8_t patch = data[10];
            
            SEGGER_RTT_printf(0, "OTA: LoRa START - size=%lu, v%d.%d.%d\n", 
                              fw_size, major, minor, patch);
            
            agsys_ota_error_t err = agsys_ota_start(&m_ota_ctx, fw_size, fw_crc, 
                                                     major, minor, patch);
            if (err == AGSYS_OTA_ERR_NONE) {
                response[0] = 0x01;  /* ACK_READY */
                response[1] = 0;
                *response_len = 2;
            } else {
                response[0] = 0x80;  /* ACK_ERROR */
                response[1] = (uint8_t)err;
                *response_len = 2;
            }
            return true;
        }
        
        case 0x41: {  /* OTA_CHUNK */
            if (len < 4) {
                return false;
            }
            
            uint16_t chunk_idx = data[0] | (data[1] << 8);
            /* offset_check at data[2-3] can be used for verification if needed */
            const uint8_t *chunk_data = &data[4];
            size_t chunk_len = len - 4;
            
            /* Calculate actual offset from chunk index */
            uint32_t offset = (uint32_t)chunk_idx * 200;  /* 200 byte chunks for LoRa */
            
            agsys_ota_error_t err = agsys_ota_write_chunk(&m_ota_ctx, offset, 
                                                          chunk_data, chunk_len);
            
            response[0] = (err == AGSYS_OTA_ERR_NONE) ? 0x02 : 0x80;  /* ACK_CHUNK_OK or ERROR */
            response[1] = agsys_ota_get_progress(&m_ota_ctx);
            response[2] = chunk_idx & 0xFF;
            response[3] = (chunk_idx >> 8) & 0xFF;
            *response_len = 4;
            return true;
        }
        
        case 0x42: {  /* OTA_FINISH */
            SEGGER_RTT_printf(0, "OTA: LoRa FINISH\n");
            
            agsys_ota_error_t err = agsys_ota_finish(&m_ota_ctx);
            if (err == AGSYS_OTA_ERR_NONE) {
                response[0] = 0x04;  /* ACK_REBOOTING */
                response[1] = 100;
                *response_len = 2;
                /* Reboot will happen after ACK is sent (handled by complete callback) */
            } else {
                response[0] = 0x80;  /* ACK_ERROR */
                response[1] = (uint8_t)err;
                *response_len = 2;
            }
            return true;
        }
        
        case 0x43: {  /* OTA_ABORT */
            SEGGER_RTT_printf(0, "OTA: LoRa ABORT\n");
            agsys_ota_abort(&m_ota_ctx);
            m_ota_in_progress = false;
            
            response[0] = 0x00;  /* ACK_OK */
            *response_len = 1;
            return true;
        }
        
        default:
            return false;
    }
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

static void softdevice_init(void)
{
    ret_code_t err_code;
    
    err_code = nrf_sdh_enable_request();
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "SoftDevice enable failed: %d\n", err_code);
        return;
    }
    
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(1, &ram_start);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "BLE config failed: %d\n", err_code);
        return;
    }
    
    err_code = nrf_sdh_ble_enable(&ram_start);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "BLE enable failed: %d\n", err_code);
        return;
    }
    
    SEGGER_RTT_printf(0, "SoftDevice initialized, RAM start: 0x%08X\n", ram_start);
}

static bool create_shared_resources(void)
{
    /* Initialize SPI buses with DMA support
     * Bus 0: Peripherals - CAN + LoRa (SPIM0)
     * Bus 1: Memory - FRAM + Flash (SPIM2, avoids TWI1 conflict)
     */
    agsys_spi_bus_config_t periph_bus = {
        .sck_pin = SPI_PERIPH_SCK_PIN,
        .mosi_pin = SPI_PERIPH_MOSI_PIN,
        .miso_pin = SPI_PERIPH_MISO_PIN,
        .spim_instance = 0,
    };
    if (agsys_spi_bus_init(AGSYS_SPI_BUS_0, &periph_bus) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "Failed to init SPI bus 0 (Peripherals)\n");
        return false;
    }
    
    agsys_spi_bus_config_t mem_bus = {
        .sck_pin = AGSYS_MEM_SPI_SCK,
        .mosi_pin = AGSYS_MEM_SPI_MOSI,
        .miso_pin = AGSYS_MEM_SPI_MISO,
        .spim_instance = 2,  /* Use SPIM2 to avoid TWI1 conflict */
    };
    if (agsys_spi_bus_init(AGSYS_SPI_BUS_1, &mem_bus) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "Failed to init SPI bus 1 (Memory)\n");
        return false;
    }
    
    SEGGER_RTT_printf(0, "SPI buses initialized with DMA\n");
    
    /* Initialize BLE UI context */
    agsys_ble_ui_init(&m_ble_ui);
    
    /* Initialize device (FRAM, Flash, BLE auth, BLE service) */
    agsys_device_init_t dev_init = {
        .device_name = "AgValve",
        .device_type = AGSYS_DEVICE_TYPE_VALVE_CONTROLLER,
        .fram_cs_pin = AGSYS_MEM_FRAM_CS,
        .flash_cs_pin = AGSYS_MEM_FLASH_CS,
        .memory_spi_bus = AGSYS_SPI_BUS_1,  /* Memory on bus 1 */
        .evt_handler = ble_event_handler
    };
    if (!agsys_device_init(&m_device_ctx, &dev_init)) {
        SEGGER_RTT_printf(0, "WARNING: Device init failed\n");
    }
    
    /* Pass FRAM context to schedule task */
    schedule_set_fram_ctx(&m_device_ctx.fram_ctx);
    
    /* Initialize task modules */
    if (!can_task_init()) {
        SEGGER_RTT_printf(0, "Failed to init CAN task\n");
        return false;
    }
    
    if (!lora_task_init()) {
        SEGGER_RTT_printf(0, "Failed to init LoRa task\n");
        return false;
    }
    
    if (!schedule_task_init()) {
        SEGGER_RTT_printf(0, "Failed to init Schedule task\n");
        return false;
    }
    
    SEGGER_RTT_printf(0, "Shared resources created\n");
    return true;
}

/* ==========================================================================
 * PAIRING MODE
 * ========================================================================== */

static bool check_pairing_button(void)
{
    /* Check if button is pressed (active LOW) */
    if (nrf_gpio_pin_read(PAIRING_BUTTON_PIN) != 0) {
        return false;
    }
    
    /* Debounce */
    nrf_delay_ms(50);
    if (nrf_gpio_pin_read(PAIRING_BUTTON_PIN) != 0) {
        return false;
    }
    
    /* Wait for hold duration */
    uint32_t start = 0;
    while (start < PAIRING_BUTTON_HOLD_MS) {
        if (nrf_gpio_pin_read(PAIRING_BUTTON_PIN) != 0) {
            return false;  /* Released early */
        }
        nrf_delay_ms(10);
        start += 10;
        
        /* Blink LED while holding */
        if ((start / 250) % 2) {
            nrf_gpio_pin_clear(LED_STATUS_PIN);
        } else {
            nrf_gpio_pin_set(LED_STATUS_PIN);
        }
    }
    
    nrf_gpio_pin_clear(LED_STATUS_PIN);
    return true;
}

static void enter_pairing_mode(void)
{
    SEGGER_RTT_printf(0, "Entering pairing mode (timeout: %d sec)\n",
                      BLE_PAIRING_TIMEOUT_MS / 1000);
    g_pairing_mode_active = true;
    g_pairing_start_tick = xTaskGetTickCount();
    agsys_device_start_advertising(&m_device_ctx);
    agsys_ble_ui_set_advertising(&m_ble_ui, xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void exit_pairing_mode(void)
{
    SEGGER_RTT_printf(0, "Exiting pairing mode\n");
    g_pairing_mode_active = false;
    agsys_device_stop_advertising(&m_device_ctx);
    agsys_ble_ui_set_idle(&m_ble_ui);
}

/* ==========================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
        SEGGER_RTT_printf(0, "Clock init failed: %d\n", err_code);
    }
    
    SEGGER_RTT_printf(0, "\n\n=== Valve Controller FreeRTOS ===\n");
    SEGGER_RTT_printf(0, "nRF52832 + S132 SoftDevice\n\n");
    
    /* Configure power fail pin */
    nrf_gpio_cfg_input(POWER_FAIL_PIN, NRF_GPIO_PIN_PULLUP);
    g_on_battery_power = (nrf_gpio_pin_read(POWER_FAIL_PIN) == 0);
    if (g_on_battery_power) {
        SEGGER_RTT_printf(0, "WARNING: Starting on battery power\n");
    }
    
    /* Configure pairing button */
    nrf_gpio_cfg_input(PAIRING_BUTTON_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* Configure LED for pairing button feedback */
    nrf_gpio_cfg_output(LED_STATUS_PIN);
    
    /* Check if pairing button held at boot (before BLE init) */
    bool start_pairing = check_pairing_button();
    if (start_pairing) {
        SEGGER_RTT_printf(0, "Pairing button held - will enter pairing mode\n");
    }
    
    softdevice_init();
    
    if (!create_shared_resources()) {
        SEGGER_RTT_printf(0, "Failed to create shared resources!\n");
        for (;;) { __WFE(); }
    }
    
    /* Start pairing mode after BLE is initialized */
    if (start_pairing) {
        enter_pairing_mode();
    }
    
    /* Create tasks */
    xTaskCreate(can_task, "CAN", TASK_STACK_CAN,
                NULL, TASK_PRIORITY_CAN, &m_can_task_handle);
    
    xTaskCreate(lora_task, "LoRa", TASK_STACK_LORA,
                NULL, TASK_PRIORITY_LORA, &m_lora_task_handle);
    
    xTaskCreate(schedule_task, "Sched", TASK_STACK_SCHEDULE,
                NULL, TASK_PRIORITY_SCHEDULE, &m_schedule_task_handle);
    
    xTaskCreate(led_task, "LED", TASK_STACK_LED,
                NULL, TASK_PRIORITY_LED, &m_led_task_handle);
    
    SEGGER_RTT_printf(0, "Tasks created\n");
    
    /* Initialize OTA after tasks are created */
    if (!init_ota()) {
        SEGGER_RTT_printf(0, "WARNING: OTA init failed, updates disabled\n");
    }
    
    nrf_sdh_freertos_init(NULL, NULL);
    
    SEGGER_RTT_printf(0, "Starting FreeRTOS scheduler...\n");
    
    vTaskStartScheduler();
    
    for (;;) {
        __WFE();
    }
}

/* ==========================================================================
 * FREERTOS HOOKS
 * ========================================================================== */

void vApplicationMallocFailedHook(void)
{
    SEGGER_RTT_printf(0, "Malloc failed!\n");
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    SEGGER_RTT_printf(0, "Stack overflow in task: %s\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationIdleHook(void)
{
    /* Check power state in idle */
    power_fail_check();
    __WFE();
}

static StaticTask_t xIdleTaskTCB;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = xIdleStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = xTimerStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
