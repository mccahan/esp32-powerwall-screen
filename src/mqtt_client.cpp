#include "mqtt_client.h"
#include <WiFi.h>

// Static instance pointer
PowerwallMQTTClient* PowerwallMQTTClient::instance = nullptr;

// Global instance
PowerwallMQTTClient mqttClient;

PowerwallMQTTClient::PowerwallMQTTClient()
    : solarCallback(nullptr), gridCallback(nullptr), homeCallback(nullptr),
      batteryCallback(nullptr), socCallback(nullptr), offGridCallback(nullptr),
      timeRemainingCallback(nullptr), evCallback(nullptr), evConnectedCallback(nullptr),
      evSOCCallback(nullptr), last_ev_power(0.0f), reconnect_enabled(false),
      last_reconnect_attempt(0), reconnect_delay(MQTT_RECONNECT_MIN_DELAY) {
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
        
        // Don't connect yet - wait for WiFi to be ready
        Serial.printf("MQTT client initialized - Server: %s:%d (waiting for WiFi)\n", config.host.c_str(), config.port);
    } else {
        Serial.println("MQTT not configured - skipping initialization");
    }
}

void PowerwallMQTTClient::loop() {
    // Handle auto-reconnect with exponential backoff
    if (!reconnect_enabled || mqtt_client.connected()) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED || config.host.length() == 0) {
        return;
    }

    unsigned long now = millis();
    if (now - last_reconnect_attempt >= reconnect_delay) {
        last_reconnect_attempt = now;

        Serial.printf("Attempting MQTT reconnect (delay: %lums)...\n", reconnect_delay);
        mqtt_client.connect();

        // Exponential backoff: double delay up to max
        reconnect_delay = min(reconnect_delay * 2, (unsigned long)MQTT_RECONNECT_MAX_DELAY);
    }
}

void PowerwallMQTTClient::loadConfig() {
    preferences.begin("mqtt", true); // Read-only
    config.host = preferences.getString("host", "");
    config.port = preferences.getInt("port", 1883);
    config.user = preferences.getString("user", "");
    config.password = preferences.getString("password", "");
    config.topic_prefix = preferences.getString("prefix", "pypowerwall/");

    // EV configuration
    config.ev_enabled = preferences.getBool("ev_enabled", false);
    config.ev_power_topic = preferences.getString("ev_power", "");
    config.ev_connected_topic = preferences.getString("ev_conn", "");
    config.ev_soc_topic = preferences.getString("ev_soc", "");
    preferences.end();

    Serial.println("─────────────────────────────────");
    Serial.println("MQTT Configuration Loaded:");
    Serial.printf("  Host: %s\n", config.host.length() > 0 ? config.host.c_str() : "(not configured)");
    Serial.printf("  Port: %d\n", config.port);
    Serial.printf("  User: %s\n", config.user.length() > 0 ? config.user.c_str() : "(none)");
    Serial.printf("  Password: %s\n", config.password.length() > 0 ? "***" : "(none)");
    Serial.printf("  Topic Prefix: %s\n", config.topic_prefix.c_str());
    Serial.printf("  EV Enabled: %s\n", config.ev_enabled ? "yes" : "no");
    if (config.ev_enabled) {
        Serial.printf("  EV Power Topic: %s\n", config.ev_power_topic.c_str());
        Serial.printf("  EV Connected Topic: %s\n", config.ev_connected_topic.length() > 0 ? config.ev_connected_topic.c_str() : "(not configured)");
        Serial.printf("  EV SOC Topic: %s\n", config.ev_soc_topic.length() > 0 ? config.ev_soc_topic.c_str() : "(not configured)");
    }
    Serial.println("─────────────────────────────────");
}

void PowerwallMQTTClient::saveConfig() {
    preferences.begin("mqtt", false);
    preferences.putString("host", config.host);
    preferences.putInt("port", config.port);
    preferences.putString("user", config.user);
    preferences.putString("password", config.password);
    preferences.putString("prefix", config.topic_prefix);

    // EV configuration
    preferences.putBool("ev_enabled", config.ev_enabled);
    preferences.putString("ev_power", config.ev_power_topic);
    preferences.putString("ev_conn", config.ev_connected_topic);
    preferences.putString("ev_soc", config.ev_soc_topic);
    preferences.end();

    Serial.println("✓ MQTT Config saved to flash");
    
    // Reinitialize with new config
    if (config.host.length() > 0) {
        Serial.println("→ Reinitializing MQTT with new config...");
        mqtt_client.disconnect();
        mqtt_client.setServer(config.host.c_str(), config.port);
        
        if (config.user.length() > 0) {
            mqtt_client.setCredentials(config.user.c_str(), config.password.c_str());
        } else {
            mqtt_client.setCredentials(nullptr, nullptr);
        }
        
        // Only connect if WiFi is already connected
        if (WiFi.status() == WL_CONNECTED) {
            mqtt_client.connect();
        } else {
            Serial.println("→ WiFi not connected, will connect to MQTT when WiFi is ready");
        }
    }
}

bool PowerwallMQTTClient::isConnected() {
    return mqtt_client.connected();
}

MQTTConfig& PowerwallMQTTClient::getConfig() {
    return config;
}

void PowerwallMQTTClient::disconnect() {
    reconnect_enabled = false;  // Disable auto-reconnect on explicit disconnect
    mqtt_client.disconnect();
}

void PowerwallMQTTClient::connect() {
    // Only attempt connection if WiFi is connected and MQTT is configured
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("✗ Cannot connect to MQTT - WiFi not connected");
        return;
    }
    
    if (config.host.length() == 0) {
        Serial.println("✗ Cannot connect to MQTT - not configured");
        return;
    }
    
    Serial.printf("→ Connecting to MQTT broker at %s:%d", config.host.c_str(), config.port);
    if (config.user.length() > 0) {
        Serial.printf(" (user: %s)", config.user.c_str());
    }
    Serial.println("...");
    
    mqtt_client.connect();
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

void PowerwallMQTTClient::setEVCallback(void (*callback)(float)) {
    evCallback = callback;
}

void PowerwallMQTTClient::setEVConnectedCallback(void (*callback)(bool)) {
    evConnectedCallback = callback;
}

void PowerwallMQTTClient::setEVSOCCallback(void (*callback)(float)) {
    evSOCCallback = callback;
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
    Serial.println("✓ Connected to MQTT broker");

    // Reset reconnect state on successful connection
    reconnect_enabled = true;  // Enable auto-reconnect for future disconnects
    reconnect_delay = MQTT_RECONNECT_MIN_DELAY;  // Reset backoff
    
    // Subscribe to all pypowerwall topics
    String prefix = config.topic_prefix;
    
    mqtt_client.subscribe((prefix + "site/instant_power").c_str(), 0);
    mqtt_client.subscribe((prefix + "battery/instant_power").c_str(), 0);
    mqtt_client.subscribe((prefix + "solar/instant_power").c_str(), 0);
    mqtt_client.subscribe((prefix + "load/instant_power").c_str(), 0);
    mqtt_client.subscribe((prefix + "battery/level").c_str(), 0);
    mqtt_client.subscribe((prefix + "site/offgrid").c_str(), 0);
    mqtt_client.subscribe((prefix + "battery/time_remaining").c_str(), 0);

    Serial.printf("✓ Subscribed to MQTT topics with prefix: %s\n", prefix.c_str());

    // Subscribe to EV topics if enabled (these use full topic paths, not prefix)
    if (config.ev_enabled) {
        if (config.ev_power_topic.length() > 0) {
            mqtt_client.subscribe(config.ev_power_topic.c_str(), 0);
            Serial.printf("✓ Subscribed to EV power topic: %s\n", config.ev_power_topic.c_str());
        }
        if (config.ev_connected_topic.length() > 0) {
            mqtt_client.subscribe(config.ev_connected_topic.c_str(), 0);
            Serial.printf("✓ Subscribed to EV connected topic: %s\n", config.ev_connected_topic.c_str());
        }
        if (config.ev_soc_topic.length() > 0) {
            mqtt_client.subscribe(config.ev_soc_topic.c_str(), 0);
            Serial.printf("✓ Subscribed to EV SOC topic: %s\n", config.ev_soc_topic.c_str());
        }
    }
}

void PowerwallMQTTClient::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.print("✗ Disconnected from MQTT broker - Reason: ");
    switch(reason) {
        case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
            Serial.println("TCP disconnected");
            break;
        case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
            Serial.println("Unacceptable protocol version");
            break;
        case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
            Serial.println("Identifier rejected");
            break;
        case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
            Serial.println("Server unavailable");
            break;
        case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
            Serial.println("Malformed credentials");
            break;
        case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
            Serial.println("Not authorized");
            break;
        default:
            Serial.println("Unknown");
            break;
    }
    
    // Enable auto-reconnect if WiFi is still connected
    if (WiFi.status() == WL_CONNECTED && config.host.length() > 0) {
        reconnect_enabled = true;
        last_reconnect_attempt = millis();  // Start backoff timer
        Serial.printf("Will attempt to reconnect in %lums...\n", reconnect_delay);
    }
}

void PowerwallMQTTClient::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    // Limit message size to prevent stack overflow
    if (len > MAX_MQTT_MESSAGE_SIZE) {
        Serial.printf("✗ MQTT message too large (%d bytes), ignoring\n", len);
        return;
    }
    
    // Convert payload to null-terminated string
    char message[MAX_MQTT_MESSAGE_SIZE + 1];
    memcpy(message, payload, len);
    message[len] = '\0';
    
    String topicStr = String(topic);
    String prefix = config.topic_prefix;

    // Handle EV connected topic first (accepts non-numeric values like "true", "on", etc.)
    if (config.ev_enabled && config.ev_connected_topic.length() > 0 && topicStr == config.ev_connected_topic) {
        bool connected = false;
        String msgStr = String(message);
        msgStr.toLowerCase();
        if (msgStr == "1" || msgStr == "true" || msgStr == "on" || msgStr == "yes" || msgStr == "connected") {
            connected = true;
        }
        if (evConnectedCallback) {
            evConnectedCallback(connected);
        }
        Serial.printf("← MQTT: EV Connected: %s\n", connected ? "yes" : "no");
        return;
    }

    // Parse the value with error checking for numeric topics
    char* endptr;
    float value = strtod(message, &endptr);

    // Check if conversion was successful
    if (endptr == message || *endptr != '\0') {
        Serial.printf("✗ Failed to parse MQTT value from topic '%s': %s\n", topic, message);
        return;
    }

    // Match topic and call appropriate callback
    if (topicStr == prefix + "solar/instant_power") {
        if (solarCallback) {
            solarCallback(value);
        }
        Serial.printf("← MQTT: Solar: %.1f W\n", value);
    }
    else if (topicStr == prefix + "site/instant_power") {
        if (gridCallback) {
            gridCallback(value);
        }
        Serial.printf("← MQTT: Grid: %.1f W\n", value);
    }
    else if (topicStr == prefix + "load/instant_power") {
        // Subtract EV power from home if EV tracking is enabled
        float adjusted_home = value;
        if (config.ev_enabled && last_ev_power > 0) {
            adjusted_home = value - last_ev_power;
            if (adjusted_home < 0) adjusted_home = 0;
        }
        if (homeCallback) {
            homeCallback(adjusted_home);
        }
        if (config.ev_enabled && last_ev_power > 0) {
            Serial.printf("← MQTT: Load: %.1f W (adjusted: %.1f W, EV: %.1f W)\n", value, adjusted_home, last_ev_power);
        } else {
            Serial.printf("← MQTT: Load: %.1f W\n", value);
        }
    }
    else if (topicStr == prefix + "battery/instant_power") {
        if (batteryCallback) {
            batteryCallback(value);
        }
        Serial.printf("← MQTT: Battery: %.1f W\n", value);
    }
    else if (topicStr == prefix + "battery/level") {
        if (socCallback) {
            socCallback(value);
        }
        Serial.printf("← MQTT: SOC: %.1f %%\n", value);
    }
    else if (topicStr == prefix + "site/offgrid") {
        // Parse integer value with error checking
        char* endptr_int;
        long offgrid_long = strtol(message, &endptr_int, 10);
        if (endptr_int == message || *endptr_int != '\0' || offgrid_long < 0 || offgrid_long > 1) {
            Serial.printf("✗ Failed to parse off-grid value: %s\n", message);
            return;
        }
        int offgrid = (int)offgrid_long;
        if (offGridCallback) {
            offGridCallback(offgrid);
        }
        Serial.printf("← MQTT: Off-grid: %d\n", offgrid);
    }
    else if (topicStr == prefix + "battery/time_remaining") {
        if (timeRemainingCallback) {
            timeRemainingCallback(value);
        }
        Serial.printf("← MQTT: Time remaining: %.1f hours\n", value);
    }
    // EV topics (use full topic path matching, not prefix-based)
    else if (config.ev_enabled && config.ev_power_topic.length() > 0 && topicStr == config.ev_power_topic) {
        last_ev_power = value;
        if (evCallback) {
            evCallback(value);
        }
        Serial.printf("← MQTT: EV Power: %.1f W\n", value);
    }
    else if (config.ev_enabled && config.ev_soc_topic.length() > 0 && topicStr == config.ev_soc_topic) {
        if (evSOCCallback) {
            evSOCCallback(value);
        }
        Serial.printf("← MQTT: EV SOC: %.1f %%\n", value);
    }
}
