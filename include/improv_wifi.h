#ifndef IMPROV_WIFI_H
#define IMPROV_WIFI_H

#include <Arduino.h>
#include <improv.h>
#include <Preferences.h>

// WiFi connection timeout
#define WIFI_CONNECT_TIMEOUT 30000
// WiFi reconnection delay (retry every 10 seconds)
#define WIFI_RECONNECT_DELAY 10000

// Improv WiFi setup and loop
void setupImprovWiFi();
void loopImprov();

// WiFi connection management
void connectToWiFi(const char* ssid, const char* password);
void checkWiFiConnection();
String getLocalIP();

// Retry WiFi connection with saved credentials
void retryWiFiConnection();

// Get next WiFi retry time (for countdown display)
unsigned long getNextWiFiRetryTime();

// WiFi connection state
extern bool wifi_connecting;
extern unsigned long wifi_connect_start;
extern bool wifi_was_connected;
extern unsigned long wifi_reconnect_attempt_time;

// Preferences for storing WiFi credentials
extern Preferences wifi_preferences;

#endif // IMPROV_WIFI_H
