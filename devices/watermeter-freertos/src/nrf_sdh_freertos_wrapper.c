/**
 * @file nrf_sdh_freertos_wrapper.c
 * @brief Wrapper to include app_error.h before nrf_sdh_freertos.c
 * 
 * The SDK's nrf_sdh_freertos.c uses APP_ERROR_HANDLER but doesn't include
 * app_error.h. This wrapper ensures the header is included first.
 */

#include "app_error.h"
#include "nrf_sdh_freertos.c"
