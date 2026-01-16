#include "web_server.h"
#include "main_screen.h"
#include <ArduinoJson.h>

// Global instance
PowerwallWebServer webServer;

PowerwallWebServer::PowerwallWebServer() : server(80) {
}

void PowerwallWebServer::begin() {
    setupRoutes();
    server.begin();
    Serial.println("Web server started on port 80");
}

void PowerwallWebServer::setupRoutes() {
    // Root page - redirect to config
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/config");
    });
    
    // Configuration page
    server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", getConfigPage());
    });
    
    // API endpoint to save MQTT configuration
    server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Validate content length
            if (total > MAX_JSON_PAYLOAD_SIZE) {
                request->send(413, "application/json", "{\"error\":\"Payload too large\"}");
                return;
            }
            
            StaticJsonDocument<MAX_JSON_PAYLOAD_SIZE> doc;
            DeserializationError error = deserializeJson(doc, data, len);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            // Update configuration
            MQTTConfig& config = mqttClient.getConfig();
            if (doc.containsKey("host")) config.host = doc["host"].as<String>();
            if (doc.containsKey("port")) config.port = doc["port"].as<int>();
            if (doc.containsKey("user")) config.user = doc["user"].as<String>();
            // Only update password if a non-empty value is provided
            if (doc.containsKey("password")) {
                String newPassword = doc["password"].as<String>();
                if (newPassword.length() > 0) {
                    config.password = newPassword;
                }
            }
            if (doc.containsKey("prefix")) config.topic_prefix = doc["prefix"].as<String>();
            
            // Save to flash and reconnect with new settings
            mqttClient.saveConfig();
            
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );
    
    // API endpoint to get current MQTT configuration
    server.on("/api/mqtt", HTTP_GET, [](AsyncWebServerRequest *request) {
        MQTTConfig& config = mqttClient.getConfig();

        StaticJsonDocument<512> doc;
        doc["host"] = config.host;
        doc["port"] = config.port;
        doc["user"] = config.user;
        // Don't expose password in GET response for security
        doc["password"] = config.password.length() > 0 ? "********" : "";
        doc["prefix"] = config.topic_prefix;
        doc["connected"] = mqttClient.isConnected();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API endpoint to save display configuration
    server.on("/api/display", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (total > MAX_JSON_PAYLOAD_SIZE) {
                request->send(413, "application/json", "{\"error\":\"Payload too large\"}");
                return;
            }

            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            DisplayConfig& config = displayConfig.getConfig();
            if (doc.containsKey("rotation")) {
                int degrees = doc["rotation"].as<int>();
                config.rotation = DisplayConfigManager::degreesToRotation(degrees);
            }

            displayConfig.saveConfig();

            // Note: Rotation change requires device restart to take effect
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Restart required for rotation change\"}");
        }
    );

    // API endpoint to get current display configuration
    server.on("/api/display", HTTP_GET, [](AsyncWebServerRequest *request) {
        DisplayConfig& config = displayConfig.getConfig();

        StaticJsonDocument<128> doc;
        doc["rotation"] = DisplayConfigManager::rotationToDegrees(config.rotation);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API endpoint to save brightness configuration
    server.on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (total > MAX_JSON_PAYLOAD_SIZE) {
                request->send(413, "application/json", "{\"error\":\"Payload too large\"}");
                return;
            }

            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            BrightnessConfig& config = brightnessConfig.getConfig();
            if (doc.containsKey("dayBrightness")) config.dayBrightness = doc["dayBrightness"].as<uint8_t>();
            if (doc.containsKey("nightBrightness")) config.nightBrightness = doc["nightBrightness"].as<uint8_t>();
            if (doc.containsKey("dayStartHour")) config.dayStartHour = doc["dayStartHour"].as<uint8_t>();
            if (doc.containsKey("dayEndHour")) config.dayEndHour = doc["dayEndHour"].as<uint8_t>();
            if (doc.containsKey("dayIdleDimmingEnabled")) config.dayIdleDimmingEnabled = doc["dayIdleDimmingEnabled"].as<bool>();
            if (doc.containsKey("nightIdleDimmingEnabled")) config.nightIdleDimmingEnabled = doc["nightIdleDimmingEnabled"].as<bool>();
            if (doc.containsKey("idleTimeout")) {
                int timeout = doc["idleTimeout"].as<int>();
                config.idleTimeout = BrightnessConfigManager::secondsToTimeout(timeout);
            }
            if (doc.containsKey("idleBrightness")) config.idleBrightness = doc["idleBrightness"].as<uint8_t>();

            brightnessConfig.saveConfig();

            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );

    // API endpoint to get current brightness configuration
    server.on("/api/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
        BrightnessConfig& config = brightnessConfig.getConfig();

        StaticJsonDocument<512> doc;
        doc["dayBrightness"] = config.dayBrightness;
        doc["nightBrightness"] = config.nightBrightness;
        doc["dayStartHour"] = config.dayStartHour;
        doc["dayEndHour"] = config.dayEndHour;
        doc["dayIdleDimmingEnabled"] = config.dayIdleDimmingEnabled;
        doc["nightIdleDimmingEnabled"] = config.nightIdleDimmingEnabled;
        doc["idleTimeout"] = BrightnessConfigManager::timeoutToSeconds(config.idleTimeout);
        doc["idleBrightness"] = config.idleBrightness;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API endpoint to save time configuration
    server.on("/api/time", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (total > MAX_JSON_PAYLOAD_SIZE) {
                request->send(413, "application/json", "{\"error\":\"Payload too large\"}");
                return;
            }

            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            TimeConfig& config = timeConfig.getConfig();
            if (doc.containsKey("ntpServer")) config.ntpServer = doc["ntpServer"].as<String>();
            if (doc.containsKey("timezone")) config.timezone = doc["timezone"].as<String>();
            if (doc.containsKey("ntpEnabled")) config.ntpEnabled = doc["ntpEnabled"].as<bool>();

            timeConfig.saveConfig();

            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );

    // API endpoint to get current time configuration
    server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
        TimeConfig& config = timeConfig.getConfig();

        StaticJsonDocument<512> doc;
        doc["ntpServer"] = config.ntpServer;
        doc["timezone"] = config.timezone;
        doc["ntpEnabled"] = config.ntpEnabled;
        doc["timeSynced"] = timeConfig.isTimeSynced();
        
        // Get current time if available
        struct tm timeinfo;
        if (timeConfig.getLocalTime(&timeinfo)) {
            char timeStr[32];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            doc["currentTime"] = timeStr;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // API endpoint to save EV configuration
    server.on("/api/ev", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (total > MAX_JSON_PAYLOAD_SIZE) {
                request->send(413, "application/json", "{\"error\":\"Payload too large\"}");
                return;
            }

            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, data, len);

            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            MQTTConfig& config = mqttClient.getConfig();
            bool wasEnabled = config.ev_enabled;

            if (doc.containsKey("enabled")) config.ev_enabled = doc["enabled"].as<bool>();
            if (doc.containsKey("powerTopic")) config.ev_power_topic = doc["powerTopic"].as<String>();
            if (doc.containsKey("connectedTopic")) config.ev_connected_topic = doc["connectedTopic"].as<String>();
            if (doc.containsKey("socTopic")) config.ev_soc_topic = doc["socTopic"].as<String>();

            // Save config and update UI
            mqttClient.saveConfig();
            setEVEnabled(config.ev_enabled);

            // If EV was just enabled or topics changed, reconnect MQTT to subscribe to new topics
            if (config.ev_enabled && mqttClient.isConnected()) {
                mqttClient.disconnect();
                mqttClient.connect();
            }

            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );

    // API endpoint to get current EV configuration
    server.on("/api/ev", HTTP_GET, [](AsyncWebServerRequest *request) {
        MQTTConfig& config = mqttClient.getConfig();

        StaticJsonDocument<512> doc;
        doc["enabled"] = config.ev_enabled;
        doc["powerTopic"] = config.ev_power_topic;
        doc["connectedTopic"] = config.ev_connected_topic;
        doc["socTopic"] = config.ev_soc_topic;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Screenshot capture endpoint
    server.on("/api/screenshot/capture", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (captureScreenshot()) {
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Screenshot captured\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Failed to capture screenshot\"}");
        }
    });

    // Screenshot download endpoint
    server.on("/api/screenshot/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (hasScreenshot()) {
            // Send screenshot data from PSRAM buffer
            AsyncWebServerResponse *response = request->beginResponse(
                "image/bmp",
                getScreenshotSize(),
                [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                    const uint8_t* screenshot_data = getScreenshotData();
                    size_t screenshot_size = getScreenshotSize();
                    
                    if (index >= screenshot_size) {
                        return 0;
                    }
                    
                    size_t remaining = screenshot_size - index;
                    size_t to_send = (remaining < maxLen) ? remaining : maxLen;
                    
                    memcpy(buffer, screenshot_data + index, to_send);
                    return to_send;
                }
            );
            response->addHeader("Content-Disposition", "attachment; filename=\"screenshot.bmp\"");
            request->send(response);
        } else {
            request->send(404, "application/json", "{\"error\":\"No screenshot available\"}");
        }
    });

    // Screenshot status endpoint
    server.on("/api/screenshot/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<128> doc;
        doc["available"] = hasScreenshot();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}

String PowerwallWebServer::getConfigPage() {
    MQTTConfig& mqttConf = mqttClient.getConfig();
    DisplayConfig& dispConfig = displayConfig.getConfig();
    BrightnessConfig& brightConf = brightnessConfig.getConfig();
    TimeConfig& timeConf = timeConfig.getConfig();
    
    int currentRotation = DisplayConfigManager::rotationToDegrees(dispConfig.rotation);

    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Powerwall Display Configuration</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: #1a1a1a;
            color: #e0e0e0;
            margin: 0;
            padding: 20px;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background: #2a2a2a;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.3);
        }
        h1 {
            color: #4FC3F7;
            text-align: center;
            margin-top: 0;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #b0b0b0;
            font-weight: bold;
        }
        input[type="text"],
        input[type="number"],
        input[type="password"],
        input[type="checkbox"],
        select {
            padding: 10px;
            border: 1px solid #444;
            border-radius: 4px;
            background: #1a1a1a;
            color: #e0e0e0;
            font-size: 16px;
        }
        input[type="text"],
        input[type="number"],
        input[type="password"],
        select {
            width: 100%;
            box-sizing: border-box;
        }
        input[type="checkbox"] {
            width: auto;
            margin-right: 10px;
        }
        input[type="text"]:focus,
        input[type="number"]:focus,
        input[type="password"]:focus,
        select:focus {
            outline: none;
            border-color: #4FC3F7;
        }
        .button {
            width: 100%;
            padding: 12px;
            background: #4FC3F7;
            color: #1a1a1a;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: background 0.3s;
        }
        .button:hover {
            background: #29B6F6;
        }
        .status {
            margin-top: 20px;
            padding: 15px;
            border-radius: 4px;
            text-align: center;
            display: none;
        }
        .status.success {
            background: #2e7d32;
            color: #fff;
        }
        .status.error {
            background: #c62828;
            color: #fff;
        }
        .info {
            margin-top: 20px;
            padding: 15px;
            background: #1565c0;
            border-radius: 4px;
            font-size: 14px;
        }
        .section-title {
            color: #4FC3F7;
            font-size: 18px;
            margin-top: 30px;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 1px solid #444;
        }
        .section-title:first-of-type {
            margin-top: 0;
        }
        .range-input-group {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .range-input-group input[type="range"] {
            flex: 1;
        }
        .range-value {
            min-width: 50px;
            text-align: center;
            color: #4FC3F7;
            font-weight: bold;
        }
        input[type="range"] {
            width: 100%;
            height: 8px;
            border-radius: 5px;
            background: #1a1a1a;
            outline: none;
            -webkit-appearance: none;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4FC3F7;
            cursor: pointer;
        }
        input[type="range"]::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4FC3F7;
            cursor: pointer;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Powerwall Display</h1>

        <h2 class="section-title">Display Settings</h2>
        <form id="displayForm">
            <div class="form-group">
                <label for="rotation">Screen Rotation:</label>
                <select id="rotation" name="rotation">
                    <option value="0">0 (Normal)</option>
                    <option value="180">180 (Upside Down)</option>
                </select>
            </div>
            <button type="submit" class="button">Save Display Settings</button>
        </form>
        <div class="status" id="displayStatus"></div>

        <h2 class="section-title">Brightness Settings</h2>
        <form id="brightnessForm">
            <div class="form-group">
                <label for="dayBrightness">Day Brightness: <span id="dayBrightnessValue" class="range-value">)rawliteral" + String(brightConf.dayBrightness) + R"rawliteral(%</span></label>
                <input type="range" id="dayBrightness" name="dayBrightness" min="10" max="100" value=")rawliteral" + String(brightConf.dayBrightness) + R"rawliteral(" oninput="document.getElementById('dayBrightnessValue').textContent = this.value + '%'">
            </div>
            <div class="form-group">
                <label for="nightBrightness">Night Brightness: <span id="nightBrightnessValue" class="range-value">)rawliteral" + String(brightConf.nightBrightness) + R"rawliteral(%</span></label>
                <input type="range" id="nightBrightness" name="nightBrightness" min="10" max="100" value=")rawliteral" + String(brightConf.nightBrightness) + R"rawliteral(" oninput="document.getElementById('nightBrightnessValue').textContent = this.value + '%'">
            </div>
            <div class="form-group">
                <label for="dayStartHour">Day Start Hour (0-23):</label>
                <input type="number" id="dayStartHour" name="dayStartHour" min="0" max="23" value=")rawliteral" + String(brightConf.dayStartHour) + R"rawliteral(" required>
            </div>
            <div class="form-group">
                <label for="dayEndHour">Day End Hour (0-23):</label>
                <input type="number" id="dayEndHour" name="dayEndHour" min="0" max="23" value=")rawliteral" + String(brightConf.dayEndHour) + R"rawliteral(" required>
            </div>
            <div class="form-group">
                <label>
                    <input type="checkbox" id="dayIdleDimmingEnabled" name="dayIdleDimmingEnabled" )rawliteral" + String(brightConf.dayIdleDimmingEnabled ? "checked" : "") + R"rawliteral(>
                    Enable Idle Dimming (Day)
                </label>
            </div>
            <div class="form-group">
                <label>
                    <input type="checkbox" id="nightIdleDimmingEnabled" name="nightIdleDimmingEnabled" )rawliteral" + String(brightConf.nightIdleDimmingEnabled ? "checked" : "") + R"rawliteral(>
                    Enable Idle Dimming (Night)
                </label>
            </div>
            <div class="form-group">
                <label for="idleTimeout">Idle Timeout:</label>
                <select id="idleTimeout" name="idleTimeout">
                    <option value="0">Never</option>
                    <option value="5">5 seconds</option>
                    <option value="15">15 seconds</option>
                    <option value="30">30 seconds</option>
                    <option value="60">60 seconds</option>
                </select>
            </div>
            <div class="form-group">
                <label for="idleBrightness">Idle Brightness: <span id="idleBrightnessValue" class="range-value">)rawliteral" + String(brightConf.idleBrightness) + R"rawliteral(%</span></label>
                <input type="range" id="idleBrightness" name="idleBrightness" min="10" max="100" value=")rawliteral" + String(brightConf.idleBrightness) + R"rawliteral(" oninput="document.getElementById('idleBrightnessValue').textContent = this.value + '%'">
            </div>
            <button type="submit" class="button">Save Brightness Settings</button>
        </form>
        <div class="status" id="brightnessStatus"></div>
    </div>

    <div class="container">
        <h2 class="section-title">Time Settings</h2>
        <form id="timeForm">
            <div class="form-group">
                <label>
                    <input type="checkbox" id="ntpEnabled" name="ntpEnabled" )rawliteral" + String(timeConf.ntpEnabled ? "checked" : "") + R"rawliteral(>
                    Enable NTP Time Sync
                </label>
            </div>
            <div class="form-group">
                <label for="ntpServer">NTP Server:</label>
                <input type="text" id="ntpServer" name="ntpServer" value=")rawliteral" + timeConf.ntpServer + R"rawliteral(" placeholder="pool.ntp.org">
            </div>
            <div class="form-group">
                <label for="timezone">Timezone (POSIX format):</label>
                <input type="text" id="timezone" name="timezone" value=")rawliteral" + timeConf.timezone + R"rawliteral(" placeholder="PST8PDT,M3.2.0,M11.1.0">
            </div>
            <button type="submit" class="button">Save Time Settings</button>
        </form>
        <div class="status" id="timeStatus"></div>
        <div class="info">
            <strong>Common Timezones:</strong><br>
            • EST5EDT,M3.2.0,M11.1.0 (US Eastern)<br>
            • CST6CDT,M3.2.0,M11.1.0 (US Central)<br>
            • MST7MDT,M3.2.0,M11.1.0 (US Mountain)<br>
            • PST8PDT,M3.2.0,M11.1.0 (US Pacific)<br>
            • UTC0 (UTC, no DST)
        </div>
    </div>

    <div class="container">
        <h2 class="section-title">MQTT Settings</h2>
        <form id="mqttForm">
            <div class="form-group">
                <label for="host">MQTT Host:</label>
                <input type="text" id="host" name="host" value=")rawliteral" + mqttConf.host + R"rawliteral(" required>
            </div>
            <div class="form-group">
                <label for="port">MQTT Port:</label>
                <input type="number" id="port" name="port" value=")rawliteral" + String(mqttConf.port) + R"rawliteral(" required>
            </div>
            <div class="form-group">
                <label for="user">MQTT Username:</label>
                <input type="text" id="user" name="user" value=")rawliteral" + mqttConf.user + R"rawliteral(">
            </div>
            <div class="form-group">
                <label for="password">MQTT Password:</label>
                <input type="password" id="password" name="password" placeholder="Enter new password or leave blank">
            </div>
            <div class="form-group">
                <label for="prefix">Topic Prefix:</label>
                <input type="text" id="prefix" name="prefix" value=")rawliteral" + mqttConf.topic_prefix + R"rawliteral(" placeholder="pypowerwall/" required>
            </div>
            <button type="submit" class="button">Save MQTT Settings</button>
        </form>
        <div class="status" id="mqttStatus"></div>
        <div class="info">
            <strong>Note:</strong> Topic prefix should match your pypowerwall MQTT configuration (default: "pypowerwall/").
            Device will automatically reconnect to MQTT broker after saving.
        </div>
    </div>

    <div class="container">
        <h2 class="section-title">EV Charger Settings (Optional)</h2>
        <form id="evForm">
            <div class="form-group">
                <label>
                    <input type="checkbox" id="evEnabled" name="evEnabled" )rawliteral" + String(mqttConf.ev_enabled ? "checked" : "") + R"rawliteral(>
                    Enable EV Charger Tracking
                </label>
            </div>
            <div class="form-group">
                <label for="evPowerTopic">EV Power Topic (required if enabled):</label>
                <input type="text" id="evPowerTopic" name="evPowerTopic" value=")rawliteral" + mqttConf.ev_power_topic + R"rawliteral(" placeholder="homeassistant/sensor/ev_charger/power">
            </div>
            <div class="form-group">
                <label for="evConnectedTopic">EV Connected Topic (optional):</label>
                <input type="text" id="evConnectedTopic" name="evConnectedTopic" value=")rawliteral" + mqttConf.ev_connected_topic + R"rawliteral(" placeholder="homeassistant/binary_sensor/ev_connected/state">
            </div>
            <div class="form-group">
                <label for="evSOCTopic">EV Charge Level Topic (optional):</label>
                <input type="text" id="evSOCTopic" name="evSOCTopic" value=")rawliteral" + mqttConf.ev_soc_topic + R"rawliteral(" placeholder="homeassistant/sensor/ev_battery/state">
            </div>
            <button type="submit" class="button">Save EV Settings</button>
        </form>
        <div class="status" id="evStatus"></div>
        <div class="info">
            <strong>Note:</strong> EV power is assumed to be included in home/load readings. It will be subtracted from the displayed Home value to avoid double-counting. Use full MQTT topic paths (not using the prefix above).
        </div>
    </div>

    <div class="container">
        <h2 class="section-title">Screenshot</h2>
        <div class="form-group">
            <button type="button" class="button" id="captureScreenshot">Capture Screenshot</button>
        </div>
        <div class="form-group">
            <button type="button" class="button" id="downloadScreenshot" style="display:none;">Download Screenshot</button>
        </div>
        <div class="status" id="screenshotStatus"></div>
        <div class="info">
            <strong>Info:</strong> Captures the current display as a BMP image file for documentation or troubleshooting.
        </div>
    </div>
    <script>
        // Set current values
        document.getElementById('rotation').value = ')rawliteral" + String(currentRotation) + R"rawliteral(';
        document.getElementById('idleTimeout').value = ')rawliteral" + String(BrightnessConfigManager::timeoutToSeconds(brightConf.idleTimeout)) + R"rawliteral(';

        // Display settings form handler
        document.getElementById('displayForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const formData = new FormData(e.target);
            const data = { rotation: parseInt(formData.get('rotation')) };

            const status = document.getElementById('displayStatus');

            try {
                const response = await fetch('/api/display', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });

                if (response.ok) {
                    status.className = 'status success';
                    status.textContent = 'Display settings saved! Restart the device to apply rotation changes.';
                    status.style.display = 'block';
                } else {
                    status.className = 'status error';
                    status.textContent = 'Failed to save display settings';
                    status.style.display = 'block';
                }
            } catch (error) {
                status.className = 'status error';
                status.textContent = 'Error: ' + error.message;
                status.style.display = 'block';
            }
        });

        // Brightness settings form handler
        document.getElementById('brightnessForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const formData = new FormData(e.target);
            const data = {
                dayBrightness: parseInt(formData.get('dayBrightness')),
                nightBrightness: parseInt(formData.get('nightBrightness')),
                dayStartHour: parseInt(formData.get('dayStartHour')),
                dayEndHour: parseInt(formData.get('dayEndHour')),
                dayIdleDimmingEnabled: document.getElementById('dayIdleDimmingEnabled').checked,
                nightIdleDimmingEnabled: document.getElementById('nightIdleDimmingEnabled').checked,
                idleTimeout: parseInt(formData.get('idleTimeout')),
                idleBrightness: parseInt(formData.get('idleBrightness'))
            };

            const status = document.getElementById('brightnessStatus');

            try {
                const response = await fetch('/api/brightness', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });

                if (response.ok) {
                    status.className = 'status success';
                    status.textContent = 'Brightness settings saved successfully!';
                    status.style.display = 'block';
                } else {
                    status.className = 'status error';
                    status.textContent = 'Failed to save brightness settings';
                    status.style.display = 'block';
                }
            } catch (error) {
                status.className = 'status error';
                status.textContent = 'Error: ' + error.message;
                status.style.display = 'block';
            }
        });

        // Time settings form handler
        document.getElementById('timeForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const formData = new FormData(e.target);
            const data = {
                ntpEnabled: document.getElementById('ntpEnabled').checked,
                ntpServer: formData.get('ntpServer'),
                timezone: formData.get('timezone')
            };

            const status = document.getElementById('timeStatus');

            try {
                const response = await fetch('/api/time', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });

                if (response.ok) {
                    status.className = 'status success';
                    status.textContent = 'Time settings saved! Syncing with NTP server...';
                    status.style.display = 'block';
                } else {
                    status.className = 'status error';
                    status.textContent = 'Failed to save time settings';
                    status.style.display = 'block';
                }
            } catch (error) {
                status.className = 'status error';
                status.textContent = 'Error: ' + error.message;
                status.style.display = 'block';
            }
        });

        // MQTT settings form handler
        document.getElementById('mqttForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const formData = new FormData(e.target);
            const data = Object.fromEntries(formData.entries());

            const status = document.getElementById('mqttStatus');

            try {
                const response = await fetch('/api/mqtt', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });

                if (response.ok) {
                    status.className = 'status success';
                    status.textContent = 'MQTT settings saved successfully! Reconnecting to MQTT...';
                    status.style.display = 'block';
                } else {
                    status.className = 'status error';
                    status.textContent = 'Failed to save MQTT settings';
                    status.style.display = 'block';
                }
            } catch (error) {
                status.className = 'status error';
                status.textContent = 'Error: ' + error.message;
                status.style.display = 'block';
            }
        });

        // EV settings form handler
        document.getElementById('evForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            const formData = new FormData(e.target);
            const data = {
                enabled: document.getElementById('evEnabled').checked,
                powerTopic: formData.get('evPowerTopic'),
                connectedTopic: formData.get('evConnectedTopic'),
                socTopic: formData.get('evSOCTopic')
            };

            const status = document.getElementById('evStatus');

            try {
                const response = await fetch('/api/ev', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });

                if (response.ok) {
                    status.className = 'status success';
                    status.textContent = 'EV settings saved successfully!';
                    status.style.display = 'block';
                } else {
                    status.className = 'status error';
                    status.textContent = 'Failed to save EV settings';
                    status.style.display = 'block';
                }
            } catch (error) {
                status.className = 'status error';
                status.textContent = 'Error: ' + error.message;
                status.style.display = 'block';
            }
        });

        // Screenshot capture handler
        document.getElementById('captureScreenshot').addEventListener('click', async () => {
            const status = document.getElementById('screenshotStatus');
            const captureBtn = document.getElementById('captureScreenshot');
            const downloadBtn = document.getElementById('downloadScreenshot');

            captureBtn.disabled = true;
            status.className = 'status';
            status.textContent = 'Capturing screenshot...';
            status.style.display = 'block';

            try {
                const response = await fetch('/api/screenshot/capture', {
                    method: 'POST'
                });

                if (response.ok) {
                    status.className = 'status success';
                    status.textContent = 'Screenshot captured successfully!';
                    downloadBtn.style.display = 'block';
                } else {
                    status.className = 'status error';
                    status.textContent = 'Failed to capture screenshot';
                }
            } catch (error) {
                status.className = 'status error';
                status.textContent = 'Error: ' + error.message;
            }

            captureBtn.disabled = false;
        });

        // Screenshot download handler
        document.getElementById('downloadScreenshot').addEventListener('click', () => {
            window.location.href = '/api/screenshot/download';
        });

        // Check if screenshot exists on page load
        fetch('/api/screenshot/status')
            .then(response => response.json())
            .then(data => {
                if (data.available) {
                    document.getElementById('downloadScreenshot').style.display = 'block';
                }
            })
            .catch(error => console.log('Could not check screenshot status:', error));
    </script>
</body>
</html>
)rawliteral";
    
    return html;
}
