#ifndef LOADING_SCREEN_H
#define LOADING_SCREEN_H

#include <lvgl.h>

// Loading screen management
void createLoadingScreen(lv_obj_t* parent);
void showLoadingScreen();
void hideLoadingScreen();
bool isLoadingScreenVisible();

#endif // LOADING_SCREEN_H
