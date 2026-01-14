#ifndef WIFI_ERROR_SCREEN_H
#define WIFI_ERROR_SCREEN_H

#include <lvgl.h>

// Create WiFi error screen as overlay on parent
void createWifiErrorScreen(lv_obj_t *parent_screen);

// Show/hide WiFi error screen
void showWifiErrorScreen(const char* message);
void hideWifiErrorScreen();
bool isWifiErrorScreenVisible();

// Update countdown display
void updateWifiErrorCountdown(unsigned long next_retry_time);

#endif // WIFI_ERROR_SCREEN_H
