#include "info_screen.h"
#include "main_screen.h"
#include "mqtt_client.h"
#include <WiFi.h>

// Theme colors (matching main screen)
#define COLOR_BG        0x0A0C10
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRAY      0x6A6A6A
#define COLOR_GREEN     0x22C55E
#define COLOR_RED       0xEF4444

// Display dimensions
#define TFT_WIDTH 480
#define TFT_HEIGHT 480

// Info screen elements
static lv_obj_t *info_screen = nullptr;
static lv_obj_t *lbl_wifi_status = nullptr;
static lv_obj_t *lbl_wifi_ssid = nullptr;
static lv_obj_t *lbl_ip_addr = nullptr;
static lv_obj_t *lbl_mqtt_host = nullptr;
static lv_obj_t *lbl_mqtt_status = nullptr;
static lv_obj_t *lbl_last_update = nullptr;

// Forward declaration for back button callback
static void back_btn_event_cb(lv_event_t *e);

void createInfoScreen() {
    // Create info screen (separate from main screen)
    info_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(info_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_clear_flag(info_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(info_screen);
    lv_label_set_text(title, "System Info");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(info_screen);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x2A2D32), 0);
    lv_obj_set_style_radius(btn_back, 8, 0);
    lv_obj_add_event_cb(btn_back, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn_back);
    lv_label_set_text(btn_label, "< Back");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_center(btn_label);

    // Info container
    int y_offset = 100;
    int label_spacing = 50;
    int left_margin = 40;

    // WiFi Status
    lv_obj_t *wifi_header = lv_label_create(info_screen);
    lv_label_set_text(wifi_header, "WiFi Status:");
    lv_obj_set_style_text_color(wifi_header, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(wifi_header, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(wifi_header, left_margin, y_offset);

    lbl_wifi_status = lv_label_create(info_screen);
    lv_label_set_text(lbl_wifi_status, "---");
    lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_wifi_status, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_wifi_status, 200, y_offset);

    y_offset += label_spacing;

    // WiFi SSID
    lv_obj_t *ssid_header = lv_label_create(info_screen);
    lv_label_set_text(ssid_header, "Network:");
    lv_obj_set_style_text_color(ssid_header, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(ssid_header, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(ssid_header, left_margin, y_offset);

    lbl_wifi_ssid = lv_label_create(info_screen);
    lv_label_set_text(lbl_wifi_ssid, "---");
    lv_obj_set_style_text_color(lbl_wifi_ssid, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_wifi_ssid, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_wifi_ssid, 200, y_offset);

    y_offset += label_spacing;

    // IP Address
    lv_obj_t *ip_header = lv_label_create(info_screen);
    lv_label_set_text(ip_header, "IP Address:");
    lv_obj_set_style_text_color(ip_header, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(ip_header, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(ip_header, left_margin, y_offset);

    lbl_ip_addr = lv_label_create(info_screen);
    lv_label_set_text(lbl_ip_addr, "---");
    lv_obj_set_style_text_color(lbl_ip_addr, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_ip_addr, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_ip_addr, 200, y_offset);

    y_offset += label_spacing;

    // MQTT Broker
    lv_obj_t *mqtt_header = lv_label_create(info_screen);
    lv_label_set_text(mqtt_header, "MQTT Broker:");
    lv_obj_set_style_text_color(mqtt_header, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(mqtt_header, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(mqtt_header, left_margin, y_offset);

    lbl_mqtt_host = lv_label_create(info_screen);
    lv_label_set_text(lbl_mqtt_host, "---");
    lv_obj_set_style_text_color(lbl_mqtt_host, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_mqtt_host, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_mqtt_host, 200, y_offset);

    y_offset += label_spacing;

    // MQTT Status
    lv_obj_t *mqtt_status_header = lv_label_create(info_screen);
    lv_label_set_text(mqtt_status_header, "MQTT Status:");
    lv_obj_set_style_text_color(mqtt_status_header, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(mqtt_status_header, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(mqtt_status_header, left_margin, y_offset);

    lbl_mqtt_status = lv_label_create(info_screen);
    lv_label_set_text(lbl_mqtt_status, "---");
    lv_obj_set_style_text_color(lbl_mqtt_status, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_mqtt_status, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_mqtt_status, 200, y_offset);

    y_offset += label_spacing;

    // Last Update
    lv_obj_t *update_header = lv_label_create(info_screen);
    lv_label_set_text(update_header, "Last Update:");
    lv_obj_set_style_text_color(update_header, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(update_header, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(update_header, left_margin, y_offset);

    lbl_last_update = lv_label_create(info_screen);
    lv_label_set_text(lbl_last_update, "---");
    lv_obj_set_style_text_color(lbl_last_update, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_last_update, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_last_update, 200, y_offset);
}

static void back_btn_event_cb(lv_event_t *e) {
    hideInfoScreen();
}

void showInfoScreen() {
    if (info_screen) {
        updateInfoScreenData();
        lv_scr_load(info_screen);
    }
}

void hideInfoScreen() {
    lv_obj_t *main_screen = getMainScreen();
    if (main_screen) {
        lv_scr_load(main_screen);
    }
}

bool isInfoScreenVisible() {
    return lv_scr_act() == info_screen;
}

void updateInfoScreenData() {
    if (!info_screen) return;

    // WiFi Status
    if (WiFi.status() == WL_CONNECTED) {
        lv_label_set_text(lbl_wifi_status, "Connected");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(COLOR_GREEN), 0);

        // SSID
        lv_label_set_text(lbl_wifi_ssid, WiFi.SSID().c_str());

        // IP Address
        lv_label_set_text(lbl_ip_addr, WiFi.localIP().toString().c_str());
    } else {
        lv_label_set_text(lbl_wifi_status, "Disconnected");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(COLOR_RED), 0);
        lv_label_set_text(lbl_wifi_ssid, "---");
        lv_label_set_text(lbl_ip_addr, "---");
    }

    // MQTT Broker host:port
    MQTTConfig& config = mqttClient.getConfig();
    if (config.host.length() > 0) {
        char mqtt_addr[64];
        snprintf(mqtt_addr, sizeof(mqtt_addr), "%s:%d", config.host.c_str(), config.port);
        lv_label_set_text(lbl_mqtt_host, mqtt_addr);
    } else {
        lv_label_set_text(lbl_mqtt_host, "Not configured");
    }

    // MQTT Status
    if (mqttClient.isConnected()) {
        lv_label_set_text(lbl_mqtt_status, "Connected");
        lv_obj_set_style_text_color(lbl_mqtt_status, lv_color_hex(COLOR_GREEN), 0);
    } else {
        lv_label_set_text(lbl_mqtt_status, "Disconnected");
        lv_obj_set_style_text_color(lbl_mqtt_status, lv_color_hex(COLOR_RED), 0);
    }

    // Last update time
    if (last_data_ms > 0) {
        unsigned long elapsed = (millis() - last_data_ms) / 1000;
        char time_str[32];
        if (elapsed < 60) {
            snprintf(time_str, sizeof(time_str), "%lu sec ago", elapsed);
        } else if (elapsed < 3600) {
            snprintf(time_str, sizeof(time_str), "%lu min ago", elapsed / 60);
        } else {
            snprintf(time_str, sizeof(time_str), "%lu hr ago", elapsed / 3600);
        }
        lv_label_set_text(lbl_last_update, time_str);
    } else {
        lv_label_set_text(lbl_last_update, "No data yet");
    }
}
