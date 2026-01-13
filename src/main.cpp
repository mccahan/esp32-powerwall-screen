#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <Arduino_GFX_Library.h>
#include <improv.h>
#include <Preferences.h>
#include <cmath>
#include "ui_assets/ui_assets.h"
#include "mqtt_client.h"
#include "web_server.h"

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

// UI Layout Positions (from powerwall-monitor.yml design)
#define LABEL_HEIGHT 28
#define LABEL_HEIGHT_LARGE 39

#define BATTERY_VAL_Y 345
#define SOLAR_VAL_Y 111
#define GRID_VAL_X 62
#define GRID_VAL_Y 240
#define GRID_VAL_WIDTH 100
#define HOME_VAL_X 318
#define HOME_VAL_Y 240
#define HOME_VAL_WIDTH 100
#define SOC_LABEL_Y 413
#define SOC_BAR_X 82
#define SOC_BAR_Y 454
#define SOC_BAR_WIDTH 316
#define SOC_BAR_HEIGHT 13
#define GRID_OFFLINE_X 77
#define GRID_OFFLINE_Y 159

// Theme colors (matching ESPHome config)
#define COLOR_BG        0x0A0C10
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRID      0x8A8A8A
#define COLOR_SOLAR     0xFFD54A
#define COLOR_HOME      0x4FC3F7
#define COLOR_BATTERY   0x64DD17
#define COLOR_BAR_BG    0x16181C
#define COLOR_BAR_FILL  0x22C55E

// LVGL display buffers (double buffered)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf1;
static lv_color_t *disp_draw_buf2;
static lv_disp_drv_t disp_drv;

// UI elements - Main dashboard
static lv_obj_t *main_screen = nullptr;
static lv_obj_t *boot_screen = nullptr;
static lv_obj_t *boot_spinner = nullptr;

// Power value labels
static lv_obj_t *lbl_solar_val = nullptr;
static lv_obj_t *lbl_grid_val = nullptr;
static lv_obj_t *lbl_home_val = nullptr;
static lv_obj_t *lbl_batt_val = nullptr;

// SOC elements
static lv_obj_t *lbl_soc = nullptr;
static lv_obj_t *bar_soc = nullptr;

// Layout background and overlays
static lv_obj_t *img_layout = nullptr;
static lv_obj_t *img_grid_offline = nullptr;

// Status/WiFi label
static lv_obj_t *lbl_status = nullptr;

// Data RX indicator dot
static lv_obj_t *dot_data_rx = nullptr;

// Timing variables for pulse animation
static unsigned long last_data_ms = 0;
static unsigned long last_pulse_ms = 0;

// Improv WiFi state
static improv::State improv_state = improv::STATE_AUTHORIZED;
static uint8_t improv_buffer[256];
static size_t improv_buffer_pos = 0;

// WiFi connection state
static bool wifi_connecting = false;
static unsigned long wifi_connect_start = 0;
static const unsigned long WIFI_CONNECT_TIMEOUT = 30000;

// Boot screen timeout
static unsigned long boot_start_time = 0;
static const unsigned long BOOT_SCREEN_TIMEOUT = 5000; // 5 seconds

// LVGL tick tracking
static unsigned long last_tick = 0;

// Preferences for storing WiFi credentials
Preferences preferences;

// Forward declarations
void setupDisplay();
void setupLVGL();
void setupImprovWiFi();
void createUI();
void createBootScreen();
void createMainDashboard();
void showBootScreen();
void hideBootScreen();
void updateStatusLabel(const char* status);
void loopImprov();
void handleImprovCommand(improv::ImprovCommand cmd);
void sendImprovState();
void sendImprovError(improv::Error error);
void sendImprovRPCResponse(improv::Command cmd, const std::vector<String> &data);
void connectToWiFi(const char* ssid, const char* password);
void checkWiFiConnection();
String getLocalIP();
void updateDataRxPulse();

// Power value update functions (forward declarations)
void updateSolarValue(float watts);
void updateGridValue(float watts);
void updateHomeValue(float watts);
void updateBatteryValue(float watts);
void updateSOC(float soc_percent);

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

    // Show boot screen initially
    showBootScreen();
    boot_start_time = millis();
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

    if (preferences.begin("wifi", false)) {
        if (preferences.isKey("ssid")) {
            saved_ssid = preferences.getString("ssid", "");
            saved_pass = preferences.getString("password", "");
        }
        preferences.end();
    }

    if (saved_ssid.length() > 0) {
        updateStatusLabel("Connecting to WiFi...");
        lv_timer_handler();
        connectToWiFi(saved_ssid.c_str(), saved_pass.c_str());
    } else {
        updateStatusLabel("Configure WiFi via\nESP Web Tools");
    }
    
    // Setup MQTT callbacks
    mqttClient.setSolarCallback(updateSolarValue);
    mqttClient.setGridCallback(updateGridValue);
    mqttClient.setHomeCallback(updateHomeValue);
    mqttClient.setBatteryCallback(updateBatteryValue);
    mqttClient.setSOCCallback(updateSOC);
    
    // Initialize MQTT client (will load config from flash)
    mqttClient.begin();
    
    // Start web server (will be accessible after WiFi connects)
    webServer.begin();
}

void loop() {
    // Update LVGL tick for animations
    unsigned long now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    lv_timer_handler();
    loopImprov();
    checkWiFiConnection();
    updateDataRxPulse();

    // Check boot screen timeout
    if (boot_screen && !lv_obj_has_flag(boot_screen, LV_OBJ_FLAG_HIDDEN)) {
        if (millis() - boot_start_time > BOOT_SCREEN_TIMEOUT) {
            hideBootScreen();
        }
    }

    delay(5);
}

void setupDisplay() {
    // Lower pixel clock (8MHz) reduces tearing by giving more time between refreshes
    gfx->begin(8000000);
    gfx->fillScreen(BLACK);
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
}

void setupLVGL() {
    lv_init();

    // Full frame double buffers in PSRAM for smooth updates
    size_t buf_size = TFT_WIDTH * TFT_HEIGHT;

    disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!disp_draw_buf1 || !disp_draw_buf2) {
        Serial.println("Failed to allocate display buffers!");
        while (1) { delay(1000); }
    }

    // Double buffering: LVGL draws to one buffer while we copy the other to display
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, buf_size);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;  // Always send full frame to reduce partial update tearing
    lv_disp_drv_register(&disp_drv);
}

void setupImprovWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    improv_state = improv::STATE_AUTHORIZED;
    sendImprovState();
}

void createUI() {
    createMainDashboard();
    createBootScreen();
}

void createMainDashboard() {
    // Main screen with dark background
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(main_screen);

    // ========== Layout Background Image (must be created FIRST so labels appear on top) ==========
    img_layout = lv_img_create(main_screen);
    lv_img_set_src(img_layout, &layout_img);
    lv_obj_set_pos(img_layout, 0, 0);
    lv_obj_set_size(img_layout, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_clear_flag(img_layout, LV_OBJ_FLAG_SCROLLABLE);

    // ========== POWER VALUE LABELS (using custom font space_bold_21) ==========
    // Battery value - centered at bottom
    lbl_batt_val = lv_label_create(main_screen);
    lv_label_set_text(lbl_batt_val, "0.0 kW");
    lv_obj_set_style_text_color(lbl_batt_val, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_batt_val, &space_bold_21, 0);
    lv_obj_set_style_text_align(lbl_batt_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_batt_val, 0, BATTERY_VAL_Y);
    lv_obj_set_width(lbl_batt_val, TFT_WIDTH);
    lv_obj_set_height(lbl_batt_val, LABEL_HEIGHT);

    // Solar value - centered at top
    lbl_solar_val = lv_label_create(main_screen);
    lv_label_set_text(lbl_solar_val, "0.0 kW");
    lv_obj_set_style_text_color(lbl_solar_val, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_solar_val, &space_bold_21, 0);
    lv_obj_set_style_text_align(lbl_solar_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_solar_val, 0, SOLAR_VAL_Y);
    lv_obj_set_width(lbl_solar_val, TFT_WIDTH);
    lv_obj_set_height(lbl_solar_val, LABEL_HEIGHT);

    // Grid value - left side
    lbl_grid_val = lv_label_create(main_screen);
    lv_label_set_text(lbl_grid_val, "0.0 kW");
    lv_obj_set_style_text_color(lbl_grid_val, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_grid_val, &space_bold_21, 0);
    lv_obj_set_style_text_align(lbl_grid_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_grid_val, GRID_VAL_X, GRID_VAL_Y);
    lv_obj_set_width(lbl_grid_val, GRID_VAL_WIDTH);
    lv_obj_set_height(lbl_grid_val, LABEL_HEIGHT);

    // Home value - right side
    lbl_home_val = lv_label_create(main_screen);
    lv_label_set_text(lbl_home_val, "0.0 kW");
    lv_obj_set_style_text_color(lbl_home_val, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_home_val, &space_bold_21, 0);
    lv_obj_set_style_text_align(lbl_home_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_home_val, HOME_VAL_X, HOME_VAL_Y);
    lv_obj_set_width(lbl_home_val, HOME_VAL_WIDTH);
    lv_obj_set_height(lbl_home_val, LABEL_HEIGHT);

    // ========== SOC Label (using custom font space_bold_30) ==========
    // SOC percentage - centered above battery bar
    lbl_soc = lv_label_create(main_screen);
    lv_label_set_text(lbl_soc, "0%");
    lv_obj_set_style_text_color(lbl_soc, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_soc, &space_bold_30, 0);
    lv_obj_set_style_text_align(lbl_soc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(lbl_soc, 0, SOC_LABEL_Y);
    lv_obj_set_width(lbl_soc, TFT_WIDTH);
    lv_obj_set_height(lbl_soc, LABEL_HEIGHT_LARGE);

    // ========== SOC Bar ==========
    bar_soc = lv_bar_create(main_screen);
    lv_obj_set_size(bar_soc, SOC_BAR_WIDTH, SOC_BAR_HEIGHT);
    lv_obj_set_pos(bar_soc, SOC_BAR_X, SOC_BAR_Y);
    lv_bar_set_range(bar_soc, 0, 100);
    lv_bar_set_value(bar_soc, 0, LV_ANIM_OFF);

    // Bar background style
    lv_obj_set_style_bg_color(bar_soc, lv_color_hex(COLOR_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_soc, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_soc, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_soc, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_soc, 2, LV_PART_MAIN);

    // Bar indicator style
    lv_obj_set_style_bg_color(bar_soc, lv_color_hex(COLOR_BAR_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_soc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_soc, 2, LV_PART_INDICATOR);

    // ========== Grid Offline Overlay ==========
    img_grid_offline = lv_img_create(main_screen);
    lv_img_set_src(img_grid_offline, &grid_offline_img);
    lv_obj_set_pos(img_grid_offline, GRID_OFFLINE_X, GRID_OFFLINE_Y);
    lv_obj_add_flag(img_grid_offline, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // ========== Status/WiFi Label ==========
    lbl_status = lv_label_create(main_screen);
    lv_label_set_text(lbl_status, "");
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x6A6A6A), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_status, TFT_WIDTH);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -10);

    // ========== Data RX Indicator Dot ==========
    dot_data_rx = lv_obj_create(main_screen);
    lv_obj_set_size(dot_data_rx, 10, 10);
    lv_obj_set_pos(dot_data_rx, 460, 10);
    lv_obj_set_style_radius(dot_data_rx, 5, 0);
    lv_obj_set_style_bg_color(dot_data_rx, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(dot_data_rx, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dot_data_rx, 0, 0);
    lv_obj_add_flag(dot_data_rx, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dot_data_rx, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(dot_data_rx, LV_OBJ_FLAG_SCROLLABLE);
}

void createBootScreen() {
    // Boot screen overlay
    boot_screen = lv_obj_create(main_screen);
    lv_obj_set_size(boot_screen, 480, 480);
    lv_obj_set_pos(boot_screen, 0, 0);
    lv_obj_set_style_bg_color(boot_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(boot_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(boot_screen, 0, 0);
    lv_obj_set_style_radius(boot_screen, 0, 0);
    lv_obj_clear_flag(boot_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title on boot screen
    lv_obj_t *boot_title = lv_label_create(boot_screen);
    lv_label_set_text(boot_title, "Powerwall Display");
    lv_obj_set_style_text_color(boot_title, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(boot_title, &lv_font_montserrat_32, 0);
    lv_obj_align(boot_title, LV_ALIGN_CENTER, 0, -80);

    // Spinner
    boot_spinner = lv_spinner_create(boot_screen, 1000, 60);
    lv_obj_set_size(boot_spinner, 80, 80);
    lv_obj_align(boot_spinner, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_arc_color(boot_spinner, lv_color_hex(0x313336), LV_PART_MAIN);
    lv_obj_set_style_arc_width(boot_spinner, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(boot_spinner, lv_color_hex(0x137FEC), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(boot_spinner, 10, LV_PART_INDICATOR);

    // Status label on boot screen
    lv_obj_t *boot_status = lv_label_create(boot_screen);
    lv_label_set_text(boot_status, "Connecting...");
    lv_obj_set_style_text_color(boot_status, lv_color_hex(0x6A6A6A), 0);
    lv_obj_set_style_text_font(boot_status, &lv_font_montserrat_16, 0);
    lv_obj_align(boot_status, LV_ALIGN_CENTER, 0, 100);
}

void showBootScreen() {
    if (boot_screen) {
        lv_obj_clear_flag(boot_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void hideBootScreen() {
    if (boot_screen) {
        lv_obj_add_flag(boot_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void updateStatusLabel(const char* status) {
    if (lbl_status) {
        lv_label_set_text(lbl_status, status);
    }
}

// ============== Improv WiFi Serial Protocol Implementation ==============

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

void handleImprovCommand(improv::ImprovCommand cmd) {
    switch (cmd.command) {
        case improv::WIFI_SETTINGS: {
            improv_state = improv::STATE_PROVISIONING;
            sendImprovState();

            updateStatusLabel("Connecting to WiFi...");
            lv_timer_handler();

            preferences.begin("wifi", false);
            preferences.putString("ssid", cmd.ssid.c_str());
            preferences.putString("password", cmd.password.c_str());
            preferences.end();

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
            updateStatusLabel("Scanning WiFi...");
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

            if (improv_state != improv::STATE_PROVISIONED) {
                updateStatusLabel("Configure WiFi via\nESP Web Tools");
            }
            break;
        }

        default:
            sendImprovError(improv::ERROR_UNKNOWN_RPC);
            break;
    }

    improv_buffer_pos = 0;
}

void sendImprovState() {
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

void sendImprovError(improv::Error error) {
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

void sendImprovRPCResponse(improv::Command cmd, const std::vector<String> &data) {
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

        // Hide boot screen and show dashboard
        hideBootScreen();

        String status = "WiFi: " + WiFi.SSID() + " | Config: http://" + ip;
        updateStatusLabel(status.c_str());

        Serial.printf("WiFi connected! IP: %s\n", ip.c_str());
        
        // Now that WiFi is connected, connect to MQTT broker
        mqttClient.connect();
    }
    else if (millis() - wifi_connect_start > WIFI_CONNECT_TIMEOUT) {
        wifi_connecting = false;
        improv_state = improv::STATE_AUTHORIZED;
        sendImprovState();
        sendImprovError(improv::ERROR_UNABLE_TO_CONNECT);

        updateStatusLabel("WiFi failed - Configure via ESP Web Tools");
        Serial.println("WiFi connection timeout");
    }
}

String getLocalIP() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "";
}

// ============== Data RX Pulse Animation ==============

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void updateDataRxPulse() {
    if (!dot_data_rx) return;
    
    const unsigned long now = millis();
    const unsigned long since_data = now - last_data_ms;
    const unsigned long since_pulse = now - last_pulse_ms;
    
    // Start a new pulse only if:
    //  - data arrived recently (within 200ms)
    //  - AND at least 1 second has passed since the last pulse started
    if (since_data <= 200 && since_pulse >= 1000) {
        last_pulse_ms = now;
        lv_obj_clear_flag(dot_data_rx, LV_OBJ_FLAG_HIDDEN);
    }
    
    const unsigned long pulse_age = now - last_pulse_ms;
    
    // Pulse lasts 900ms
    if (pulse_age <= 900) {
        float t = (float)pulse_age / 900.0f;  // 0..1
        float s = sin(M_PI * t);
        float a = s * s;  // smooth single pulse
        
        // Add visibility floor so dot doesn't completely disappear
        a = 0.15f + 0.85f * a;
        
        // LVGL opacity range is 0-255, so scale accordingly
        lv_opa_t opacity = (lv_opa_t)((int)(a * 255.0f));
        lv_obj_set_style_bg_opa(dot_data_rx, opacity, 0);
    } else {
        lv_obj_add_flag(dot_data_rx, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============== Power Value Update Functions ==============
// These will be called when MQTT data arrives

void updateSolarValue(float watts) {
    if (lbl_solar_val) {
        char buf[24];
        float kw = watts / 1000.0f;
        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_solar_val, buf);
    }
    last_data_ms = millis();
}

void updateGridValue(float watts) {
    if (lbl_grid_val) {
        char buf[24];
        float kw = watts / 1000.0f;
        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_grid_val, buf);
    }
    last_data_ms = millis();
}

void updateHomeValue(float watts) {
    if (lbl_home_val) {
        char buf[24];
        float kw = watts / 1000.0f;
        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_home_val, buf);
    }
    last_data_ms = millis();
}

void updateBatteryValue(float watts) {
    if (lbl_batt_val) {
        char buf[24];
        float kw = watts / 1000.0f;
        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_batt_val, buf);
    }
    last_data_ms = millis();
}

void updateSOC(float soc_percent) {
    // Adjust for 5% reserve like ESPHome config
    float adjusted = (soc_percent / 0.95f) - (5.0f / 0.95f);
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 100) adjusted = 100;

    if (lbl_soc) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)roundf(adjusted));
        lv_label_set_text(lbl_soc, buf);
    }

    if (bar_soc) {
        lv_bar_set_value(bar_soc, (int)roundf(adjusted), LV_ANIM_OFF);
    }
    
    last_data_ms = millis();
}
