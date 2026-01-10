/**
 * @file sleep_manager.c
 * @brief Deep sleep manager implementation
 * 
 * Uses RTC2 for wake timing since RTC0 is reserved for SoftDevice
 * and RTC1 is used by FreeRTOS tick.
 * 
 * Sleep sequence:
 * 1. Put peripherals to sleep (LoRa, FRAM)
 * 2. Configure RTC2 for wake after sleep_ms
 * 3. Configure button GPIO for wake
 * 4. Enter System ON sleep via sd_app_evt_wait()
 * 5. On wake, restore peripherals
 */

#include "sdk_config.h"
#include "sleep_manager.h"
#include "board_config.h"
#include "lora_task.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_rtc.h"
#include "nrf_sdh.h"
#include "nrf_pwr_mgmt.h"

#include "FreeRTOS.h"
#include "task.h"

#include "SEGGER_RTT.h"

/* ==========================================================================
 * RTC2 CONFIGURATION
 * ========================================================================== */

/* RTC2 runs at 32.768 kHz / (PRESCALER + 1)
 * With PRESCALER = 327, frequency = 32768 / 328 = ~100 Hz (10ms resolution)
 * Max sleep time = 2^24 / 100 = ~167772 seconds = ~46 hours
 */
#define RTC_PRESCALER       327
#define RTC_FREQ_HZ         100
#define MS_TO_RTC_TICKS(ms) ((ms) * RTC_FREQ_HZ / 1000)

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static volatile bool m_rtc_wake = false;
static volatile bool m_button_wake = false;
static bool m_initialized = false;

/* ==========================================================================
 * INTERRUPT HANDLERS
 * ========================================================================== */

void RTC2_IRQHandler(void)
{
    if (NRF_RTC2->EVENTS_COMPARE[0]) {
        NRF_RTC2->EVENTS_COMPARE[0] = 0;
        m_rtc_wake = true;
    }
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool sleep_manager_init(void)
{
    if (m_initialized) {
        return true;
    }
    
    /* Configure RTC2 */
    NRF_RTC2->PRESCALER = RTC_PRESCALER;
    
    /* Enable compare interrupt */
    NRF_RTC2->INTENSET = RTC_INTENSET_COMPARE0_Msk;
    
    /* Enable RTC2 interrupt in NVIC */
    NVIC_SetPriority(RTC2_IRQn, 7);  /* Low priority */
    NVIC_EnableIRQ(RTC2_IRQn);
    
    /* Configure button for wake (sense low) */
    nrf_gpio_cfg_input(PAIRING_BUTTON_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_sense_set(PAIRING_BUTTON_PIN, NRF_GPIO_PIN_SENSE_LOW);
    
    m_initialized = true;
    SEGGER_RTT_printf(0, "SleepMgr: Initialized (RTC2 @ %d Hz)\n", RTC_FREQ_HZ);
    return true;
}

/* ==========================================================================
 * SLEEP FUNCTIONS
 * ========================================================================== */

void sleep_manager_prepare_sleep(void)
{
    /* Put LoRa to sleep */
    lora_sleep();
    
    /* Turn off LED */
    nrf_gpio_pin_set(LED_STATUS_PIN);
    
    SEGGER_RTT_printf(0, "SleepMgr: Peripherals prepared for sleep\n");
}

void sleep_manager_restore_wake(void)
{
    /* Wake LoRa */
    lora_wake();
    
    SEGGER_RTT_printf(0, "SleepMgr: Peripherals restored\n");
}

uint32_t sleep_manager_sleep(uint32_t sleep_ms)
{
    if (!m_initialized) {
        return 0;
    }
    
    SEGGER_RTT_printf(0, "SleepMgr: Sleeping for %lu ms\n", sleep_ms);
    
    /* Clear wake flags */
    m_rtc_wake = false;
    m_button_wake = false;
    
    /* Calculate RTC ticks */
    uint32_t ticks = MS_TO_RTC_TICKS(sleep_ms);
    if (ticks > 0xFFFFFF) {
        ticks = 0xFFFFFF;  /* Max 24-bit value */
    }
    
    /* Configure RTC2 compare */
    NRF_RTC2->CC[0] = ticks;
    NRF_RTC2->TASKS_CLEAR = 1;
    NRF_RTC2->EVENTS_COMPARE[0] = 0;
    
    /* Start RTC2 */
    NRF_RTC2->TASKS_START = 1;
    
    /* Record start time */
    uint32_t start_tick = xTaskGetTickCount();
    
    /* Enter low power mode - SoftDevice handles the actual sleep
     * We use vTaskDelay which will put the CPU to sleep between ticks
     * The RTC2 interrupt will wake us early if needed */
    while (!m_rtc_wake && !m_button_wake) {
        /* Check button state */
        if (nrf_gpio_pin_read(PAIRING_BUTTON_PIN) == 0) {
            m_button_wake = true;
            break;
        }
        
        /* Sleep for 100ms at a time, checking for wake */
        vTaskDelay(pdMS_TO_TICKS(100));
        
        /* Check if RTC expired */
        if (NRF_RTC2->EVENTS_COMPARE[0]) {
            m_rtc_wake = true;
            NRF_RTC2->EVENTS_COMPARE[0] = 0;
            break;
        }
    }
    
    /* Stop RTC2 */
    NRF_RTC2->TASKS_STOP = 1;
    
    /* Calculate actual sleep time */
    uint32_t elapsed = (xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS;
    
    SEGGER_RTT_printf(0, "SleepMgr: Woke after %lu ms (%s)\n", 
                      elapsed,
                      m_button_wake ? "button" : "RTC");
    
    return elapsed;
}

bool sleep_manager_woken_by_button(void)
{
    return m_button_wake;
}

void sleep_manager_clear_wake_flags(void)
{
    m_rtc_wake = false;
    m_button_wake = false;
}
