/**
 * @file agsys_debug.h
 * @brief Debug logging macros for AgSys devices
 * 
 * Uses Nordic's NRF_LOG or RTT depending on configuration.
 */

#ifndef AGSYS_DEBUG_H
#define AGSYS_DEBUG_H

#include "agsys_config.h"

#if AGSYS_USE_NRF_LOG

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define AGSYS_LOG_INIT()        do { \
    NRF_LOG_INIT(NULL); \
    NRF_LOG_DEFAULT_BACKENDS_INIT(); \
} while(0)

#define AGSYS_LOG_FLUSH()       NRF_LOG_FLUSH()
#define AGSYS_LOG_INFO(...)     NRF_LOG_INFO(__VA_ARGS__)
#define AGSYS_LOG_DEBUG(...)    NRF_LOG_DEBUG(__VA_ARGS__)
#define AGSYS_LOG_WARNING(...)  NRF_LOG_WARNING(__VA_ARGS__)
#define AGSYS_LOG_ERROR(...)    NRF_LOG_ERROR(__VA_ARGS__)
#define AGSYS_LOG_HEXDUMP(p, len) NRF_LOG_HEXDUMP_INFO(p, len)

#elif AGSYS_USE_RTT

#include "SEGGER_RTT.h"

#define AGSYS_LOG_INIT()        SEGGER_RTT_Init()
#define AGSYS_LOG_FLUSH()       do {} while(0)
#define AGSYS_LOG_INFO(...)     SEGGER_RTT_printf(0, __VA_ARGS__); SEGGER_RTT_printf(0, "\n")
#define AGSYS_LOG_DEBUG(...)    SEGGER_RTT_printf(0, "[D] " __VA_ARGS__); SEGGER_RTT_printf(0, "\n")
#define AGSYS_LOG_WARNING(...)  SEGGER_RTT_printf(0, "[W] " __VA_ARGS__); SEGGER_RTT_printf(0, "\n")
#define AGSYS_LOG_ERROR(...)    SEGGER_RTT_printf(0, "[E] " __VA_ARGS__); SEGGER_RTT_printf(0, "\n")
#define AGSYS_LOG_HEXDUMP(p, len) do { \
    for (uint32_t i = 0; i < (len); i++) { \
        SEGGER_RTT_printf(0, "%02X ", ((uint8_t*)(p))[i]); \
    } \
    SEGGER_RTT_printf(0, "\n"); \
} while(0)

#else

#define AGSYS_LOG_INIT()        do {} while(0)
#define AGSYS_LOG_FLUSH()       do {} while(0)
#define AGSYS_LOG_INFO(...)     do {} while(0)
#define AGSYS_LOG_DEBUG(...)    do {} while(0)
#define AGSYS_LOG_WARNING(...)  do {} while(0)
#define AGSYS_LOG_ERROR(...)    do {} while(0)
#define AGSYS_LOG_HEXDUMP(p, len) do {} while(0)

#endif

/* Assert macro */
#if AGSYS_DEBUG_ENABLED
#define AGSYS_ASSERT(expr) do { \
    if (!(expr)) { \
        AGSYS_LOG_ERROR("ASSERT FAILED: %s:%d", __FILE__, __LINE__); \
        while(1) { __WFE(); } \
    } \
} while(0)
#else
#define AGSYS_ASSERT(expr) ((void)0)
#endif

#endif /* AGSYS_DEBUG_H */
