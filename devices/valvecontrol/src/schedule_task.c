/**
 * @file schedule_task.c
 * @brief Schedule task implementation for Valve Controller
 * 
 * Manages time-based irrigation schedules using RV-3028 RTC and MB85RS1MT FRAM.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_drv_twi.h"
#include "SEGGER_RTT.h"

#include "schedule_task.h"
#include "can_task.h"
#include "board_config.h"
#include "agsys_fram.h"
#include "agsys_memory_layout.h"

#include <string.h>

/* ==========================================================================
 * RV-3028 RTC DEFINITIONS (I2C)
 * ========================================================================== */

#define RV3028_ADDR             0x52

#define RV3028_SECONDS          0x00
#define RV3028_MINUTES          0x01
#define RV3028_HOURS            0x02
#define RV3028_WEEKDAY          0x03
#define RV3028_DATE             0x04
#define RV3028_MONTH            0x05
#define RV3028_YEAR             0x06
#define RV3028_UNIX_TIME_0      0x1B
#define RV3028_UNIX_TIME_1      0x1C
#define RV3028_UNIX_TIME_2      0x1D
#define RV3028_UNIX_TIME_3      0x1E
#define RV3028_STATUS           0x0E
#define RV3028_CONTROL_1        0x0F
#define RV3028_CONTROL_2        0x10

/* ==========================================================================
 * FRAM SCHEDULE STORAGE (uses common HAL)
 * 
 * Schedules are stored in the App Data region defined in agsys_memory_layout.h
 * ========================================================================== */

#define FRAM_SCHEDULE_ADDR      AGSYS_FRAM_APP_DATA_ADDR
#define FRAM_SCHEDULE_MAGIC     0xA65C

/* FRAM context - set by schedule_init() */
static agsys_fram_ctx_t *m_fram_ctx = NULL;

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static schedule_entry_t m_schedules[MAX_SCHEDULES];
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(1);
static bool m_twi_initialized = false;

/* External power state */
extern volatile bool g_on_battery_power;

/* ==========================================================================
 * I2C (TWI) FUNCTIONS
 * ========================================================================== */

static bool twi_init(void)
{
    if (m_twi_initialized) return true;
    
    nrf_drv_twi_config_t config = NRF_DRV_TWI_DEFAULT_CONFIG;
    config.scl = I2C_SCL_PIN;
    config.sda = I2C_SDA_PIN;
    config.frequency = NRF_DRV_TWI_FREQ_400K;
    
    ret_code_t err = nrf_drv_twi_init(&m_twi, &config, NULL, NULL);
    if (err != NRF_SUCCESS) {
        SEGGER_RTT_printf(0, "TWI init failed: %d\n", err);
        return false;
    }
    
    nrf_drv_twi_enable(&m_twi);
    m_twi_initialized = true;
    SEGGER_RTT_printf(0, "TWI initialized\n");
    return true;
}

static bool rtc_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    return nrf_drv_twi_tx(&m_twi, RV3028_ADDR, data, 2, false) == NRF_SUCCESS;
}

static bool rtc_read_reg(uint8_t reg, uint8_t *value)
{
    if (nrf_drv_twi_tx(&m_twi, RV3028_ADDR, &reg, 1, true) != NRF_SUCCESS) {
        return false;
    }
    return nrf_drv_twi_rx(&m_twi, RV3028_ADDR, value, 1) == NRF_SUCCESS;
}

static bool rtc_read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    if (nrf_drv_twi_tx(&m_twi, RV3028_ADDR, &reg, 1, true) != NRF_SUCCESS) {
        return false;
    }
    return nrf_drv_twi_rx(&m_twi, RV3028_ADDR, data, len) == NRF_SUCCESS;
}

/* ==========================================================================
 * RTC FUNCTIONS
 * ========================================================================== */

static void rtc_init(void)
{
    /* Enable direct UNIX time reading */
    uint8_t ctrl2;
    if (rtc_read_reg(RV3028_CONTROL_2, &ctrl2)) {
        ctrl2 |= 0x20;  /* Set EERD bit for EEPROM auto-refresh */
        rtc_write_reg(RV3028_CONTROL_2, ctrl2);
    }
    
    SEGGER_RTT_printf(0, "RTC initialized\n");
}

uint32_t schedule_get_rtc_time(void)
{
    uint8_t data[4];
    if (!rtc_read_regs(RV3028_UNIX_TIME_0, data, 4)) {
        return 0;
    }
    
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

void schedule_set_rtc_time(uint32_t unix_time)
{
    rtc_write_reg(RV3028_UNIX_TIME_0, unix_time & 0xFF);
    rtc_write_reg(RV3028_UNIX_TIME_1, (unix_time >> 8) & 0xFF);
    rtc_write_reg(RV3028_UNIX_TIME_2, (unix_time >> 16) & 0xFF);
    rtc_write_reg(RV3028_UNIX_TIME_3, (unix_time >> 24) & 0xFF);
    
    SEGGER_RTT_printf(0, "RTC time set: %u\n", unix_time);
}

/* Helper to get day of week and time from unix timestamp */
static void unix_to_time(uint32_t unix_time, uint8_t *dow, uint8_t *hour, uint8_t *minute)
{
    /* Simple calculation - doesn't account for timezone */
    uint32_t days = unix_time / 86400;
    uint32_t seconds_today = unix_time % 86400;
    
    *dow = (days + 4) % 7;  /* Jan 1, 1970 was Thursday (day 4) */
    *hour = seconds_today / 3600;
    *minute = (seconds_today % 3600) / 60;
}

/* ==========================================================================
 * SCHEDULE STORAGE (uses common FRAM HAL)
 * ========================================================================== */

void schedule_set_fram_ctx(agsys_fram_ctx_t *ctx)
{
    m_fram_ctx = ctx;
}

void schedule_load(void)
{
    if (m_fram_ctx == NULL) {
        SEGGER_RTT_printf(0, "Schedule: FRAM context not set\n");
        return;
    }
    
    /* Read magic number */
    uint16_t magic;
    if (agsys_fram_read(m_fram_ctx, FRAM_SCHEDULE_ADDR, (uint8_t *)&magic, 2) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "Schedule: FRAM read error\n");
        return;
    }
    
    if (magic != FRAM_SCHEDULE_MAGIC) {
        SEGGER_RTT_printf(0, "No valid schedules in FRAM\n");
        memset(m_schedules, 0, sizeof(m_schedules));
        return;
    }
    
    /* Read schedules */
    if (agsys_fram_read(m_fram_ctx, FRAM_SCHEDULE_ADDR + 2, (uint8_t *)m_schedules, sizeof(m_schedules)) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "Schedule: FRAM read error\n");
        return;
    }
    
    /* Count enabled schedules */
    uint8_t count = 0;
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (m_schedules[i].enabled) count++;
    }
    
    SEGGER_RTT_printf(0, "Loaded %d schedules from FRAM\n", count);
}

void schedule_save(void)
{
    if (m_fram_ctx == NULL) {
        SEGGER_RTT_printf(0, "Schedule: FRAM context not set\n");
        return;
    }
    
    /* Write magic number */
    uint16_t magic = FRAM_SCHEDULE_MAGIC;
    if (agsys_fram_write(m_fram_ctx, FRAM_SCHEDULE_ADDR, (uint8_t *)&magic, 2) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "Schedule: FRAM write error\n");
        return;
    }
    
    /* Write schedules */
    if (agsys_fram_write(m_fram_ctx, FRAM_SCHEDULE_ADDR + 2, (uint8_t *)m_schedules, sizeof(m_schedules)) != AGSYS_OK) {
        SEGGER_RTT_printf(0, "Schedule: FRAM write error\n");
        return;
    }
    
    SEGGER_RTT_printf(0, "Schedules saved to FRAM\n");
}

void schedule_update(uint8_t index, const schedule_entry_t *entry)
{
    if (index >= MAX_SCHEDULES) return;
    
    memcpy(&m_schedules[index], entry, sizeof(schedule_entry_t));
    schedule_save();
}

const schedule_entry_t* schedule_get(uint8_t index)
{
    if (index >= MAX_SCHEDULES) return NULL;
    return &m_schedules[index];
}

/* ==========================================================================
 * SCHEDULE EXECUTION
 * ========================================================================== */

static bool should_run_schedule(const schedule_entry_t *entry, uint8_t dow, 
                                 uint8_t hour, uint8_t minute)
{
    if (!entry->enabled) return false;
    
    /* Check day of week */
    if (!(entry->days_of_week & (1 << dow))) return false;
    
    /* Check time (within 1 minute window) */
    if (entry->start_hour != hour) return false;
    if (entry->start_minute != minute) return false;
    
    return true;
}

static void run_schedule(const schedule_entry_t *entry)
{
    SEGGER_RTT_printf(0, "Running schedule: UID=%02X%02X... for %d min\n",
                      entry->actuator_uid[0], entry->actuator_uid[1], 
                      entry->duration_minutes);
    
    /* Open valve by UID */
    if (!can_open_valve_by_uid(entry->actuator_uid)) {
        SEGGER_RTT_printf(0, "Schedule: Failed to open valve (UID not found)\n");
        return;
    }
    
    /* Note: In a full implementation, we'd track the running schedule
     * and close the valve after duration_minutes. For now, the property
     * controller handles duration tracking via LoRa commands. */
}

/* ==========================================================================
 * SCHEDULE TASK
 * ========================================================================== */

bool schedule_task_init(void)
{
    memset(m_schedules, 0, sizeof(m_schedules));
    return true;
}

void schedule_task(void *pvParameters)
{
    (void)pvParameters;
    
    SEGGER_RTT_printf(0, "Schedule task started\n");
    
    /* Initialize I2C for RTC */
    twi_init();
    rtc_init();
    
    /* Load schedules from FRAM */
    schedule_load();
    
    uint8_t last_minute = 0xFF;
    
    for (;;) {
        /* Only run schedules when on mains power */
        if (!g_on_battery_power) {
            uint32_t now = schedule_get_rtc_time();
            
            if (now > 0) {
                uint8_t dow, hour, minute;
                unix_to_time(now, &dow, &hour, &minute);
                
                /* Check schedules once per minute */
                if (minute != last_minute) {
                    last_minute = minute;
                    
                    for (int i = 0; i < MAX_SCHEDULES; i++) {
                        if (should_run_schedule(&m_schedules[i], dow, hour, minute)) {
                            run_schedule(&m_schedules[i]);
                        }
                    }
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
