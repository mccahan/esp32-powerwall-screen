#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include <lvgl.h>

// Main screen management
void createMainDashboard();
lv_obj_t* getMainScreen();

// Power value update functions
void updateSolarValue(float watts);
void updateGridValue(float watts);
void updateHomeValue(float watts);
void updateBatteryValue(float watts);
void updateSOC(float soc_percent);
void updateOffGridStatus(int offgrid);
void updateTimeRemaining(float hours);

// Animation updates (called from main loop)
void updateDataRxPulse();
void updatePowerFlowAnimation();

// Timing variable for data RX indicator
extern unsigned long last_data_ms;

#endif // MAIN_SCREEN_H
