#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "mqtt_client.h"

class PowerwallWebServer {
public:
    PowerwallWebServer();
    
    void begin();
    
private:
    AsyncWebServer server;
    
    void setupRoutes();
    String getConfigPage();
};

extern PowerwallWebServer webServer;

#endif // WEB_SERVER_H
