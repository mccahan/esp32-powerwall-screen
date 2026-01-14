#ifndef CONFIG_SCREEN_H
#define CONFIG_SCREEN_H

#include <lvgl.h>

// Config screen management
void createConfigScreen();
void showConfigScreen();
void hideConfigScreen();
bool isConfigScreenVisible();

// Update QR code with current IP (call when showing screen)
void updateConfigScreenQR();

#endif // CONFIG_SCREEN_H
