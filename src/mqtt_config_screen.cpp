#include "mqtt_config_screen.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>

// Theme colors
#define COLOR_BG        0x0A0C10
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRAY      0x6A6A6A
#define COLOR_ACCENT    0x137FEC

// QR code size
#define QR_SIZE 200

// MQTT config screen elements
static lv_obj_t *mqtt_config_screen = nullptr;
static lv_obj_t *qr_code = nullptr;
static lv_obj_t *url_label = nullptr;

void createMqttConfigScreen(lv_obj_t *parent_screen) {
    // MQTT config screen overlay
    mqtt_config_screen = lv_obj_create(parent_screen);
    lv_obj_set_size(mqtt_config_screen, 480, 480);
    lv_obj_set_pos(mqtt_config_screen, 0, 0);
    lv_obj_set_style_bg_color(mqtt_config_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(mqtt_config_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mqtt_config_screen, 0, 0);
    lv_obj_set_style_radius(mqtt_config_screen, 0, 0);
    lv_obj_clear_flag(mqtt_config_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mqtt_config_screen, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // Title
    lv_obj_t *title = lv_label_create(mqtt_config_screen);
    lv_label_set_text(title, "MQTT Not Configured");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    // Instruction
    lv_obj_t *instruction = lv_label_create(mqtt_config_screen);
    lv_label_set_text(instruction, "Scan to configure");
    lv_obj_set_style_text_color(instruction, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(instruction, &lv_font_montserrat_16, 0);
    lv_obj_align(instruction, LV_ALIGN_TOP_MID, 0, 75);

    // QR code (created but not populated yet - will be set when showing)
    qr_code = lv_qrcode_create(mqtt_config_screen, QR_SIZE, lv_color_hex(COLOR_WHITE), lv_color_hex(COLOR_BG));
    lv_obj_align(qr_code, LV_ALIGN_CENTER, 0, 0);
    // Add white border around QR code for better scanning
    lv_obj_set_style_border_color(qr_code, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_width(qr_code, 10, 0);

    // URL label (will be updated when showing)
    url_label = lv_label_create(mqtt_config_screen);
    lv_label_set_text(url_label, "");
    lv_obj_set_style_text_color(url_label, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_text_font(url_label, &lv_font_montserrat_20, 0);
    lv_obj_align(url_label, LV_ALIGN_BOTTOM_MID, 0, -60);

    // Hint
    lv_obj_t *hint = lv_label_create(mqtt_config_screen);
    lv_label_set_text(hint, "Or visit the URL above in your browser");
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -30);
}

void showMqttConfigScreen(const char* ip_address) {
    if (mqtt_config_screen && qr_code && url_label) {
        // Build URL
        char url[64];
        snprintf(url, sizeof(url), "http://%s/", ip_address);

        // Update QR code with URL
        lv_qrcode_update(qr_code, url, strlen(url));

        // Update URL label
        lv_label_set_text(url_label, url);

        lv_obj_clear_flag(mqtt_config_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void hideMqttConfigScreen() {
    if (mqtt_config_screen) {
        lv_obj_add_flag(mqtt_config_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

bool isMqttConfigScreenVisible() {
    if (mqtt_config_screen) {
        return !lv_obj_has_flag(mqtt_config_screen, LV_OBJ_FLAG_HIDDEN);
    }
    return false;
}
