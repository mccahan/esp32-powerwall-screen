#include "improv_wifi.h"
#include "main_screen.h"
#include "boot_screen.h"
#include "wifi_error_screen.h"
#include "mqtt_client.h"
#include <WiFi.h>
#include <lvgl.h>

// Device info
#define DEVICE_NAME "Powerwall Display"
#define FIRMWARE_VERSION "1.0.0"
#define HARDWARE_VARIANT "ESP32-S3-4848S040"
#define DEVICE_URL "http://{LOCAL_IPV4}/"

// Improv WiFi state
static improv::State improv_state = improv::STATE_AUTHORIZED;
static uint8_t improv_buffer[256];
static size_t improv_buffer_pos = 0;

// WiFi connection state
bool wifi_connecting = false;
unsigned long wifi_connect_start = 0;

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
            std::vector<String> info = {
                FIRMWARE_VERSION,
                DEVICE_NAME,
                HARDWARE_VARIANT,
                DEVICE_URL
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

void checkWiFiConnection() {
    if (!wifi_connecting) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifi_connecting = false;
        improv_state = improv::STATE_PROVISIONED;
        sendImprovState();

        String ip = getLocalIP();
        std::vector<String> urls = {ip};
        sendImprovRPCResponse(improv::WIFI_SETTINGS, urls);

        // Hide boot/error screens and show dashboard
        hideBootScreen();
        hideWifiErrorScreen();

        Serial.printf("WiFi connected! IP: %s\n", ip.c_str());
        
        // Now that WiFi is connected, connect to MQTT broker
        mqttClient.connect();
    }
    else if (millis() - wifi_connect_start > WIFI_CONNECT_TIMEOUT) {
        wifi_connecting = false;
        improv_state = improv::STATE_AUTHORIZED;
        sendImprovState();
        sendImprovError(improv::ERROR_UNABLE_TO_CONNECT);

        // Show WiFi error screen
        hideBootScreen();
        showWifiErrorScreen("Connection failed\nUse ESP Web Tools to retry");

        Serial.println("WiFi connection timeout");
    }
}

String getLocalIP() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "";
}
