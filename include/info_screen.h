#ifndef INFO_SCREEN_H
#define INFO_SCREEN_H

#include <lvgl.h>

// Info screen management
void createInfoScreen();
void showInfoScreen();
void hideInfoScreen();
bool isInfoScreenVisible();

// Update info screen data (call periodically)
void updateInfoScreenData();

#endif // INFO_SCREEN_H
