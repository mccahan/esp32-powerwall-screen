#include "captive_portal.h"
#include "improv_wifi.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

// DNS server for captive portal redirect
static DNSServer dnsServer;
static const byte DNS_PORT = 53;

// Captive portal web server (separate from main web server)
static AsyncWebServer* portalServer = nullptr;

// Portal state
static bool portalActive = false;

// Forward declarations
static String getPortalPage();
static String getScanResultsJson();

void startCaptivePortal() {
    if (portalActive) return;

    Serial.println("Starting captive portal...");

    // Stop any existing WiFi connection
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    // Start Access Point
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("AP started: %s\n", AP_SSID);
    Serial.printf("AP IP address: %s\n", apIP.toString().c_str());

    // Start DNS server - redirect all domains to our IP
    dnsServer.start(DNS_PORT, "*", apIP);

    // Create and configure portal web server
    portalServer = new AsyncWebServer(80);

    // Captive portal detection endpoints (various OS/browsers check these)
    // Android
    portalServer->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
    // iOS/macOS
    portalServer->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
    // Windows
    portalServer->on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
    portalServer->on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });
    // Firefox
    portalServer->on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    // Main portal page
    portalServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", getPortalPage());
    });

    // WiFi scan endpoint
    portalServer->on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getScanResultsJson());
    });

    // WiFi connect endpoint
    portalServer->on("/connect", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Parse JSON body
            String body = String((char*)data).substring(0, len);

            // Simple JSON parsing for ssid and password
            int ssidStart = body.indexOf("\"ssid\":\"") + 8;
            int ssidEnd = body.indexOf("\"", ssidStart);
            int passStart = body.indexOf("\"password\":\"") + 12;
            int passEnd = body.indexOf("\"", passStart);

            if (ssidStart > 7 && ssidEnd > ssidStart) {
                String ssid = body.substring(ssidStart, ssidEnd);
                String password = "";
                if (passStart > 11 && passEnd > passStart) {
                    password = body.substring(passStart, passEnd);
                }

                Serial.printf("Captive portal: saving credentials for '%s'\n", ssid.c_str());

                // Save credentials
                wifi_preferences.begin("wifi", false);
                wifi_preferences.putString("ssid", ssid);
                wifi_preferences.putString("password", password);
                wifi_preferences.end();

                request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Credentials saved. Restarting...\"}");

                // Restart after a short delay to allow response to be sent
                delay(1000);
                ESP.restart();
            } else {
                request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
            }
        }
    );

    // Catch-all handler for captive portal redirect
    portalServer->onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    });

    portalServer->begin();
    portalActive = true;

    Serial.println("Captive portal ready");
    Serial.printf("Connect to WiFi network '%s' to configure\n", AP_SSID);
}

void stopCaptivePortal() {
    if (!portalActive) return;

    Serial.println("Stopping captive portal...");

    dnsServer.stop();

    if (portalServer) {
        portalServer->end();
        delete portalServer;
        portalServer = nullptr;
    }

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    portalActive = false;
    Serial.println("Captive portal stopped");
}

void loopCaptivePortal() {
    if (portalActive) {
        dnsServer.processNextRequest();
    }
}

bool isCaptivePortalActive() {
    return portalActive;
}

static String getScanResultsJson() {
    String json = "[";

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true); // Start async scan
        return "[]";
    } else if (n == WIFI_SCAN_RUNNING) {
        return "[]";
    } else if (n > 0) {
        for (int i = 0; i < n && i < 15; i++) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN) + "}";
        }
        WiFi.scanDelete();
        WiFi.scanNetworks(true); // Start new scan
    }

    json += "]";
    return json;
}

static String getPortalPage() {
    // Start a background scan
    if (WiFi.scanComplete() == WIFI_SCAN_FAILED) {
        WiFi.scanNetworks(true);
    }

    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Powerwall Display Setup</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            padding: 20px;
            color: #fff;
        }
        .container {
            max-width: 400px;
            margin: 0 auto;
        }
        h1 {
            text-align: center;
            margin-bottom: 10px;
            font-size: 24px;
        }
        .subtitle {
            text-align: center;
            color: #888;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .card {
            background: rgba(255,255,255,0.1);
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
        }
        .card h2 {
            font-size: 16px;
            margin-bottom: 15px;
            color: #4FC3F7;
        }
        .network-list {
            max-height: 200px;
            overflow-y: auto;
        }
        .network {
            display: flex;
            align-items: center;
            padding: 12px;
            background: rgba(255,255,255,0.05);
            border-radius: 8px;
            margin-bottom: 8px;
            cursor: pointer;
            transition: background 0.2s;
        }
        .network:hover {
            background: rgba(255,255,255,0.15);
        }
        .network.selected {
            background: rgba(79, 195, 247, 0.3);
            border: 1px solid #4FC3F7;
        }
        .network-name {
            flex: 1;
            font-size: 14px;
        }
        .network-signal {
            font-size: 12px;
            color: #888;
        }
        .network-lock {
            margin-left: 10px;
            font-size: 12px;
        }
        input[type="password"], input[type="text"] {
            width: 100%;
            padding: 12px;
            border: 1px solid rgba(255,255,255,0.2);
            border-radius: 8px;
            background: rgba(0,0,0,0.3);
            color: #fff;
            font-size: 16px;
            margin-bottom: 15px;
        }
        input:focus {
            outline: none;
            border-color: #4FC3F7;
        }
        button {
            width: 100%;
            padding: 14px;
            border: none;
            border-radius: 8px;
            background: #4FC3F7;
            color: #000;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.2s;
        }
        button:hover {
            background: #29B6F6;
        }
        button:disabled {
            background: #666;
            cursor: not-allowed;
        }
        .status {
            text-align: center;
            padding: 15px;
            border-radius: 8px;
            margin-top: 15px;
            display: none;
        }
        .status.success {
            background: rgba(76, 175, 80, 0.3);
            color: #81C784;
        }
        .status.error {
            background: rgba(244, 67, 54, 0.3);
            color: #E57373;
        }
        .status.info {
            background: rgba(33, 150, 243, 0.3);
            color: #64B5F6;
        }
        .manual-entry {
            margin-top: 15px;
            padding-top: 15px;
            border-top: 1px solid rgba(255,255,255,0.1);
        }
        .manual-toggle {
            color: #4FC3F7;
            background: none;
            border: none;
            padding: 0;
            font-size: 14px;
            cursor: pointer;
            text-decoration: underline;
            width: auto;
        }
        .manual-toggle:hover {
            color: #29B6F6;
            background: none;
        }
        .manual-fields {
            display: none;
            margin-top: 15px;
        }
        .manual-fields.show {
            display: block;
        }
        .refresh-btn {
            background: transparent;
            border: 1px solid #4FC3F7;
            color: #4FC3F7;
            padding: 8px 16px;
            font-size: 12px;
            margin-bottom: 15px;
        }
        .refresh-btn:hover {
            background: rgba(79, 195, 247, 0.1);
        }
        .loading {
            text-align: center;
            padding: 20px;
            color: #888;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Powerwall Display</h1>
        <p class="subtitle">WiFi Setup</p>

        <div class="card">
            <h2>Available Networks</h2>
            <button class="refresh-btn" onclick="scanNetworks()">Refresh</button>
            <div id="networkList" class="network-list">
                <div class="loading">Scanning for networks...</div>
            </div>
            <div class="manual-entry">
                <button class="manual-toggle" onclick="toggleManual()">Enter network manually</button>
                <div id="manualFields" class="manual-fields">
                    <input type="text" id="manualSsid" placeholder="Network name (SSID)">
                </div>
            </div>
        </div>

        <div class="card">
            <h2>Password</h2>
            <input type="password" id="password" placeholder="Enter WiFi password">
            <button id="connectBtn" onclick="connect()" disabled>Connect</button>
            <div id="status" class="status"></div>
        </div>
    </div>

    <script>
        let selectedSsid = '';
        let networks = [];

        function scanNetworks() {
            document.getElementById('networkList').innerHTML = '<div class="loading">Scanning...</div>';
            fetch('/scan')
                .then(r => r.json())
                .then(data => {
                    networks = data;
                    renderNetworks();
                })
                .catch(e => {
                    document.getElementById('networkList').innerHTML = '<div class="loading">Scan failed. Try again.</div>';
                });
        }

        function renderNetworks() {
            const list = document.getElementById('networkList');
            if (networks.length === 0) {
                list.innerHTML = '<div class="loading">No networks found. Try refreshing.</div>';
                return;
            }
            list.innerHTML = networks.map(n => `
                <div class="network ${selectedSsid === n.ssid ? 'selected' : ''}" onclick="selectNetwork('${n.ssid.replace(/'/g, "\\'")}')">
                    <span class="network-name">${n.ssid}</span>
                    <span class="network-signal">${n.rssi} dBm</span>
                    ${n.secure ? '<span class="network-lock">&#128274;</span>' : ''}
                </div>
            `).join('');
        }

        function selectNetwork(ssid) {
            selectedSsid = ssid;
            document.getElementById('manualSsid').value = '';
            document.getElementById('manualFields').classList.remove('show');
            renderNetworks();
            updateConnectButton();
            document.getElementById('password').focus();
        }

        function toggleManual() {
            const fields = document.getElementById('manualFields');
            fields.classList.toggle('show');
            if (fields.classList.contains('show')) {
                document.getElementById('manualSsid').focus();
                selectedSsid = '';
                renderNetworks();
            }
        }

        function updateConnectButton() {
            const manualSsid = document.getElementById('manualSsid').value;
            const ssid = manualSsid || selectedSsid;
            document.getElementById('connectBtn').disabled = !ssid;
        }

        function connect() {
            const manualSsid = document.getElementById('manualSsid').value;
            const ssid = manualSsid || selectedSsid;
            const password = document.getElementById('password').value;

            if (!ssid) return;

            const status = document.getElementById('status');
            const btn = document.getElementById('connectBtn');

            btn.disabled = true;
            btn.textContent = 'Connecting...';
            status.className = 'status info';
            status.textContent = 'Saving credentials and connecting...';
            status.style.display = 'block';

            fetch('/connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid, password })
            })
            .then(r => r.json())
            .then(data => {
                status.className = 'status success';
                status.textContent = 'Credentials saved! The device will now restart and connect to your network.';
                setTimeout(() => {
                    status.textContent += ' You can close this page.';
                }, 3000);
            })
            .catch(e => {
                status.className = 'status error';
                status.textContent = 'Failed to save. Please try again.';
                btn.disabled = false;
                btn.textContent = 'Connect';
            });
        }

        document.getElementById('manualSsid').addEventListener('input', updateConnectButton);
        document.getElementById('password').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') connect();
        });

        // Initial scan
        scanNetworks();
        // Refresh every 5 seconds
        setInterval(scanNetworks, 5000);
    </script>
</body>
</html>
)rawliteral";
}
