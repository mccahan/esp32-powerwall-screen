#include "wifi_error_screen.h"
#include "ui_assets/ui_assets.h"
#include "improv_wifi.h"

// Theme colors
#define COLOR_BG        0x0A0C10
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRAY      0x6A6A6A
#define COLOR_BLUE      0x4FC3F7

// WiFi error screen elements
static lv_obj_t *wifi_error_screen = nullptr;
static lv_obj_t *wifi_error_label = nullptr;
static lv_obj_t *wifi_countdown_label = nullptr;

// Event handler for manual retry
static void retry_button_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        Serial.println("Manual WiFi retry triggered by user");
        retryWiFiConnection();
    }
}

void createWifiErrorScreen(lv_obj_t *parent_screen) {
    // WiFi error screen overlay
    wifi_error_screen = lv_obj_create(parent_screen);
    lv_obj_set_size(wifi_error_screen, 480, 480);
    lv_obj_set_pos(wifi_error_screen, 0, 0);
    lv_obj_set_style_bg_color(wifi_error_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(wifi_error_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifi_error_screen, 0, 0);
    lv_obj_set_style_radius(wifi_error_screen, 0, 0);
    lv_obj_clear_flag(wifi_error_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(wifi_error_screen, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // No WiFi icon
    lv_obj_t *wifi_icon = lv_img_create(wifi_error_screen);
    lv_img_set_src(wifi_icon, &icon_no_wifi_img);
    lv_obj_align(wifi_icon, LV_ALIGN_CENTER, 0, -80);

    // Error message label
    wifi_error_label = lv_label_create(wifi_error_screen);
    lv_label_set_text(wifi_error_label, "WiFi not configured");
    lv_obj_set_style_text_color(wifi_error_label, lv_color_hex(COLOR_GRAY), 0);
    lv_obj_set_style_text_font(wifi_error_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(wifi_error_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_error_label, LV_ALIGN_CENTER, 0, 20);

    // Countdown/retry label (clickable)
    wifi_countdown_label = lv_label_create(wifi_error_screen);
    lv_label_set_text(wifi_countdown_label, "Tap to retry");
    lv_obj_set_style_text_color(wifi_countdown_label, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_style_text_font(wifi_countdown_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(wifi_countdown_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_countdown_label, LV_ALIGN_CENTER, 0, 80);
    
    // Make countdown label clickable
    lv_obj_add_flag(wifi_countdown_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(wifi_countdown_label, retry_button_event_cb, LV_EVENT_CLICKED, NULL);
}

void showWifiErrorScreen(const char* message) {
    if (wifi_error_screen) {
        if (wifi_error_label && message) {
            lv_label_set_text(wifi_error_label, message);
        }
        lv_obj_clear_flag(wifi_error_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void hideWifiErrorScreen() {
    if (wifi_error_screen) {
        lv_obj_add_flag(wifi_error_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

bool isWifiErrorScreenVisible() {
    if (wifi_error_screen) {
        return !lv_obj_has_flag(wifi_error_screen, LV_OBJ_FLAG_HIDDEN);
    }
    return false;
}

void updateWifiErrorCountdown(unsigned long next_retry_time) {
    if (!wifi_countdown_label || !isWifiErrorScreenVisible()) {
        return;
    }
    
    // If actively connecting, show connecting message instead of countdown
    if (wifi_connecting) {
        lv_label_set_text(wifi_countdown_label, "Connecting... (tap to retry)");
        return;
    }
    
    unsigned long now = millis();
    // Calculate time until next retry
    if (next_retry_time > now) {
        unsigned long seconds_remaining = (next_retry_time - now) / 1000;
        char buf[64];
        snprintf(buf, sizeof(buf), "Retrying in %us (tap to retry now)", (unsigned int)seconds_remaining);
        lv_label_set_text(wifi_countdown_label, buf);
    } else {
        // Time has passed, should retry soon
        lv_label_set_text(wifi_countdown_label, "Retrying... (tap to retry)");
    }
}
