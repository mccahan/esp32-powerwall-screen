#ifndef BOOT_SCREEN_H
#define BOOT_SCREEN_H

#include <lvgl.h>

// Boot screen timeout
#define BOOT_SCREEN_TIMEOUT 5000 // 5 seconds

// Boot screen management
void createBootScreen(lv_obj_t *parent_screen);
void showBootScreen();
void hideBootScreen();
bool isBootScreenVisible();

// Boot screen timing
extern unsigned long boot_start_time;

#endif // BOOT_SCREEN_H
