/**
 * @file ui_screen_ota.h
 * @brief OTA update progress and error screens for water meter
 */

#ifndef UI_SCREEN_OTA_H
#define UI_SCREEN_OTA_H

#include "ui/ui_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create OTA screen objects
 */
void ui_ota_create(void);

/**
 * @brief Show OTA progress screen
 * @param percent Initial progress 0-100
 * @param status Status message
 * @param version Target version string or NULL
 */
void ui_ota_show_progress(uint8_t percent, const char *status, const char *version);

/**
 * @brief Update OTA progress
 * @param percent Progress 0-100
 */
void ui_ota_update_progress(uint8_t percent);

/**
 * @brief Update OTA status message
 * @param status New status message
 */
void ui_ota_update_status(const char *status);

/**
 * @brief Show OTA error screen
 * @param error_msg Error description
 */
void ui_ota_show_error(const char *error_msg);

/**
 * @brief Check if OTA error screen is active
 */
bool ui_ota_is_error_active(void);

/**
 * @brief Tick handler for OTA error timeout
 * @return true if timeout expired and screen dismissed
 */
bool ui_ota_tick_error(void);

/**
 * @brief Dismiss OTA screens and return to main
 */
void ui_ota_dismiss(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_OTA_H */
