#include "config_screen.h"
#include "info_screen.h"
#include "improv_wifi.h"
#include <WiFi.h>

// Theme colors (matching main screen)
#define COLOR_BG        0x0A0C10
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRAY      0x6A6A6A
#define COLOR_CYAN      0x4FC3F7
#define COLOR_RED       0xEF4444
#define COLOR_ORANGE    0xF59E0B

// Display dimensions
#define TFT_WIDTH 480
#define TFT_HEIGHT 480

// QR code size
#define QR_SIZE 200

// Config screen elements
static lv_obj_t *config_screen = nullptr;
static lv_obj_t *qr_code = nullptr;
static lv_obj_t *lbl_url = nullptr;
static lv_obj_t *lbl_no_wifi = nullptr;

// Forward declarations for button callbacks
static void back_btn_event_cb(lv_event_t *e);
static void restart_btn_event_cb(lv_event_t *e);
static void clear_wifi_btn_event_cb(lv_event_t *e);

void createConfigScreen() {
    // Create config screen
    config_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(config_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_clear_flag(config_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(config_screen);
    lv_label_set_text(title, "Web Configuration");
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    // Back button
    lv_obj_t *btn_back = lv_btn_create(config_screen);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x2A2D32), 0);
    lv_obj_set_style_radius(btn_back, 8, 0);
    lv_obj_add_event_cb(btn_back, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn_back);
    lv_label_set_text(btn_label, "< Back");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_center(btn_label);

    // Instructions
    lv_obj_t *instructions = lv_label_create(config_screen);
    lv_label_set_text(instructions, "Scan to open settings in browser");
    lv_obj_set_style_text_color(instructions, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(instructions, &lv_font_montserrat_16, 0);
    lv_obj_align(instructions, LV_ALIGN_TOP_MID, 0, 80);

    // QR code (created but hidden initially)
    qr_code = lv_qrcode_create(config_screen, QR_SIZE, lv_color_hex(COLOR_BG), lv_color_hex(COLOR_WHITE));
    lv_obj_align(qr_code, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(qr_code, LV_OBJ_FLAG_HIDDEN);

    // URL label below QR code
    lbl_url = lv_label_create(config_screen);
    lv_label_set_text(lbl_url, "");
    lv_obj_set_style_text_color(lbl_url, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_text_font(lbl_url, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_url, LV_ALIGN_CENTER, 0, QR_SIZE / 2 + 30);
    lv_obj_add_flag(lbl_url, LV_OBJ_FLAG_HIDDEN);

    // No WiFi message (shown when not connected)
    lbl_no_wifi = lv_label_create(config_screen);
    lv_label_set_text(lbl_no_wifi, "WiFi not connected\n\nConnect to WiFi first\nto access web configuration");
    lv_obj_set_style_text_color(lbl_no_wifi, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(lbl_no_wifi, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(lbl_no_wifi, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_no_wifi, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(lbl_no_wifi, LV_OBJ_FLAG_HIDDEN);

    // Clear WiFi button at bottom left
    lv_obj_t *btn_clear_wifi = lv_btn_create(config_screen);
    lv_obj_set_size(btn_clear_wifi, 140, 45);
    lv_obj_align(btn_clear_wifi, LV_ALIGN_BOTTOM_MID, -80, -30);
    lv_obj_set_style_bg_color(btn_clear_wifi, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_radius(btn_clear_wifi, 8, 0);
    lv_obj_add_event_cb(btn_clear_wifi, clear_wifi_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *clear_wifi_label = lv_label_create(btn_clear_wifi);
    lv_label_set_text(clear_wifi_label, "Clear WiFi");
    lv_obj_set_style_text_color(clear_wifi_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(clear_wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_center(clear_wifi_label);

    // Restart button at bottom right
    lv_obj_t *btn_restart = lv_btn_create(config_screen);
    lv_obj_set_size(btn_restart, 120, 45);
    lv_obj_align(btn_restart, LV_ALIGN_BOTTOM_MID, 80, -30);
    lv_obj_set_style_bg_color(btn_restart, lv_color_hex(COLOR_RED), 0);
    lv_obj_set_style_radius(btn_restart, 8, 0);
    lv_obj_add_event_cb(btn_restart, restart_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *restart_label = lv_label_create(btn_restart);
    lv_label_set_text(restart_label, "Restart");
    lv_obj_set_style_text_color(restart_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_16, 0);
    lv_obj_center(restart_label);
}

static void back_btn_event_cb(lv_event_t *e) {
    hideConfigScreen();
}

static void restart_btn_event_cb(lv_event_t *e) {
    ESP.restart();
}

static void clear_wifi_btn_event_cb(lv_event_t *e) {
    // Clear saved WiFi credentials
    wifi_preferences.begin("wifi", false);
    wifi_preferences.clear();
    wifi_preferences.end();

    // Restart device to enter setup mode
    ESP.restart();
}

void showConfigScreen() {
    if (config_screen) {
        updateConfigScreenQR();
        lv_scr_load(config_screen);
    }
}

void hideConfigScreen() {
    // Go back to info screen
    showInfoScreen();
}

bool isConfigScreenVisible() {
    return lv_scr_act() == config_screen;
}

void updateConfigScreenQR() {
    if (!config_screen) return;

    if (WiFi.status() == WL_CONNECTED) {
        // Build URL
        String ip = WiFi.localIP().toString();
        String url = "http://" + ip + "/config";

        // Update QR code
        lv_qrcode_update(qr_code, url.c_str(), url.length());
        lv_obj_clear_flag(qr_code, LV_OBJ_FLAG_HIDDEN);

        // Update URL label
        lv_label_set_text(lbl_url, url.c_str());
        lv_obj_clear_flag(lbl_url, LV_OBJ_FLAG_HIDDEN);

        // Hide no WiFi message
        lv_obj_add_flag(lbl_no_wifi, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Hide QR code and URL
        lv_obj_add_flag(qr_code, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_url, LV_OBJ_FLAG_HIDDEN);

        // Show no WiFi message
        lv_obj_clear_flag(lbl_no_wifi, LV_OBJ_FLAG_HIDDEN);
    }
}
