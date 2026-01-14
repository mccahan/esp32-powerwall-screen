#ifndef MQTT_CONFIG_SCREEN_H
#define MQTT_CONFIG_SCREEN_H

#include <lvgl.h>

// Create MQTT config screen as overlay on parent
void createMqttConfigScreen(lv_obj_t *parent_screen);

// Show/hide MQTT config screen with QR code to web server
void showMqttConfigScreen(const char* ip_address);
void hideMqttConfigScreen();
bool isMqttConfigScreenVisible();

#endif // MQTT_CONFIG_SCREEN_H
