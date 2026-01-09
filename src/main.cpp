#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "config.h"

// ImprovWiFi support
#define IMPROV_SERIAL_VERSION 1
#include <ImprovWiFiLibrary.h>

ImprovWiFi improvSerial(&Serial);

// Configuration storage
Preferences preferences;
String mqtt_server = "";
String mqtt_port = "1883";
String mqtt_user = "";
String mqtt_password = "";
String mqtt_prefix = DEFAULT_MQTT_PREFIX;

// Full MQTT topic strings
String topic_grid_power;
String topic_solar_power;
String topic_battery_power;
String topic_battery_level;
String topic_home_power;

// Power data structure
struct PowerData {
    float grid_power = 0;      // Positive = importing, Negative = exporting
    float solar_power = 0;     // Always positive
    float battery_power = 0;   // Positive = discharging, Negative = charging
    float battery_level = 0;   // Percentage 0-100
    float home_power = 0;      // Always positive
};

PowerData powerData;

// Display and LVGL objects
Arduino_ESP32RGBPanel *bus = nullptr;
Arduino_RGB_Display *gfx = nullptr;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

// UI elements
lv_obj_t *screen;
lv_obj_t *grid_label;
lv_obj_t *solar_label;
lv_obj_t *battery_label;
lv_obj_t *home_label;
lv_obj_t *grid_power_label;
lv_obj_t *solar_power_label;
lv_obj_t *battery_power_label;
lv_obj_t *home_power_label;
lv_obj_t *battery_level_label;

// Flow line objects
lv_obj_t *grid_to_home_line;
lv_obj_t *solar_to_home_line;
lv_obj_t *solar_to_battery_line;
lv_obj_t *solar_to_grid_line;
lv_obj_t *battery_to_home_line;
lv_obj_t *battery_to_grid_line;

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastUIUpdate = 0;
bool shouldSaveConfig = false;

// Forward declarations
void setupDisplay();
void setupLVGL();
void createUI();
void updateUI();
void setupWiFiManager();
void saveConfigCallback();
void loadConfig();
void saveConfig();
void buildMqttTopics();
void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool reconnectMQTT();
void drawFlowLine(lv_obj_t* line, int x1, int y1, int x2, int y2, lv_color_t color, bool active);
void onImprovWiFiErrorCb(ImprovTypes::Error err);
void onImprovWiFiConnectedCb(const char *ssid, const char *password);

// ImprovWiFi callbacks
void onImprovWiFiErrorCb(ImprovTypes::Error err) {
    Serial.printf("ImprovWiFi Error: %d\n", err);
}

void onImprovWiFiConnectedCb(const char *ssid, const char *password) {
    Serial.printf("ImprovWiFi: Connecting to %s...\n", ssid);
    WiFi.begin(ssid, password);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime < WIFI_CONNECT_TIMEOUT_MS)) {
        delay(WIFI_CONNECT_RETRY_DELAY_MS);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nImprovWiFi: Connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        // Save WiFi credentials to preferences
        preferences.begin("powerwall", false);
        preferences.putString("wifi_ssid", ssid);
        preferences.putString("wifi_pass", password);
        preferences.end();
        
        improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_S3, 
                                   "Powerwall Display", 
                                   "1.0.0", 
                                   "ESP32-S3 Powerwall",
                                   "http://" + WiFi.localIP().toString());
    } else {
        Serial.println("\nImprovWiFi: Connection failed!");
        improvSerial.setError(ImprovTypes::Error::ERROR_UNABLE_TO_CONNECT);
    }
}

// Display flushing
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);

    lv_disp_flush_ready(disp);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32-S3 Powerwall Display Starting...");

    // Setup backlight
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);

    // Initialize display
    setupDisplay();
    
    // Initialize LVGL
    setupLVGL();
    
    // Create UI
    createUI();
    
    // Initialize ImprovWiFi
    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_S3, 
                               "Powerwall Display", 
                               "1.0.0", 
                               "ESP32-S3 Powerwall",
                               "");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
    
    // Check for ImprovWiFi provisioning via serial
    Serial.println("Checking for ImprovWiFi provisioning...");
    unsigned long improvStart = millis();
    bool improvProvisioned = false;
    
    while (millis() - improvStart < IMPROV_PROVISIONING_TIMEOUT_MS) {
        improvSerial.handleSerial();
        if (WiFi.status() == WL_CONNECTED) {
            improvProvisioned = true;
            Serial.println("ImprovWiFi provisioning successful!");
            break;
        }
        delay(10);
    }
    
    // Load saved configuration
    loadConfig();
    
    // Build MQTT topics from prefix
    buildMqttTopics();
    
    // If not provisioned via ImprovWiFi, use WiFiManager
    if (!improvProvisioned) {
        // Setup WiFi with captive portal
        setupWiFiManager();
    }
    
    // Setup MQTT
    setupMQTT();
    
    Serial.println("Setup complete!");
}

void loop() {
    // Handle ImprovWiFi serial commands
    improvSerial.handleSerial();
    
    // Handle MQTT connection
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastMqttReconnectAttempt > 5000) {
            lastMqttReconnectAttempt = now;
            if (reconnectMQTT()) {
                lastMqttReconnectAttempt = 0;
            }
        }
    } else {
        mqttClient.loop();
    }
    
    // Update UI periodically
    unsigned long now = millis();
    if (now - lastUIUpdate > 100) {  // Update every 100ms
        lastUIUpdate = now;
        updateUI();
    }
    
    lv_timer_handler();
    delay(5);
}

void setupDisplay() {
    Serial.println("Initializing display...");
    
    bus = new Arduino_ESP32RGBPanel(
        TFT_DE, TFT_VSYNC, TFT_HSYNC, TFT_PCLK,
        TFT_D0, TFT_D1, TFT_D2, TFT_D3, TFT_D4,
        TFT_D5, TFT_D6, TFT_D7, TFT_D8, TFT_D9,
        TFT_D10, TFT_D11, TFT_D12, TFT_D13, TFT_D14,
        TFT_D15,
        1 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
        1 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
        1 /* pclk_active_neg */, 9000000 /* prefer_speed */, true /* auto_flush */
    );

    gfx = new Arduino_RGB_Display(
        SCREEN_WIDTH, SCREEN_HEIGHT,
        bus, 0 /* rotation */, false /* auto_flush */
    );

    gfx->begin();
    gfx->fillScreen(BLACK);
    
    Serial.println("Display initialized");
}

void setupLVGL() {
    Serial.println("Initializing LVGL...");
    
    lv_init();

    // Allocate LVGL draw buffer
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * SCREEN_WIDTH * 40, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        Serial.println("LVGL disp_draw_buf allocate failed!");
    } else {
        lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, SCREEN_WIDTH * 40);

        /* Initialize the display driver */
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = SCREEN_WIDTH;
        disp_drv.ver_res = SCREEN_HEIGHT;
        disp_drv.flush_cb = my_disp_flush;
        disp_drv.draw_buf = &draw_buf;
        lv_disp_drv_register(&disp_drv);

        Serial.println("LVGL initialized");
    }
}

void createUI() {
    Serial.println("Creating UI...");
    
    screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    
    // Define positions for the 4 components in a circular layout
    int center_x = SCREEN_WIDTH / 2;
    int center_y = SCREEN_HEIGHT / 2;
    int radius = 140;  // Distance from center
    
    // Grid on left (white) - 180 degrees
    int grid_x = center_x - radius;
    int grid_y = center_y;
    
    // Solar on top (yellow) - 90 degrees  
    int solar_x = center_x;
    int solar_y = center_y - radius;
    
    // Battery on bottom (green) - 270 degrees
    int battery_x = center_x;
    int battery_y = center_y + radius;
    
    // Home on right (blue) - 0 degrees
    int home_x = center_x + radius;
    int home_y = center_y;
    
    // Component size
    int comp_size = 80;
    
    // Create Grid component (left, white)
    lv_obj_t *grid_box = lv_obj_create(screen);
    lv_obj_set_size(grid_box, comp_size, comp_size);
    lv_obj_set_pos(grid_box, grid_x - comp_size/2, grid_y - comp_size/2);
    lv_obj_set_style_bg_color(grid_box, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_color(grid_box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(grid_box, 3, 0);
    lv_obj_set_style_radius(grid_box, 10, 0);
    
    grid_label = lv_label_create(grid_box);
    lv_label_set_text(grid_label, "GRID");
    lv_obj_set_style_text_color(grid_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(grid_label, &lv_font_montserrat_16, 0);
    lv_obj_center(grid_label);
    
    grid_power_label = lv_label_create(screen);
    lv_label_set_text(grid_power_label, "0 W");
    lv_obj_set_style_text_color(grid_power_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(grid_power_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(grid_power_label, grid_x - comp_size/2 - 20, grid_y + comp_size/2 + 5);
    
    // Create Solar component (top, yellow)
    lv_obj_t *solar_box = lv_obj_create(screen);
    lv_obj_set_size(solar_box, comp_size, comp_size);
    lv_obj_set_pos(solar_box, solar_x - comp_size/2, solar_y - comp_size/2);
    lv_obj_set_style_bg_color(solar_box, lv_color_hex(0x333300), 0);
    lv_obj_set_style_border_color(solar_box, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_border_width(solar_box, 3, 0);
    lv_obj_set_style_radius(solar_box, 10, 0);
    
    solar_label = lv_label_create(solar_box);
    lv_label_set_text(solar_label, "SOLAR");
    lv_obj_set_style_text_color(solar_label, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(solar_label, &lv_font_montserrat_16, 0);
    lv_obj_center(solar_label);
    
    solar_power_label = lv_label_create(screen);
    lv_label_set_text(solar_power_label, "0 W");
    lv_obj_set_style_text_color(solar_power_label, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(solar_power_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(solar_power_label, solar_x - 30, solar_y - comp_size/2 - 30);
    
    // Create Battery component (bottom, green)
    lv_obj_t *battery_box = lv_obj_create(screen);
    lv_obj_set_size(battery_box, comp_size, comp_size);
    lv_obj_set_pos(battery_box, battery_x - comp_size/2, battery_y - comp_size/2);
    lv_obj_set_style_bg_color(battery_box, lv_color_hex(0x003300), 0);
    lv_obj_set_style_border_color(battery_box, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(battery_box, 3, 0);
    lv_obj_set_style_radius(battery_box, 10, 0);
    
    battery_label = lv_label_create(battery_box);
    lv_label_set_text(battery_label, "BATT");
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
    lv_obj_align(battery_label, LV_ALIGN_CENTER, 0, -10);
    
    battery_level_label = lv_label_create(battery_box);
    lv_label_set_text(battery_level_label, "0%");
    lv_obj_set_style_text_color(battery_level_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(battery_level_label, &lv_font_montserrat_12, 0);
    lv_obj_align(battery_level_label, LV_ALIGN_CENTER, 0, 10);
    
    battery_power_label = lv_label_create(screen);
    lv_label_set_text(battery_power_label, "0 W");
    lv_obj_set_style_text_color(battery_power_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(battery_power_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(battery_power_label, battery_x - 30, battery_y + comp_size/2 + 5);
    
    // Create Home component (right, blue)
    lv_obj_t *home_box = lv_obj_create(screen);
    lv_obj_set_size(home_box, comp_size, comp_size);
    lv_obj_set_pos(home_box, home_x - comp_size/2, home_y - comp_size/2);
    lv_obj_set_style_bg_color(home_box, lv_color_hex(0x000033), 0);
    lv_obj_set_style_border_color(home_box, lv_color_hex(0x0080FF), 0);
    lv_obj_set_style_border_width(home_box, 3, 0);
    lv_obj_set_style_radius(home_box, 10, 0);
    
    home_label = lv_label_create(home_box);
    lv_label_set_text(home_label, "HOME");
    lv_obj_set_style_text_color(home_label, lv_color_hex(0x0080FF), 0);
    lv_obj_set_style_text_font(home_label, &lv_font_montserrat_16, 0);
    lv_obj_center(home_label);
    
    home_power_label = lv_label_create(screen);
    lv_label_set_text(home_power_label, "0 W");
    lv_obj_set_style_text_color(home_power_label, lv_color_hex(0x0080FF), 0);
    lv_obj_set_style_text_font(home_power_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(home_power_label, home_x + comp_size/2 - 20, home_y - 15);
    
    // Create center info display
    lv_obj_t *center_label = lv_label_create(screen);
    lv_label_set_text(center_label, "Powerwall");
    lv_obj_set_style_text_color(center_label, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(center_label, &lv_font_montserrat_14, 0);
    lv_obj_align(center_label, LV_ALIGN_CENTER, 0, 0);
    
    // Create flow lines (will be updated in updateUI)
    grid_to_home_line = lv_line_create(screen);
    solar_to_home_line = lv_line_create(screen);
    solar_to_battery_line = lv_line_create(screen);
    solar_to_grid_line = lv_line_create(screen);
    battery_to_home_line = lv_line_create(screen);
    battery_to_grid_line = lv_line_create(screen);
    
    // Initialize all lines as hidden
    lv_obj_add_flag(grid_to_home_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(solar_to_home_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(solar_to_battery_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(solar_to_grid_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(battery_to_home_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(battery_to_grid_line, LV_OBJ_FLAG_HIDDEN);
    
    Serial.println("UI created");
}

void updateUI() {
    char buf[32];
    
    // Update Grid
    snprintf(buf, sizeof(buf), "%.1f W", powerData.grid_power);
    lv_label_set_text(grid_power_label, buf);
    
    // Update Solar
    snprintf(buf, sizeof(buf), "%.1f W", powerData.solar_power);
    lv_label_set_text(solar_power_label, buf);
    
    // Update Battery
    snprintf(buf, sizeof(buf), "%.1f W", powerData.battery_power);
    lv_label_set_text(battery_power_label, buf);
    
    snprintf(buf, sizeof(buf), "%.1f%%", powerData.battery_level);
    lv_label_set_text(battery_level_label, buf);
    
    // Update Home
    snprintf(buf, sizeof(buf), "%.1f W", powerData.home_power);
    lv_label_set_text(home_power_label, buf);
    
    // Update flow lines based on power flows
    int center_x = SCREEN_WIDTH / 2;
    int center_y = SCREEN_HEIGHT / 2;
    int radius = 140;
    int comp_size = 80;
    
    // Component center positions
    int grid_x = center_x - radius;
    int grid_y = center_y;
    int solar_x = center_x;
    int solar_y = center_y - radius;
    int battery_x = center_x;
    int battery_y = center_y + radius;
    int home_x = center_x + radius;
    int home_y = center_y;
    
    // Grid to Home (importing from grid)
    if (powerData.grid_power > 50) {
        drawFlowLine(grid_to_home_line, grid_x + comp_size/2, grid_y, home_x - comp_size/2, home_y, lv_color_hex(0xFFFFFF), true);
    } else {
        lv_obj_add_flag(grid_to_home_line, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Solar to Home
    if (powerData.solar_power > 50 && powerData.home_power > 0) {
        drawFlowLine(solar_to_home_line, solar_x + comp_size/3, solar_y + comp_size/2, home_x - comp_size/3, home_y - comp_size/3, lv_color_hex(0xFFFF00), true);
    } else {
        lv_obj_add_flag(solar_to_home_line, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Battery to Home (battery discharging)
    if (powerData.battery_power > 50) {
        drawFlowLine(battery_to_home_line, battery_x + comp_size/3, battery_y - comp_size/2, home_x - comp_size/3, home_y + comp_size/3, lv_color_hex(0x00FF00), true);
    } else {
        lv_obj_add_flag(battery_to_home_line, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Solar to Battery (battery charging from solar)
    if (powerData.battery_power < -50 && powerData.solar_power > 0) {
        drawFlowLine(solar_to_battery_line, solar_x, solar_y + comp_size/2, battery_x, battery_y - comp_size/2, lv_color_hex(0xFFFF00), true);
    } else {
        lv_obj_add_flag(solar_to_battery_line, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Solar to Grid (exporting to grid)
    if (powerData.grid_power < -50 && powerData.solar_power > 0) {
        drawFlowLine(solar_to_grid_line, solar_x - comp_size/3, solar_y + comp_size/2, grid_x + comp_size/3, grid_y - comp_size/3, lv_color_hex(0xFFFF00), true);
    } else {
        lv_obj_add_flag(solar_to_grid_line, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Battery to Grid (battery discharging to grid)
    if (powerData.grid_power < -50 && powerData.battery_power > 0) {
        drawFlowLine(battery_to_grid_line, battery_x - comp_size/3, battery_y - comp_size/2, grid_x + comp_size/3, grid_y + comp_size/3, lv_color_hex(0x00FF00), true);
    } else {
        lv_obj_add_flag(battery_to_grid_line, LV_OBJ_FLAG_HIDDEN);
    }
}

void loadConfig() {
    preferences.begin("powerwall", false);
    mqtt_server = preferences.getString("mqtt_server", "");
    mqtt_port = preferences.getString("mqtt_port", "1883");
    mqtt_user = preferences.getString("mqtt_user", "");
    mqtt_password = preferences.getString("mqtt_pass", "");
    mqtt_prefix = preferences.getString("mqtt_prefix", DEFAULT_MQTT_PREFIX);
    preferences.end();
    
    Serial.println("Configuration loaded:");
    Serial.println("MQTT Server: " + mqtt_server);
    Serial.println("MQTT Port: " + mqtt_port);
    Serial.println("MQTT User: " + mqtt_user);
    Serial.println("MQTT Prefix: " + mqtt_prefix);
}

void saveConfig() {
    preferences.begin("powerwall", false);
    preferences.putString("mqtt_server", mqtt_server);
    preferences.putString("mqtt_port", mqtt_port);
    preferences.putString("mqtt_user", mqtt_user);
    preferences.putString("mqtt_pass", mqtt_password);
    preferences.putString("mqtt_prefix", mqtt_prefix);
    preferences.end();
    
    Serial.println("Configuration saved");
}

void saveConfigCallback() {
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

void buildMqttTopics() {
    topic_grid_power = mqtt_prefix + String(TOPIC_SITE_POWER);
    topic_solar_power = mqtt_prefix + String(TOPIC_SOLAR_POWER);
    topic_battery_power = mqtt_prefix + String(TOPIC_BATTERY_POWER);
    topic_battery_level = mqtt_prefix + String(TOPIC_BATTERY_LEVEL);
    topic_home_power = mqtt_prefix + String(TOPIC_LOAD_POWER);
    
    Serial.println("MQTT Topics:");
    Serial.println("  Grid: " + topic_grid_power);
    Serial.println("  Solar: " + topic_solar_power);
    Serial.println("  Battery: " + topic_battery_power);
    Serial.println("  Battery Level: " + topic_battery_level);
    Serial.println("  Home: " + topic_home_power);
}

void setupWiFiManager() {
    // Custom parameters for MQTT configuration
    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server.c_str(), 40);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port.c_str(), 6);
    WiFiManagerParameter custom_mqtt_user("user", "MQTT User", mqtt_user.c_str(), 40);
    WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password.c_str(), 40);
    WiFiManagerParameter custom_mqtt_prefix("prefix", "MQTT Topic Prefix", mqtt_prefix.c_str(), 40);
    
    // Add custom parameters
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_password);
    wifiManager.addParameter(&custom_mqtt_prefix);
    
    // Set save config callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    
    // Set timeout for configuration portal
    wifiManager.setConfigPortalTimeout(180);
    
    // Try to connect with saved credentials
    if (!wifiManager.autoConnect(AP_NAME, AP_PASSWORD)) {
        Serial.println("Failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
    }
    
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Read updated parameters
    mqtt_server = custom_mqtt_server.getValue();
    mqtt_port = custom_mqtt_port.getValue();
    mqtt_user = custom_mqtt_user.getValue();
    mqtt_password = custom_mqtt_password.getValue();
    mqtt_prefix = custom_mqtt_prefix.getValue();
    
    // Save the custom parameters if needed
    if (shouldSaveConfig) {
        saveConfig();
        buildMqttTopics();
    }
}

void setupMQTT() {
    if (mqtt_server.length() > 0) {
        int port = mqtt_port.toInt();
        mqttClient.setServer(mqtt_server.c_str(), port);
        mqttClient.setCallback(mqttCallback);
    } else {
        Serial.println("MQTT server not configured!");
    }
}

bool reconnectMQTT() {
    if (mqtt_server.length() == 0) {
        return false;
    }
    
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "ESP32-Powerwall-" + String(random(0xffff), HEX);
    
    bool connected;
    if (mqtt_user.length() > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_password.c_str());
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
        Serial.println("connected");
        
        // Subscribe to all topics
        mqttClient.subscribe(topic_grid_power.c_str());
        mqttClient.subscribe(topic_solar_power.c_str());
        mqttClient.subscribe(topic_battery_power.c_str());
        mqttClient.subscribe(topic_battery_level.c_str());
        mqttClient.subscribe(topic_home_power.c_str());
        
        Serial.println("Subscribed to MQTT topics");
        return true;
    } else {
        Serial.print("failed, rc=");
        Serial.println(mqttClient.state());
        return false;
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    String payloadStr = "";
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }
    
    float value = payloadStr.toFloat();
    
    String topicStr = String(topic);
    
    // Update power data based on topic
    if (topicStr == topic_grid_power) {
        powerData.grid_power = value;
        Serial.printf("Grid Power: %.1f W\n", value);
    } else if (topicStr == topic_solar_power) {
        powerData.solar_power = value;
        Serial.printf("Solar Power: %.1f W\n", value);
    } else if (topicStr == topic_battery_power) {
        powerData.battery_power = value;
        Serial.printf("Battery Power: %.1f W\n", value);
    } else if (topicStr == topic_battery_level) {
        powerData.battery_level = value;
        Serial.printf("Battery Level: %.1f%%\n", value);
    } else if (topicStr == topic_home_power) {
        powerData.home_power = value;
        Serial.printf("Home Power: %.1f W\n", value);
    }
}

void drawFlowLine(lv_obj_t* line, int x1, int y1, int x2, int y2, lv_color_t color, bool active) {
    lv_point_t points[2];
    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;
    
    lv_line_set_points(line, points, 2);
    
    if (active) {
        lv_obj_set_style_line_width(line, 4, 0);
        lv_obj_set_style_line_color(line, color, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    }
}
