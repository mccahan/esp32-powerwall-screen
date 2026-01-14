#include "main_screen.h"
#include "info_screen.h"
#include "mqtt_config_screen.h"
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
#define SOC_OFFGRID_X 84
#define SOC_OFFGRID_WIDTH 94
#define TIME_REMAINING_X 150
#define TIME_REMAINING_WIDTH 250
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
#define COLOR_GRAY      0x6A6A6A  // Used for dimmed text in recolor mode

// Animation timing constants
#define ANIMATION_FRAME_MS  33  // ~30 FPS (matches ESPHome 33ms update_interval)

// UI elements - Main dashboard
static lv_obj_t *main_screen = nullptr;

// Power value labels
static lv_obj_t *lbl_solar_val = nullptr;
static lv_obj_t *lbl_grid_val = nullptr;
static lv_obj_t *lbl_home_val = nullptr;
static lv_obj_t *lbl_batt_val = nullptr;

// SOC elements
static lv_obj_t *lbl_soc = nullptr;
static lv_obj_t *lbl_soc_offgrid = nullptr;
static lv_obj_t *lbl_time_remaining = nullptr;
static lv_obj_t *bar_soc = nullptr;

// Disable overlays for icons
static lv_obj_t *img_solar_disabled = nullptr;
static lv_obj_t *img_grid_disabled = nullptr;
static lv_obj_t *img_battery_disabled = nullptr;

// Icon images
static lv_obj_t *img_solar = nullptr;
static lv_obj_t *img_grid = nullptr;
static lv_obj_t *img_grid_offline = nullptr;
static lv_obj_t *img_home = nullptr;
static lv_obj_t *img_battery = nullptr;
static lv_obj_t *img_center = nullptr;


// Data RX indicator dot
static lv_obj_t *dot_data_rx = nullptr;

// Info button
static lv_obj_t *btn_info = nullptr;

// Animated dots for power flow visualization (3 dots per flow path)
static lv_obj_t *dot_solar_home = nullptr;
static lv_obj_t *dot_solar_home_2 = nullptr;
static lv_obj_t *dot_solar_home_3 = nullptr;
static lv_obj_t *dot_solar_batt = nullptr;
static lv_obj_t *dot_solar_batt_2 = nullptr;
static lv_obj_t *dot_solar_batt_3 = nullptr;
static lv_obj_t *dot_solar_grid = nullptr;
static lv_obj_t *dot_solar_grid_2 = nullptr;
static lv_obj_t *dot_solar_grid_3 = nullptr;
static lv_obj_t *dot_grid_home = nullptr;
static lv_obj_t *dot_grid_home_2 = nullptr;
static lv_obj_t *dot_grid_home_3 = nullptr;
static lv_obj_t *dot_grid_batt = nullptr;
static lv_obj_t *dot_grid_batt_2 = nullptr;
static lv_obj_t *dot_grid_batt_3 = nullptr;
static lv_obj_t *dot_batt_home = nullptr;
static lv_obj_t *dot_batt_home_2 = nullptr;
static lv_obj_t *dot_batt_home_3 = nullptr;
static lv_obj_t *dot_batt_grid = nullptr;
static lv_obj_t *dot_batt_grid_2 = nullptr;
static lv_obj_t *dot_batt_grid_3 = nullptr;

// Forward declaration for info button callback
static void info_btn_event_cb(lv_event_t *e);

// Timing variables for pulse animation
unsigned long last_data_ms = 0;
static unsigned long last_pulse_ms = 0;
static unsigned long last_pulse_update_ms = 0;  // Throttle pulse updates

// Track if we've received data (to hide config screen)
static bool mqtt_data_received = false;

// Power flow animation state
static float g_grid_w = 0.0f;
static float g_home_w = 0.0f;
static float g_solar_w = 0.0f;
static float g_batt_w = 0.0f;
static float g_soc = 0.0f;
static float ph_master = 0.0f;
static unsigned long g_last_anim_ms = 0;
static bool g_offgrid = false;
static float g_time_remaining = 0.0f;

void createMainDashboard() {
    // Main screen with dark background
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(main_screen);

    // ========== Animated Power Flow Dots (created first so they appear under layout) ==========
    // Helper lambda to create a dot
    auto create_dot = [&](uint32_t color) -> lv_obj_t* {
        lv_obj_t *dot = lv_obj_create(main_screen);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_style_radius(dot, 6, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_FLOATING);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        return dot;
    };

    // Solar flows (yellow dots)
    dot_solar_home = create_dot(COLOR_SOLAR);
    dot_solar_home_2 = create_dot(COLOR_SOLAR);
    dot_solar_home_3 = create_dot(COLOR_SOLAR);
    dot_solar_batt = create_dot(COLOR_SOLAR);
    dot_solar_batt_2 = create_dot(COLOR_SOLAR);
    dot_solar_batt_3 = create_dot(COLOR_SOLAR);
    dot_solar_grid = create_dot(COLOR_SOLAR);
    dot_solar_grid_2 = create_dot(COLOR_SOLAR);
    dot_solar_grid_3 = create_dot(COLOR_SOLAR);

    // Grid flows (gray dots)
    dot_grid_home = create_dot(COLOR_GRID);
    dot_grid_home_2 = create_dot(COLOR_GRID);
    dot_grid_home_3 = create_dot(COLOR_GRID);
    dot_grid_batt = create_dot(COLOR_GRID);
    dot_grid_batt_2 = create_dot(COLOR_GRID);
    dot_grid_batt_3 = create_dot(COLOR_GRID);

    // Battery flows (green dots)
    dot_batt_home = create_dot(COLOR_BATTERY);
    dot_batt_home_2 = create_dot(COLOR_BATTERY);
    dot_batt_home_3 = create_dot(COLOR_BATTERY);
    dot_batt_grid = create_dot(COLOR_BATTERY);
    dot_batt_grid_2 = create_dot(COLOR_BATTERY);
    dot_batt_grid_3 = create_dot(COLOR_BATTERY);

    // ========== Icon Images (created after dots so dots appear underneath) ==========
    img_solar = lv_img_create(main_screen);
    lv_img_set_src(img_solar, &icon_solar_img);
    lv_obj_set_pos(img_solar, 205, 31);

    img_solar_disabled = lv_img_create(main_screen);
    lv_img_set_src(img_solar_disabled, &icon_solar_disabled_img);
    lv_obj_set_pos(img_solar_disabled, 205, 31);
    lv_obj_add_flag(img_solar_disabled, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    img_grid = lv_img_create(main_screen);
    lv_img_set_src(img_grid, &icon_grid_img);
    lv_obj_set_pos(img_grid, 77, 159);

    img_grid_disabled = lv_img_create(main_screen);
    lv_img_set_src(img_grid_disabled, &icon_grid_disabled_img);
    lv_obj_set_pos(img_grid_disabled, 77, 159);
    lv_obj_add_flag(img_grid_disabled, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    img_home = lv_img_create(main_screen);
    lv_img_set_src(img_home, &icon_home_img);
    lv_obj_set_pos(img_home, 333, 159);

    img_battery = lv_img_create(main_screen);
    lv_img_set_src(img_battery, &icon_battery_img);
    lv_obj_set_pos(img_battery, 206, 265);

    img_battery_disabled = lv_img_create(main_screen);
    lv_img_set_src(img_battery_disabled, &icon_battery_disabled_img);
    lv_obj_set_pos(img_battery_disabled, 206, 265);
    lv_obj_add_flag(img_battery_disabled, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    img_center = lv_img_create(main_screen);
    lv_img_set_src(img_center, &icon_center_img);
    lv_obj_set_pos(img_center, 209, 163);

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

    // ========== Off-Grid SOC Label (left-aligned, hidden by default) ==========
    lbl_soc_offgrid = lv_label_create(main_screen);
    lv_label_set_text(lbl_soc_offgrid, "0%");
    lv_obj_set_style_text_color(lbl_soc_offgrid, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_soc_offgrid, &space_bold_30, 0);
    lv_obj_set_style_text_align(lbl_soc_offgrid, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(lbl_soc_offgrid, SOC_OFFGRID_X, SOC_LABEL_Y);
    lv_obj_set_width(lbl_soc_offgrid, SOC_OFFGRID_WIDTH);
    lv_obj_set_height(lbl_soc_offgrid, LABEL_HEIGHT_LARGE);
    lv_label_set_recolor(lbl_soc_offgrid, true);  // Enable recolor for multi-color text
    lv_obj_add_flag(lbl_soc_offgrid, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // ========== Time Remaining Label (right-aligned, hidden by default) ==========
    lbl_time_remaining = lv_label_create(main_screen);
    lv_label_set_text(lbl_time_remaining, "");
    lv_obj_set_style_text_color(lbl_time_remaining, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(lbl_time_remaining, &space_bold_30, 0);
    lv_obj_set_style_text_align(lbl_time_remaining, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_time_remaining, TIME_REMAINING_X, SOC_LABEL_Y);
    lv_obj_set_width(lbl_time_remaining, TIME_REMAINING_WIDTH);
    lv_obj_set_height(lbl_time_remaining, LABEL_HEIGHT_LARGE);
    lv_label_set_recolor(lbl_time_remaining, true);  // Enable recolor for multi-color text
    lv_obj_add_flag(lbl_time_remaining, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

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
    lv_img_set_src(img_grid_offline, &icon_grid_offline_img);
    lv_obj_set_pos(img_grid_offline, 77, 159);
    lv_obj_add_flag(img_grid_offline, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // ========== Data RX Indicator Dot ==========
    dot_data_rx = lv_obj_create(main_screen);
    lv_obj_set_size(dot_data_rx, 10, 10);
    lv_obj_set_pos(dot_data_rx, 10, 10);
    lv_obj_set_style_radius(dot_data_rx, 5, 0);
    lv_obj_set_style_bg_color(dot_data_rx, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(dot_data_rx, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dot_data_rx, 0, 0);
    lv_obj_add_flag(dot_data_rx, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(dot_data_rx, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(dot_data_rx, LV_OBJ_FLAG_SCROLLABLE);

    // ========== Info Button (top right corner) ==========
    btn_info = lv_imgbtn_create(main_screen);
    lv_imgbtn_set_src(btn_info, LV_IMGBTN_STATE_RELEASED, NULL, &info_icon_img, NULL);
    lv_obj_set_size(btn_info, 55, 55);
    lv_obj_set_pos(btn_info, TFT_WIDTH - 65, 10);
    lv_obj_add_event_cb(btn_info, info_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn_info, LV_OBJ_FLAG_FLOATING);
}

static void info_btn_event_cb(lv_event_t *e) {
    showInfoScreen();
}

lv_obj_t* getMainScreen() {
    return main_screen;
}

// ============== Data RX Pulse Animation ==============

void updateDataRxPulse() {
    if (!dot_data_rx) return;
    
    const unsigned long now = millis();
    
    // Throttle updates to ~30 FPS
    if (now - last_pulse_update_ms < ANIMATION_FRAME_MS) {
        return;
    }
    last_pulse_update_ms = now;
    
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

// Helper to hide config screen on first data
static void onDataReceived() {
    if (!mqtt_data_received) {
        mqtt_data_received = true;
        hideMqttConfigScreen();
    }
    last_data_ms = millis();
}

void updateSolarValue(float watts) {
    g_solar_w = watts;  // Store for animation
    if (lbl_solar_val) {
        char buf[24];
        float kw = watts / 1000.0f;

        // Round to 0.0 if the value is between -100 and 100
        if (watts > -100 && watts < 100) {
            kw = 0.0f;
            lv_obj_clear_flag(img_solar_disabled, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(lbl_solar_val, LV_OPA_80, 0);
        } else {
            lv_obj_add_flag(img_solar_disabled, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(lbl_solar_val, LV_OPA_COVER, 0);
        }

        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_solar_val, buf);
    }
    onDataReceived();
}

void updateGridValue(float watts) {
    g_grid_w = watts;  // Store for animation
    if (lbl_grid_val) {
        char buf[24];
        float kw = watts / 1000.0f;

        // Round to 0.0 if the value is between -100 and 100
        if (watts > -100 && watts < 100) {
            kw = 0.0f;
            lv_obj_clear_flag(img_grid_disabled, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(lbl_grid_val, LV_OPA_80, 0);
        } else {
            lv_obj_add_flag(img_grid_disabled, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(lbl_grid_val, LV_OPA_COVER, 0);
        }

        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_grid_val, buf);
    }
    onDataReceived();
}

void updateHomeValue(float watts) {
    g_home_w = watts;  // Store for animation
    if (lbl_home_val) {
        char buf[24];
        float kw = watts / 1000.0f;

        // Round to 0.0 if the value is between -100 and 100
        if (watts > -100 && watts < 100) {
            kw = 0.0f;
        }

        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_home_val, buf);
    }
    onDataReceived();
}

void updateBatteryValue(float watts) {
    g_batt_w = watts;  // Store for animation
    if (lbl_batt_val) {
        char buf[24];
        float kw = watts / 1000.0f;

        // Round to 0.0 if the value is between -100 and 100
        if (watts > -100 && watts < 100) {
            kw = 0.0f;
            lv_obj_clear_flag(img_battery_disabled, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(lbl_batt_val, LV_OPA_80, 0);
        } else {
            lv_obj_add_flag(img_battery_disabled, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_opa(lbl_batt_val, LV_OPA_COVER, 0);
        }

        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_batt_val, buf);
    }
    onDataReceived();
}

void updateSOC(float soc_percent) {
    g_soc = soc_percent;  // Store for animation
    // Adjust for 5% reserve like ESPHome config
    float adjusted = (soc_percent / 0.95f) - (5.0f / 0.95f);
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 100) adjusted = 100;

    if (lbl_soc) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)roundf(adjusted));
        lv_label_set_text(lbl_soc, buf);
    }

    // Update off-grid label with same value but with gray % symbol
    if (lbl_soc_offgrid) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d#%06X %%#", (int)roundf(adjusted), COLOR_GRAY);
        lv_label_set_text(lbl_soc_offgrid, buf);
    }

    if (bar_soc) {
        lv_bar_set_value(bar_soc, (int)roundf(adjusted), LV_ANIM_OFF);
    }

    onDataReceived();
}

void updateOffGridStatus(int offgrid) {
    g_offgrid = (offgrid == 1);
    
    if (g_offgrid) {
        // Show off-grid UI elements
        if (img_grid_offline) {
            lv_obj_clear_flag(img_grid_offline, LV_OBJ_FLAG_HIDDEN);
        }
        if (lbl_soc_offgrid) {
            lv_obj_clear_flag(lbl_soc_offgrid, LV_OBJ_FLAG_HIDDEN);
        }
        // Show time remaining only if we have a valid value
        if (lbl_time_remaining && g_time_remaining > 0) {
            lv_obj_clear_flag(lbl_time_remaining, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Hide normal grid label and SOC label
        if (lbl_soc) {
            lv_obj_add_flag(lbl_soc, LV_OBJ_FLAG_HIDDEN);
        }
        if (lbl_grid_val) {
            lv_obj_add_flag(lbl_grid_val, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        // Hide off-grid UI elements
        if (img_grid_offline) {
            lv_obj_add_flag(img_grid_offline, LV_OBJ_FLAG_HIDDEN);
        }
        if (lbl_soc_offgrid) {
            lv_obj_add_flag(lbl_soc_offgrid, LV_OBJ_FLAG_HIDDEN);
        }
        if (lbl_time_remaining) {
            lv_obj_add_flag(lbl_time_remaining, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Show normal grid label and SOC label
        if (lbl_soc) {
            lv_obj_clear_flag(lbl_soc, LV_OBJ_FLAG_HIDDEN);
        }
        if (lbl_grid_val) {
            lv_obj_clear_flag(lbl_grid_val, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    Serial.printf("Off-grid status: %d\n", offgrid);
    onDataReceived();
}

void updateTimeRemaining(float hours) {
    g_time_remaining = hours;
    
    if (lbl_time_remaining) {
        if (hours > 0) {
            char buf[64];
            // Format: "12.5 #6A6A6A hours#" (hours in gray)
            snprintf(buf, sizeof(buf), "%.1f #%06X hours#", hours, COLOR_GRAY);
            lv_label_set_text(lbl_time_remaining, buf);
            
            // Only show if we're off-grid
            if (g_offgrid) {
                lv_obj_clear_flag(lbl_time_remaining, LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            // Hide if no time remaining data
            lv_obj_add_flag(lbl_time_remaining, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    Serial.printf("Time remaining: %.1f hours\n", hours);
    onDataReceived();
}

// ============== Power Flow Dot Animation ==============

// Helper functions for animation (defined once, outside the main animation function)
namespace {
    inline float clampf(float v, float a, float b) {
        return (v < a) ? a : ((v > b) ? b : v);
    }
    
    inline int lerp_i(int a, int b, float t) {
        return (int)lroundf((float)a + ((float)b - (float)a) * t);
    }
}

void updatePowerFlowAnimation() {
    // Geometry - positions of each power element
    const int SX = 240, SY = 80;   // Solar
    const int HX = 360, HY = 194;  // Home
    const int BX = 240, BY = 280;  // Battery
    const int GX = 120, GY = 194;  // Grid
    const int CX = 240, CY = 194;  // Center

    const float THRESH_W = 50.0f;
    const float FADE = 0.12f;
    const int DOT_R = 6;
    
    // Animation speed parameters
    const float SPEED_DIVISOR = 2500.0f;    // Normalize power to speed
    const float MIN_SPEED = 0.18f;          // Minimum animation speed
    const float MAX_SPEED = 0.25f;          // Maximum animation speed
    
    // Opacity mapping parameters
    const float OPACITY_SCALE = 200.0f;     // Scale factor for opacity calculation
    const float OPACITY_FLOOR = 10.0f;      // Minimum opacity value
    
    // Battery state of charge threshold
    const float BATTERY_FULL_THRESHOLD = 99.5f;  // Consider battery full at this SOC
    
    // Inline helper for setting dot position
    auto set_dot_pos = [DOT_R](lv_obj_t* dot, int x, int y) {
        lv_obj_set_pos(dot, x - DOT_R, y - DOT_R);
    };
    
    // Inline helper for setting dot opacity
    auto set_dot_opa = [OPACITY_SCALE, OPACITY_FLOOR](lv_obj_t* dot, float alpha) {
        alpha = clampf(alpha, 0.0f, 1.0f);
        float opa_float = (alpha * OPACITY_SCALE) + OPACITY_FLOOR;
        if (opa_float > (float)LV_OPA_MAX) opa_float = (float)LV_OPA_MAX;
        lv_obj_set_style_bg_opa(dot, (lv_opa_t)lroundf(opa_float), 0);
    };

    
    // Place dot on two-segment path and animate
    auto animate_dot = [&](lv_obj_t* dot, float t, float watts,
                           int x_src, int y_src, int x_sink, int y_sink) {
        if (watts < THRESH_W) {
            lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
        
        // Clamp t to [0, 1]
        t = clampf(t, 0.0f, 1.0f);
        
        // Calculate position on two-segment path
        int x, y;
        if (t < 0.5f) {
            // First segment: source → center
            float seg_t = t * 2.0f;
            x = lerp_i(x_src, CX, seg_t);
            y = lerp_i(y_src, CY, seg_t);
        } else {
            // Second segment: center → sink
            float seg_t = (t - 0.5f) * 2.0f;
            x = lerp_i(CX, x_sink, seg_t);
            y = lerp_i(CY, y_sink, seg_t);
        }
        
        set_dot_pos(dot, x, y);
        
        // Calculate fade alpha
        float alpha = 1.0f;
        if (t < FADE) {
            alpha = t / FADE;
        } else if (t > (1.0f - FADE)) {
            alpha = (1.0f - t) / FADE;
        }
        set_dot_opa(dot, alpha);
    };

    // Read instantaneous powers
    const float grid_w = g_grid_w;
    const float home_w = g_home_w;
    const float solar_w = g_solar_w;
    const float batt_w = g_batt_w;
    const float soc = g_soc;

    float solar_src = solar_w > 0 ? solar_w : 0;
    float grid_src = grid_w > 0 ? grid_w : 0;
    float batt_src = batt_w > 0 ? batt_w : 0;

    float home_sink = home_w > 0 ? home_w : 0;
    float batt_sink = batt_w < 0 ? -batt_w : 0;
    float grid_sink = grid_w < 0 ? -grid_w : 0;

    // Flow allocation (visual only)
    float f_s2h = 0, f_s2b = 0, f_s2g = 0;
    float f_g2h = 0, f_g2b = 0;
    float f_b2h = 0, f_b2g = 0;

    float rem_solar = solar_src;
    float rem_home = home_sink;
    float rem_batt = batt_sink;
    float rem_grid = grid_sink;

    // Priority: solar to battery first (if not full)
    if (rem_solar > 0 && rem_batt > 0 && soc < BATTERY_FULL_THRESHOLD) {
        f_s2b = fminf(rem_solar, rem_batt);
        rem_solar -= f_s2b;
        rem_batt -= f_s2b;
    }

    // Solar to home
    if (rem_solar > 0 && rem_home > 0) {
        f_s2h = fminf(rem_solar, rem_home);
        rem_solar -= f_s2h;
        rem_home -= f_s2h;
    }

    // Solar to grid (export)
    if (rem_solar > 0 && rem_grid > 0) {
        f_s2g = fminf(rem_solar, rem_grid);
        rem_solar -= f_s2g;
        rem_grid -= f_s2g;
    }

    // Grid to battery
    float rem_grid_src = grid_src;
    if (rem_grid_src > 0 && rem_batt > 0) {
        f_g2b = fminf(rem_grid_src, rem_batt);
        rem_grid_src -= f_g2b;
        rem_batt -= f_g2b;
    }

    // Grid to home
    if (rem_grid_src > 0 && rem_home > 0) {
        f_g2h = fminf(rem_grid_src, rem_home);
        rem_grid_src -= f_g2h;
        rem_home -= f_g2h;
    }

    // Battery to home
    float rem_batt_src = batt_src;
    if (rem_batt_src > 0 && rem_home > 0) {
        f_b2h = fminf(rem_batt_src, rem_home);
        rem_batt_src -= f_b2h;
        rem_home -= f_b2h;
    }

    // Battery to grid (export)
    if (rem_batt_src > 0 && rem_grid > 0) {
        f_b2g = fminf(rem_batt_src, rem_grid);
        rem_batt_src -= f_b2g;
        rem_grid -= f_b2g;
    }

    // Find max active flow
    float max_active = 0.0f;
    auto consider = [&](float w) {
        if (w >= THRESH_W && w > max_active) max_active = w;
    };

    consider(f_s2h); consider(f_s2b); consider(f_s2g);
    consider(f_g2h); consider(f_g2b);
    consider(f_b2h); consider(f_b2g);

    // If no active flows, hide all dots
    if (max_active < THRESH_W) {
        lv_obj_t* all_dots[] = {
            dot_solar_home, dot_solar_home_2, dot_solar_home_3,
            dot_solar_batt, dot_solar_batt_2, dot_solar_batt_3,
            dot_solar_grid, dot_solar_grid_2, dot_solar_grid_3,
            dot_grid_home, dot_grid_home_2, dot_grid_home_3,
            dot_grid_batt, dot_grid_batt_2, dot_grid_batt_3,
            dot_batt_home, dot_batt_home_2, dot_batt_home_3,
            dot_batt_grid, dot_batt_grid_2, dot_batt_grid_3
        };
        for (auto dot : all_dots) {
            lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        }
        g_last_anim_ms = 0;  // Reset animation time
        return;
    }

    // Calculate elapsed time and throttle updates to ~30 FPS
    const unsigned long now = millis();
    const unsigned long last_anim = g_last_anim_ms;
    unsigned long elapsed_ms;
    
    if (last_anim == 0 || now < last_anim) {
        elapsed_ms = ANIMATION_FRAME_MS;
    } else {
        elapsed_ms = now - last_anim;
    }
    
    // Throttle to ~30 FPS
    if (elapsed_ms < ANIMATION_FRAME_MS && last_anim != 0) {
        return;  // Skip this update, not enough time has passed
    }
    
    const float dt_seconds = (float)elapsed_ms / 1000.0f;
    g_last_anim_ms = now;

    // Advance master phase
    float speed = clampf(max_active / SPEED_DIVISOR, MIN_SPEED, MAX_SPEED);
    ph_master += speed * dt_seconds;
    if (ph_master >= 1.0f) ph_master -= floorf(ph_master);

    // Calculate phases for 3 dots evenly spaced (no easing needed for linear)
    const float phase_1 = ph_master;
    const float phase_2 = fmodf(ph_master + 0.33333f, 1.0f);
    const float phase_3 = fmodf(ph_master + 0.66667f, 1.0f);

    // Animate all flows
    // Solar flows (yellow dots)
    animate_dot(dot_solar_home, phase_1, f_s2h, SX, SY, HX, HY);
    animate_dot(dot_solar_home_2, phase_2, f_s2h, SX, SY, HX, HY);
    animate_dot(dot_solar_home_3, phase_3, f_s2h, SX, SY, HX, HY);
    
    animate_dot(dot_solar_batt, phase_1, f_s2b, SX, SY, BX, BY);
    animate_dot(dot_solar_batt_2, phase_2, f_s2b, SX, SY, BX, BY);
    animate_dot(dot_solar_batt_3, phase_3, f_s2b, SX, SY, BX, BY);
    
    animate_dot(dot_solar_grid, phase_1, f_s2g, SX, SY, GX, GY);
    animate_dot(dot_solar_grid_2, phase_2, f_s2g, SX, SY, GX, GY);
    animate_dot(dot_solar_grid_3, phase_3, f_s2g, SX, SY, GX, GY);

    // Grid flows (gray dots)
    animate_dot(dot_grid_home, phase_1, f_g2h, GX, GY, HX, HY);
    animate_dot(dot_grid_home_2, phase_2, f_g2h, GX, GY, HX, HY);
    animate_dot(dot_grid_home_3, phase_3, f_g2h, GX, GY, HX, HY);
    
    animate_dot(dot_grid_batt, phase_1, f_g2b, GX, GY, BX, BY);
    animate_dot(dot_grid_batt_2, phase_2, f_g2b, GX, GY, BX, BY);
    animate_dot(dot_grid_batt_3, phase_3, f_g2b, GX, GY, BX, BY);

    // Battery flows (green dots)
    animate_dot(dot_batt_home, phase_1, f_b2h, BX, BY, HX, HY);
    animate_dot(dot_batt_home_2, phase_2, f_b2h, BX, BY, HX, HY);
    animate_dot(dot_batt_home_3, phase_3, f_b2h, BX, BY, HX, HY);
    
    animate_dot(dot_batt_grid, phase_1, f_b2g, BX, BY, GX, GY);
    animate_dot(dot_batt_grid_2, phase_2, f_b2g, BX, BY, GX, GY);
    animate_dot(dot_batt_grid_3, phase_3, f_b2g, BX, BY, GX, GY);
}
