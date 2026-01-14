#include "improv_wifi.h"
#include "main_screen.h"
#include "boot_screen.h"
#include "wifi_error_screen.h"
#include "mqtt_config_screen.h"
#include "mqtt_client.h"
#include "captive_portal.h"
#include "web_server.h"
#include <WiFi.h>
#include <lvgl.h>

// Device info
#define DEVICE_NAME "Powerwall Display"
#define FIRMWARE_VERSION "1.0.0"
#define HARDWARE_VARIANT "ESP32-S3-4848S040"

// Improv WiFi state
static improv::State improv_state = improv::STATE_AUTHORIZED;
static uint8_t improv_buffer[256];
static size_t improv_buffer_pos = 0;

// WiFi connection state
bool wifi_connecting = false;
unsigned long wifi_connect_start = 0;
bool wifi_was_connected = false;
unsigned long wifi_reconnect_attempt_time = 0;
unsigned long wifi_disconnected_time = 0;

// Preferences for storing WiFi credentials
Preferences wifi_preferences;

// Forward declarations
static void handleImprovCommand(improv::ImprovCommand cmd);
static void sendImprovState();
static void sendImprovError(improv::Error error);
static void sendImprovRPCResponse(improv::Command cmd, const std::vector<String> &data);

void setupImprovWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    improv_state = improv::STATE_AUTHORIZED;
    sendImprovState();
}

void loopImprov() {
    while (Serial.available()) {
        uint8_t b = Serial.read();

        // Add byte to buffer first
        if (improv_buffer_pos < sizeof(improv_buffer)) {
            improv_buffer[improv_buffer_pos++] = b;
        } else {
            // Buffer overflow, reset
            improv_buffer_pos = 0;
            continue;
        }

        // Try to parse the buffer
        improv::ImprovCommand cmd = {improv::UNKNOWN, "", ""};
        improv::Error err = improv::ERROR_NONE;

        bool valid = improv::parse_improv_serial_byte(
            improv_buffer_pos - 1,
            b,
            improv_buffer,
            [&cmd](improv::ImprovCommand parsed_cmd) -> bool {
                cmd = parsed_cmd;
                return true;
            },
            [&err](improv::Error error) {
                err = error;
            }
        );

        if (!valid) {
            // Invalid sequence, reset buffer
            improv_buffer_pos = 0;
        } else if (cmd.command != improv::UNKNOWN) {
            // Complete command received
            handleImprovCommand(cmd);
            improv_buffer_pos = 0;
        }
        // else: valid but incomplete, keep accumulating
    }
}

static void handleImprovCommand(improv::ImprovCommand cmd) {
    switch (cmd.command) {
        case improv::WIFI_SETTINGS: {
            improv_state = improv::STATE_PROVISIONING;
            sendImprovState();

            lv_timer_handler();

            wifi_preferences.begin("wifi", false);
            wifi_preferences.putString("ssid", cmd.ssid.c_str());
            wifi_preferences.putString("password", cmd.password.c_str());
            wifi_preferences.end();

            connectToWiFi(cmd.ssid.c_str(), cmd.password.c_str());
            break;
        }

        case improv::GET_CURRENT_STATE: {
            sendImprovState();
            if (WiFi.status() == WL_CONNECTED) {
                std::vector<String> urls = {getLocalIP()};
                sendImprovRPCResponse(improv::GET_CURRENT_STATE, urls);
            }
            break;
        }

        case improv::GET_DEVICE_INFO: {
            String device_url = "http://" + getLocalIP();
            std::vector<String> info = {
                FIRMWARE_VERSION,
                DEVICE_NAME,
                HARDWARE_VARIANT,
                device_url
            };
            sendImprovRPCResponse(improv::GET_DEVICE_INFO, info);
            break;
        }

        case improv::GET_WIFI_NETWORKS: {
            lv_timer_handler();

            int n = WiFi.scanNetworks();

            // Send each network as a separate RPC response
            for (int i = 0; i < n && i < 10; i++) {
                String ssid = WiFi.SSID(i);
                String rssi = String(WiFi.RSSI(i));
                String auth = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "NO" : "YES";

                std::vector<String> network = {ssid, rssi, auth};
                sendImprovRPCResponse(improv::GET_WIFI_NETWORKS, network);
            }

            // Send empty response to signal end of list
            std::vector<String> empty;
            sendImprovRPCResponse(improv::GET_WIFI_NETWORKS, empty);

            WiFi.scanDelete();
            break;
        }

        default:
            sendImprovError(improv::ERROR_UNKNOWN_RPC);
            break;
    }

    improv_buffer_pos = 0;
}

static void sendImprovState() {
    uint8_t data[] = {
        'I', 'M', 'P', 'R', 'O', 'V',
        improv::IMPROV_SERIAL_VERSION,
        improv::TYPE_CURRENT_STATE,
        1,
        improv_state,
        0
    };

    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(data) - 1; i++) {
        checksum += data[i];
    }
    data[sizeof(data) - 1] = checksum;

    Serial.write(data, sizeof(data));
}

static void sendImprovError(improv::Error error) {
    uint8_t data[] = {
        'I', 'M', 'P', 'R', 'O', 'V',
        improv::IMPROV_SERIAL_VERSION,
        improv::TYPE_ERROR_STATE,
        1,
        error,
        0
    };

    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(data) - 1; i++) {
        checksum += data[i];
    }
    data[sizeof(data) - 1] = checksum;

    Serial.write(data, sizeof(data));
}

static void sendImprovRPCResponse(improv::Command cmd, const std::vector<String> &data) {
    std::vector<uint8_t> response = improv::build_rpc_response(cmd, data, false);

    std::vector<uint8_t> packet;
    packet.push_back('I');
    packet.push_back('M');
    packet.push_back('P');
    packet.push_back('R');
    packet.push_back('O');
    packet.push_back('V');
    packet.push_back(improv::IMPROV_SERIAL_VERSION);
    packet.push_back(improv::TYPE_RPC_RESPONSE);
    packet.push_back(response.size());

    for (auto &b : response) {
        packet.push_back(b);
    }

    uint8_t checksum = 0;
    for (auto &b : packet) {
        checksum += b;
    }
    packet.push_back(checksum);

    Serial.write(packet.data(), packet.size());
}

void connectToWiFi(const char* ssid, const char* password) {
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid, password);
    wifi_connecting = true;
    wifi_connect_start = millis();
}

void retryWiFiConnection() {
    // Load saved credentials and attempt reconnection
    String saved_ssid = "";
    String saved_pass = "";
    
    if (wifi_preferences.begin("wifi", false)) {
        if (wifi_preferences.isKey("ssid")) {
            saved_ssid = wifi_preferences.getString("ssid", "");
            saved_pass = wifi_preferences.getString("password", "");
        }
        wifi_preferences.end();
    }
    
    if (saved_ssid.length() > 0) {
        Serial.println("Attempting to reconnect to WiFi...");
        connectToWiFi(saved_ssid.c_str(), saved_pass.c_str());
    }
}

void checkWiFiConnection() {
    wl_status_t wifi_status = WiFi.status();
    
    // Handle WiFi reconnection (even when not actively connecting)
    // This covers cases where WiFi reconnects automatically at driver level
    if (!wifi_was_connected && wifi_status == WL_CONNECTED && wifi_reconnect_attempt_time > 0) {
        wifi_was_connected = true;
        wifi_connecting = false;
        wifi_disconnected_time = 0;  // Reset disconnection timer
        
        Serial.println("WiFi reconnected!");
        
        String ip = getLocalIP();
        Serial.printf("WiFi connected! IP: %s\n", ip.c_str());
        
        // Hide boot/error screens
        hideBootScreen();
        hideWifiErrorScreen();
        
        // Start web server if not already running
        webServer.begin();
        
        // Check if MQTT is configured
        MQTTConfig& mqtt_config = mqttClient.getConfig();
        if (mqtt_config.host.length() > 0) {
            // MQTT configured - connect to broker
            hideMqttConfigScreen();
            mqttClient.connect();
        } else {
            // MQTT not configured - show QR code to config page
            showMqttConfigScreen(ip.c_str());
            Serial.println("MQTT not configured - showing config screen");
        }
    }
    
    // Handle WiFi disconnection when it was previously connected
    if (wifi_was_connected && wifi_status != WL_CONNECTED) {
        wifi_was_connected = false;
        wifi_connecting = false;
        
        Serial.println("WiFi disconnected! Showing error screen...");
        
        // Disconnect MQTT since WiFi is lost
        mqttClient.disconnect();
        
        // Show WiFi error screen
        showWifiErrorScreen("WiFi connection lost\nRetrying...");
        
        // Set up for reconnection attempt
        wifi_reconnect_attempt_time = millis();
        wifi_disconnected_time = millis();  // Start tracking disconnection time
    }
    
    // Check if WiFi has been disconnected for too long - reboot device
    if (wifi_disconnected_time > 0 && wifi_status != WL_CONNECTED) {
        if (millis() - wifi_disconnected_time >= WIFI_DISCONNECTION_REBOOT_TIMEOUT) {
            Serial.println("WiFi disconnected for 5 minutes. Rebooting...");
            delay(1000);
            ESP.restart();
        }
    }
    
    // Handle periodic reconnection attempts when WiFi is not connected
    if (!wifi_connecting && wifi_status != WL_CONNECTED && wifi_reconnect_attempt_time > 0) {
        // Check if it's time to retry connection
        if (millis() - wifi_reconnect_attempt_time >= WIFI_RECONNECT_DELAY) {
            retryWiFiConnection();
            // Update timestamp after retry attempt to respect the reconnection delay
            wifi_reconnect_attempt_time = millis();
        }
    }
    
    // Handle initial connection attempt
    if (wifi_connecting) {
        if (wifi_status == WL_CONNECTED) {
            wifi_connecting = false;
            wifi_was_connected = true;
            wifi_disconnected_time = 0;  // Reset disconnection timer
            improv_state = improv::STATE_PROVISIONED;
            sendImprovState();

            String ip = getLocalIP();
            std::vector<String> urls = {"http://" + ip};
            sendImprovRPCResponse(improv::WIFI_SETTINGS, urls);

            // Stop captive portal if it was running
            stopCaptivePortal();

            // Start the main web server now that WiFi is connected
            webServer.begin();

            // Hide boot/error screens
            hideBootScreen();
            hideWifiErrorScreen();

            Serial.printf("WiFi connected! IP: %s\n", ip.c_str());

            // Check if MQTT is configured
            MQTTConfig& mqtt_config = mqttClient.getConfig();
            if (mqtt_config.host.length() > 0) {
                // MQTT configured - connect to broker
                hideMqttConfigScreen();
                mqttClient.connect();
            } else {
                // MQTT not configured - show QR code to config page
                showMqttConfigScreen(ip.c_str());
                Serial.println("MQTT not configured - showing config screen");
            }
        }
        else if (millis() - wifi_connect_start > WIFI_CONNECT_TIMEOUT) {
            wifi_connecting = false;
            improv_state = improv::STATE_AUTHORIZED;
            sendImprovState();
            sendImprovError(improv::ERROR_UNABLE_TO_CONNECT);

            // Show WiFi error screen
            hideBootScreen();
            showWifiErrorScreen("Connection failed\nRetrying...");

            Serial.println("WiFi connection timeout");
            
            // Set up for reconnection attempt
            wifi_reconnect_attempt_time = millis();
        }
    }
}

String getLocalIP() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "";
}

unsigned long getNextWiFiRetryTime() {
    if (wifi_reconnect_attempt_time > 0) {
        return wifi_reconnect_attempt_time + WIFI_RECONNECT_DELAY;
    }
    return 0;
}
