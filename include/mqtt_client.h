#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <Preferences.h>

// MQTT Configuration structure
struct MQTTConfig {
    String host;
    int port;
    String user;
    String password;
    String topic_prefix;
};

// MQTT Client class
class PowerwallMQTTClient {
public:
    PowerwallMQTTClient();
    
    void begin();
    void loadConfig();
    void saveConfig();
    
    bool isConnected();
    MQTTConfig& getConfig();
    void disconnect();
    
    // Callback setters for MQTT data updates
    void setSolarCallback(void (*callback)(float));
    void setGridCallback(void (*callback)(float));
    void setHomeCallback(void (*callback)(float));
    void setBatteryCallback(void (*callback)(float));
    void setSOCCallback(void (*callback)(float));
    void setOffGridCallback(void (*callback)(int));
    void setTimeRemainingCallback(void (*callback)(float));
    
private:
    AsyncMqttClient mqtt_client;
    MQTTConfig config;
    Preferences preferences;
    
    // Callbacks for data updates
    void (*solarCallback)(float);
    void (*gridCallback)(float);
    void (*homeCallback)(float);
    void (*batteryCallback)(float);
    void (*socCallback)(float);
    void (*offGridCallback)(int);
    void (*timeRemainingCallback)(float);
    
    void onMqttConnect(bool sessionPresent);
    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    
    // Static callbacks for AsyncMqttClient
    static void onMqttConnectStatic(bool sessionPresent);
    static void onMqttDisconnectStatic(AsyncMqttClientDisconnectReason reason);
    static void onMqttMessageStatic(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    
    // Static instance pointer for callbacks
    static PowerwallMQTTClient* instance;
};

extern PowerwallMQTTClient mqttClient;

#endif // MQTT_CLIENT_H
