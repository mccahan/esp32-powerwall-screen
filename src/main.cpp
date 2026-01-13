#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <Arduino_GFX_Library.h>
#include <improv.h>
#include <Preferences.h>

// Device info
#define DEVICE_NAME "Powerwall Display"
#define FIRMWARE_VERSION "1.0.0"
#define HARDWARE_VARIANT "ESP32-S3-4848S040"
#define DEVICE_URL "http://{LOCAL_IPV4}/"

// Backlight pin
#define GFX_BL 38

// Display configuration for Guition ESP32-S3-4848S040
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    39 /* CS */, 48 /* SCK */, 47 /* SDA */,
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    11 /* R0 */, 12 /* R1 */, 13 /* R2 */, 14 /* R3 */, 0 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    4 /* B0 */, 5 /* B1 */, 6 /* B2 */, 7 /* B3 */, 15 /* B4 */
);

Arduino_ST7701_RGBPanel *gfx = new Arduino_ST7701_RGBPanel(
    bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */,
    true /* IPS */, 480 /* width */, 480 /* height */,
    st7701_type1_init_operations, sizeof(st7701_type1_init_operations),
    true /* BGR */,
    10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */
);

// Display dimensions
#define TFT_WIDTH 480
#define TFT_HEIGHT 480

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

// UI labels for status updates
static lv_obj_t *status_label = nullptr;
static lv_obj_t *ip_label = nullptr;

// Improv WiFi state
static improv::State improv_state = improv::STATE_AUTHORIZED;
static improv::Error improv_error = improv::ERROR_NONE;
static uint8_t improv_buffer[256];
static size_t improv_buffer_pos = 0;

// WiFi connection state
static bool wifi_connecting = false;
static unsigned long wifi_connect_start = 0;
static const unsigned long WIFI_CONNECT_TIMEOUT = 30000; // 30 seconds

// Preferences for storing WiFi credentials
Preferences preferences;

// Forward declarations
void setupDisplay();
void setupLVGL();
void setupImprovWiFi();
void createUI();
void updateStatusUI(const char* status, uint32_t color = 0x00FF00);
void loopImprov();
void handleImprovCommand(improv::ImprovCommand cmd);
void sendImprovState();
void sendImprovError(improv::Error error);
void sendImprovRPCResponse(improv::Command cmd, const std::vector<String> &data);
void connectToWiFi(const char* ssid, const char* password);
void checkWiFiConnection();
String getLocalIP();

// LVGL display flush callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

void setup() {
    Serial.begin(115200);
    delay(100);

    // Setup display hardware first
    setupDisplay();

    // Initialize LVGL
    setupLVGL();

    // Create UI
    createUI();

    // Show initial status
    updateStatusUI("Initializing...", 0xFFFF00);
    lv_timer_handler();

    // Check PSRAM
    if (psramFound()) {
        Serial.printf("PSRAM found: %d bytes\n", ESP.getPsramSize());
    }

    // Setup Improv WiFi
    setupImprovWiFi();

    // Try to connect with saved credentials
    String saved_ssid = "";
    String saved_pass = "";

    // Open in read-write mode to create namespace if it doesn't exist
    if (preferences.begin("wifi", false)) {
        if (preferences.isKey("ssid")) {
            saved_ssid = preferences.getString("ssid", "");
            saved_pass = preferences.getString("password", "");
        }
        preferences.end();
    }

    if (saved_ssid.length() > 0) {
        updateStatusUI("Connecting to saved WiFi...", 0xFFFF00);
        lv_timer_handler();
        connectToWiFi(saved_ssid.c_str(), saved_pass.c_str());
    } else {
        updateStatusUI("Waiting for WiFi config\nUse ESP Web Tools", 0x00FFFF);
    }
}

void loop() {
    // Handle LVGL tasks
    lv_timer_handler();

    // Handle Improv WiFi serial protocol
    loopImprov();

    // Check WiFi connection progress
    checkWiFiConnection();

    delay(5);
}

void setupDisplay() {
    // Initialize the display with 16MHz clock
    gfx->begin(16000000);
    gfx->fillScreen(BLACK);

    // Setup backlight
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
}

void setupLVGL() {
    lv_init();

    // Allocate LVGL draw buffer from internal RAM
    size_t buf_size = TFT_WIDTH * 200;
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!disp_draw_buf) {
        disp_draw_buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * buf_size);
    }

    if (!disp_draw_buf) {
        while (1) { delay(1000); }
    }

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, buf_size);

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}

void setupImprovWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Set initial state - authorized and ready to receive credentials
    improv_state = improv::STATE_AUTHORIZED;
    sendImprovState();
}

void createUI() {
    // Create main screen
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_scr_load(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Title
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Powerwall Display");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    // Status label
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Initializing...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 0);

    // IP address label
    ip_label = lv_label_create(scr);
    lv_label_set_text(ip_label, "");
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ip_label, LV_ALIGN_CENTER, 0, 60);
}

void updateStatusUI(const char* status, uint32_t color) {
    if (status_label) {
        lv_label_set_text(status_label, status);
        lv_obj_set_style_text_color(status_label, lv_color_hex(color), 0);
    }
}

// ============== Improv WiFi Serial Protocol Implementation ==============

void loopImprov() {
    while (Serial.available()) {
        uint8_t byte = Serial.read();

        bool result = improv::parse_improv_serial_byte(
            improv_buffer_pos,
            byte,
            improv_buffer,
            [](improv::ImprovCommand cmd) -> bool {
                handleImprovCommand(cmd);
                return true;
            },
            [](improv::Error error) {
                sendImprovError(error);
            }
        );

        if (result) {
            improv_buffer[improv_buffer_pos++] = byte;

            // Reset buffer if it gets too large
            if (improv_buffer_pos >= sizeof(improv_buffer)) {
                improv_buffer_pos = 0;
            }
        } else {
            improv_buffer_pos = 0;
        }
    }
}

void handleImprovCommand(improv::ImprovCommand cmd) {
    switch (cmd.command) {
        case improv::WIFI_SETTINGS: {
            // Received WiFi credentials
            improv_state = improv::STATE_PROVISIONING;
            sendImprovState();

            updateStatusUI("Connecting to WiFi...", 0xFFFF00);
            lv_timer_handler();

            // Save credentials
            preferences.begin("wifi", false);
            preferences.putString("ssid", cmd.ssid.c_str());
            preferences.putString("password", cmd.password.c_str());
            preferences.end();

            // Connect to WiFi
            connectToWiFi(cmd.ssid.c_str(), cmd.password.c_str());
            break;
        }

        case improv::GET_CURRENT_STATE: {
            sendImprovState();

            // If already connected, send the URL
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
            // Scan for WiFi networks
            updateStatusUI("Scanning WiFi...", 0xFFFF00);
            lv_timer_handler();

            int n = WiFi.scanNetworks();
            std::vector<String> networks;

            for (int i = 0; i < n && i < 10; i++) {
                String network = WiFi.SSID(i);
                network += ",";
                network += String(WiFi.RSSI(i));
                network += ",";
                network += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "NO" : "YES";
                networks.push_back(network);
            }

            WiFi.scanDelete();
            sendImprovRPCResponse(improv::GET_WIFI_NETWORKS, networks);

            if (improv_state != improv::STATE_PROVISIONED) {
                updateStatusUI("Waiting for WiFi config\nUse ESP Web Tools", 0x00FFFF);
            }
            break;
        }

        default:
            sendImprovError(improv::ERROR_UNKNOWN_RPC);
            break;
    }

    // Reset buffer after handling command
    improv_buffer_pos = 0;
}

void sendImprovState() {
    uint8_t data[] = {
        'I', 'M', 'P', 'R', 'O', 'V',
        improv::IMPROV_SERIAL_VERSION,
        improv::TYPE_CURRENT_STATE,
        1,  // data length
        improv_state,
        0   // checksum placeholder
    };

    // Calculate checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(data) - 1; i++) {
        checksum += data[i];
    }
    data[sizeof(data) - 1] = checksum;

    Serial.write(data, sizeof(data));
}

void sendImprovError(improv::Error error) {
    improv_error = error;

    uint8_t data[] = {
        'I', 'M', 'P', 'R', 'O', 'V',
        improv::IMPROV_SERIAL_VERSION,
        improv::TYPE_ERROR_STATE,
        1,  // data length
        error,
        0   // checksum placeholder
    };

    // Calculate checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(data) - 1; i++) {
        checksum += data[i];
    }
    data[sizeof(data) - 1] = checksum;

    Serial.write(data, sizeof(data));
}

void sendImprovRPCResponse(improv::Command cmd, const std::vector<String> &data) {
    std::vector<uint8_t> response = improv::build_rpc_response(cmd, data, false);

    // Build full packet with header
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

    // Calculate and append checksum
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

        // Send success response with URL
        String ip = getLocalIP();
        std::vector<String> urls = {ip};
        sendImprovRPCResponse(improv::WIFI_SETTINGS, urls);

        // Update UI
        String status = "WiFi Connected!\nSSID: " + WiFi.SSID();
        updateStatusUI(status.c_str(), 0x00FF00);

        if (ip_label) {
            String ipText = "IP: " + ip;
            lv_label_set_text(ip_label, ipText.c_str());
        }

        Serial.printf("WiFi connected! IP: %s\n", ip.c_str());
    }
    else if (millis() - wifi_connect_start > WIFI_CONNECT_TIMEOUT) {
        wifi_connecting = false;
        improv_state = improv::STATE_AUTHORIZED;
        sendImprovState();
        sendImprovError(improv::ERROR_UNABLE_TO_CONNECT);

        updateStatusUI("WiFi connection failed!\nTry again via ESP Web Tools", 0xFF0000);

        Serial.println("WiFi connection timeout");
    }
}

String getLocalIP() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "";
}
