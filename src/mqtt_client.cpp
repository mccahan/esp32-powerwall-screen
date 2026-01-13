#include "mqtt_client.h"

// Static instance pointer
PowerwallMQTTClient* PowerwallMQTTClient::instance = nullptr;

// Global instance
PowerwallMQTTClient mqttClient;

PowerwallMQTTClient::PowerwallMQTTClient() 
    : solarCallback(nullptr), gridCallback(nullptr), homeCallback(nullptr),
      batteryCallback(nullptr), socCallback(nullptr), offGridCallback(nullptr),
      timeRemainingCallback(nullptr) {
    instance = this;
    
    // Set up async MQTT callbacks
    mqtt_client.onConnect(onMqttConnectStatic);
    mqtt_client.onDisconnect(onMqttDisconnectStatic);
    mqtt_client.onMessage(onMqttMessageStatic);
}

void PowerwallMQTTClient::begin() {
    loadConfig();
    
    if (config.host.length() > 0) {
        mqtt_client.setServer(config.host.c_str(), config.port);
        
        if (config.user.length() > 0) {
            mqtt_client.setCredentials(config.user.c_str(), config.password.c_str());
        }
        
        mqtt_client.connect();
        Serial.printf("MQTT client initialized - Server: %s:%d\n", config.host.c_str(), config.port);
    } else {
        Serial.println("MQTT not configured - skipping initialization");
    }
}

void PowerwallMQTTClient::loadConfig() {
    preferences.begin("mqtt", true); // Read-only
    config.host = preferences.getString("host", "");
    config.port = preferences.getInt("port", 1883);
    config.user = preferences.getString("user", "");
    config.password = preferences.getString("password", "");
    config.topic_prefix = preferences.getString("prefix", "pypowerwall/");
    preferences.end();
    
    Serial.printf("MQTT Config loaded - Host: %s, Port: %d, Prefix: %s\n", 
                  config.host.c_str(), config.port, config.topic_prefix.c_str());
}

void PowerwallMQTTClient::saveConfig() {
    preferences.begin("mqtt", false);
    preferences.putString("host", config.host);
    preferences.putInt("port", config.port);
    preferences.putString("user", config.user);
    preferences.putString("password", config.password);
    preferences.putString("prefix", config.topic_prefix);
    preferences.end();
    
    Serial.println("MQTT Config saved to flash");
    
    // Reinitialize with new config
    if (config.host.length() > 0) {
        mqtt_client.disconnect();
        mqtt_client.setServer(config.host.c_str(), config.port);
        
        if (config.user.length() > 0) {
            mqtt_client.setCredentials(config.user.c_str(), config.password.c_str());
        } else {
            mqtt_client.setCredentials(nullptr, nullptr);
        }
        
        mqtt_client.connect();
    }
}

bool PowerwallMQTTClient::isConnected() {
    return mqtt_client.connected();
}

MQTTConfig& PowerwallMQTTClient::getConfig() {
    return config;
}

void PowerwallMQTTClient::disconnect() {
    mqtt_client.disconnect();
}

void PowerwallMQTTClient::setSolarCallback(void (*callback)(float)) {
    solarCallback = callback;
}

void PowerwallMQTTClient::setGridCallback(void (*callback)(float)) {
    gridCallback = callback;
}

void PowerwallMQTTClient::setHomeCallback(void (*callback)(float)) {
    homeCallback = callback;
}

void PowerwallMQTTClient::setBatteryCallback(void (*callback)(float)) {
    batteryCallback = callback;
}

void PowerwallMQTTClient::setSOCCallback(void (*callback)(float)) {
    socCallback = callback;
}

void PowerwallMQTTClient::setOffGridCallback(void (*callback)(int)) {
    offGridCallback = callback;
}

void PowerwallMQTTClient::setTimeRemainingCallback(void (*callback)(float)) {
    timeRemainingCallback = callback;
}

// Static callback wrappers
void PowerwallMQTTClient::onMqttConnectStatic(bool sessionPresent) {
    if (instance) {
        instance->onMqttConnect(sessionPresent);
    }
}

void PowerwallMQTTClient::onMqttDisconnectStatic(AsyncMqttClientDisconnectReason reason) {
    if (instance) {
        instance->onMqttDisconnect(reason);
    }
}

void PowerwallMQTTClient::onMqttMessageStatic(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    if (instance) {
        instance->onMqttMessage(topic, payload, properties, len, index, total);
    }
}

// Instance methods
void PowerwallMQTTClient::onMqttConnect(bool sessionPresent) {
    Serial.println("Connected to MQTT broker");
    
    // Subscribe to all pypowerwall topics
    String prefix = config.topic_prefix;
    
    mqtt_client.subscribe((prefix + "site/instant_power").c_str(), 0);
    mqtt_client.subscribe((prefix + "battery/instant_power").c_str(), 0);
    mqtt_client.subscribe((prefix + "solar/instant_power").c_str(), 0);
    mqtt_client.subscribe((prefix + "load/instant_power").c_str(), 0);
    mqtt_client.subscribe((prefix + "battery/level").c_str(), 0);
    mqtt_client.subscribe((prefix + "site/offgrid").c_str(), 0);
    mqtt_client.subscribe((prefix + "battery/time_remaining").c_str(), 0);
    
    Serial.printf("Subscribed to MQTT topics with prefix: %s\n", prefix.c_str());
}

void PowerwallMQTTClient::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.println("Disconnected from MQTT broker");
    
    // Reconnect will happen automatically via AsyncMqttClient
}

void PowerwallMQTTClient::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    // Limit message size to prevent stack overflow
    const size_t MAX_MESSAGE_SIZE = 64;
    if (len > MAX_MESSAGE_SIZE) {
        Serial.printf("MQTT message too large (%d bytes), ignoring\n", len);
        return;
    }
    
    // Convert payload to null-terminated string
    char message[MAX_MESSAGE_SIZE + 1];
    memcpy(message, payload, len);
    message[len] = '\0';
    
    String topicStr = String(topic);
    String prefix = config.topic_prefix;
    
    // Parse the value with error checking
    char* endptr;
    float value = strtod(message, &endptr);
    
    // Check if conversion was successful
    if (endptr == message || *endptr != '\0') {
        Serial.printf("Failed to parse MQTT value: %s\n", message);
        return;
    }
    
    // Match topic and call appropriate callback
    if (topicStr == prefix + "solar/instant_power") {
        if (solarCallback) {
            solarCallback(value);
        }
        Serial.printf("Solar: %.1f W\n", value);
    }
    else if (topicStr == prefix + "site/instant_power") {
        if (gridCallback) {
            gridCallback(value);
        }
        Serial.printf("Grid: %.1f W\n", value);
    }
    else if (topicStr == prefix + "load/instant_power") {
        if (homeCallback) {
            homeCallback(value);
        }
        Serial.printf("Load: %.1f W\n", value);
    }
    else if (topicStr == prefix + "battery/instant_power") {
        if (batteryCallback) {
            batteryCallback(value);
        }
        Serial.printf("Battery: %.1f W\n", value);
    }
    else if (topicStr == prefix + "battery/level") {
        if (socCallback) {
            socCallback(value);
        }
        Serial.printf("SOC: %.1f %%\n", value);
    }
    else if (topicStr == prefix + "site/offgrid") {
        // Parse integer value with error checking
        char* endptr_int;
        long offgrid_long = strtol(message, &endptr_int, 10);
        if (endptr_int == message || *endptr_int != '\0' || offgrid_long < 0 || offgrid_long > 1) {
            Serial.printf("Failed to parse off-grid value: %s\n", message);
            return;
        }
        int offgrid = (int)offgrid_long;
        if (offGridCallback) {
            offGridCallback(offgrid);
        }
        Serial.printf("Off-grid: %d\n", offgrid);
    }
    else if (topicStr == prefix + "battery/time_remaining") {
        if (timeRemainingCallback) {
            timeRemainingCallback(value);
        }
        Serial.printf("Time remaining: %.1f hours\n", value);
    }
}
