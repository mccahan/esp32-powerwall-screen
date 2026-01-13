#include "main_screen.h"
#include "ui_assets/ui_assets.h"
#include "mqtt_client.h"
#include <WiFi.h>
#include <cmath>

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

// UI elements - Main dashboard
static lv_obj_t *main_screen = nullptr;

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
unsigned long last_data_ms = 0;
static unsigned long last_pulse_ms = 0;

// MQTT status monitoring
static unsigned long last_mqtt_status_update = 0;
static bool last_mqtt_status = false;

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

lv_obj_t* getMainScreen() {
    return main_screen;
}

void updateStatusLabel(const char* status) {
    if (lbl_status) {
        lv_label_set_text(lbl_status, status);
    }
}

// ============== Data RX Pulse Animation ==============

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

// ============== MQTT Status Monitoring ==============

void updateMQTTStatus() {
    // Check MQTT status every 5 seconds and update UI if changed
    unsigned long now = millis();
    if (now - last_mqtt_status_update < 5000) {
        return;
    }
    last_mqtt_status_update = now;
    
    bool mqtt_connected = mqttClient.isConnected();
    
    // Only update if status changed
    if (mqtt_connected != last_mqtt_status) {
        last_mqtt_status = mqtt_connected;
        
        // Update status label only if WiFi is connected
        if (WiFi.status() == WL_CONNECTED) {
            String ip = WiFi.localIP().toString();
            String status = "WiFi: " + WiFi.SSID() + " | ";
            
            if (mqtt_connected) {
                status += "MQTT: Connected";
            } else {
                status += "MQTT: Disconnected";
            }
            status += " | Config: http://" + ip;
            
            updateStatusLabel(status.c_str());
        }
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
