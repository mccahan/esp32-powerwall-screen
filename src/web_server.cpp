#include "web_server.h"
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
}

String PowerwallWebServer::getConfigPage() {
    MQTTConfig& config = mqttClient.getConfig();
    DisplayConfig& dispConfig = displayConfig.getConfig();
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
        select {
            width: 100%;
            padding: 10px;
            border: 1px solid #444;
            border-radius: 4px;
            background: #1a1a1a;
            color: #e0e0e0;
            box-sizing: border-box;
            font-size: 16px;
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

        <h2 class="section-title">MQTT Settings</h2>
        <form id="mqttForm">
            <div class="form-group">
                <label for="host">MQTT Host:</label>
                <input type="text" id="host" name="host" value=")rawliteral" + config.host + R"rawliteral(" required>
            </div>
            <div class="form-group">
                <label for="port">MQTT Port:</label>
                <input type="number" id="port" name="port" value=")rawliteral" + String(config.port) + R"rawliteral(" required>
            </div>
            <div class="form-group">
                <label for="user">MQTT Username:</label>
                <input type="text" id="user" name="user" value=")rawliteral" + config.user + R"rawliteral(">
            </div>
            <div class="form-group">
                <label for="password">MQTT Password:</label>
                <input type="password" id="password" name="password" placeholder="Enter new password or leave blank">
            </div>
            <div class="form-group">
                <label for="prefix">Topic Prefix:</label>
                <input type="text" id="prefix" name="prefix" value=")rawliteral" + config.topic_prefix + R"rawliteral(" placeholder="pypowerwall/" required>
            </div>
            <button type="submit" class="button">Save Configuration</button>
        </form>
        <div class="status" id="mqttStatus"></div>
        <div class="info">
            <strong>Note:</strong> Topic prefix should match your pypowerwall MQTT configuration (default: "pypowerwall/").
            Device will automatically reconnect to MQTT broker after saving.
        </div>
    </div>
    <script>
        // Set current rotation value
        document.getElementById('rotation').value = ')rawliteral" + String(currentRotation) + R"rawliteral(';

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
                    status.textContent = 'Configuration saved successfully! Reconnecting to MQTT...';
                    status.style.display = 'block';
                } else {
                    status.className = 'status error';
                    status.textContent = 'Failed to save configuration';
                    status.style.display = 'block';
                }
            } catch (error) {
                status.className = 'status error';
                status.textContent = 'Error: ' + error.message;
                status.style.display = 'block';
            }
        });
    </script>
</body>
</html>
)rawliteral";
    
    return html;
}
