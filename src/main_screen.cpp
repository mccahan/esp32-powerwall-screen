#include "main_screen.h"
#include "info_screen.h"
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

// MQTT status monitoring
static unsigned long last_mqtt_status_update = 0;
static bool last_mqtt_status = false;

// Power flow animation state
static float g_grid_w = 0.0f;
static float g_home_w = 0.0f;
static float g_solar_w = 0.0f;
static float g_batt_w = 0.0f;
static float g_soc = 0.0f;
static float ph_master = 0.0f;
static unsigned long g_last_anim_ms = 0;

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

    // ========== Layout Background Image (created after dots so dots appear underneath) ==========
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
    lv_imgbtn_set_src(btn_info, LV_IMGBTN_STATE_RELEASED, NULL, &info_icon, NULL);
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
    g_solar_w = watts;  // Store for animation
    if (lbl_solar_val) {
        char buf[24];
        float kw = watts / 1000.0f;
        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_solar_val, buf);
    }
    last_data_ms = millis();
}

void updateGridValue(float watts) {
    g_grid_w = watts;  // Store for animation
    if (lbl_grid_val) {
        char buf[24];
        float kw = watts / 1000.0f;
        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_grid_val, buf);
    }
    last_data_ms = millis();
}

void updateHomeValue(float watts) {
    g_home_w = watts;  // Store for animation
    if (lbl_home_val) {
        char buf[24];
        float kw = watts / 1000.0f;
        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_home_val, buf);
    }
    last_data_ms = millis();
}

void updateBatteryValue(float watts) {
    g_batt_w = watts;  // Store for animation
    if (lbl_batt_val) {
        char buf[24];
        float kw = watts / 1000.0f;
        snprintf(buf, sizeof(buf), "%.1f kW", kw);
        lv_label_set_text(lbl_batt_val, buf);
    }
    last_data_ms = millis();
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

    if (bar_soc) {
        lv_bar_set_value(bar_soc, (int)roundf(adjusted), LV_ANIM_OFF);
    }
    
    last_data_ms = millis();
}

// ============== Power Flow Dot Animation ==============

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
    const uint32_t DEFAULT_FRAME_MS = 1000 / 30;  // 30 FPS (matches YAML implementation)
    
    // Animation speed parameters
    const float SPEED_DIVISOR = 2500.0f;    // Normalize power to speed
    const float MIN_SPEED = 0.18f;          // Minimum animation speed
    const float MAX_SPEED = 0.25f;          // Maximum animation speed
    
    // Opacity mapping parameters
    const float OPACITY_SCALE = 200.0f;     // Scale factor for opacity calculation
    const float OPACITY_FLOOR = 10.0f;      // Minimum opacity value
    
    // Battery state of charge threshold
    const float BATTERY_FULL_THRESHOLD = 99.5f;  // Consider battery full at this SOC

    // Helper functions
    auto clampf = [](float v, float a, float b) -> float {
        if (v < a) return a;
        if (v > b) return b;
        return v;
    };

    auto lerp_i = [](int a, int b, float t) -> int {
        return (int)lroundf((float)a + ((float)b - (float)a) * t);
    };

    auto set_dot_centered = [&](lv_obj_t* dot, int x, int y) {
        lv_obj_set_pos(dot, x - DOT_R, y - DOT_R);
    };

    auto set_dot_alpha = [&](lv_obj_t* dot, float a01) {
        a01 = clampf(a01, 0.0f, 1.0f);
        // Calculate opacity value and clamp before casting to prevent overflow
        float opa_float = (a01 * OPACITY_SCALE) + OPACITY_FLOOR;
        if (opa_float > (float)LV_OPA_MAX) opa_float = (float)LV_OPA_MAX;
        lv_opa_t opa = (lv_opa_t)lroundf(opa_float);
        lv_obj_set_style_bg_opa(dot, opa, 0);
    };

    auto show_dot = [&](lv_obj_t* dot) {
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    };

    auto hide_dot = [&](lv_obj_t* dot) {
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    };

    auto ease_linear = [&](float t) -> float {
        return clampf(t, 0.0f, 1.0f);
    };

    auto advance_master_phase = [&](float ph, float max_watts_active, float dt_seconds) -> float {
        float speed = clampf(max_watts_active / SPEED_DIVISOR, MIN_SPEED, MAX_SPEED);
        ph += speed * dt_seconds;
        if (ph >= 1.0f) ph -= floorf(ph);
        return ph;
    };

    // Place dot on a two-segment path: source → center → sink
    auto place_on_two_segment_path = [&](lv_obj_t* dot, float t,
                                         int x_src, int y_src,
                                         int x_sink, int y_sink) {
        t = clampf(t, 0.0f, 1.0f);

        const float MIDPOINT = 0.5f;
        const float SEGMENT_SCALE = 2.0f;
        
        int x, y;
        float segment_t;
        
        if (t < MIDPOINT) {
            // First segment: source → center
            segment_t = t * SEGMENT_SCALE;
            x = lerp_i(x_src, CX, segment_t);
            y = lerp_i(y_src, CY, segment_t);
        } else {
            // Second segment: center → sink
            segment_t = (t - MIDPOINT) * SEGMENT_SCALE;
            x = lerp_i(CX, x_sink, segment_t);
            y = lerp_i(CY, y_sink, segment_t);
        }

        set_dot_centered(dot, x, y);

        // Fade at the very start and very end
        float a = 1.0f;
        if (t < FADE) a = t / FADE;
        else if (t > (1.0f - FADE)) a = (1.0f - t) / FADE;
        set_dot_alpha(dot, a);
    };

    auto animate = [&](lv_obj_t* dot, float eased_t, float watts,
                       int x_src, int y_src, int x_sink, int y_sink) {
        if (watts < THRESH_W) {
            hide_dot(dot);
            return;
        }

        show_dot(dot);
        place_on_two_segment_path(dot, eased_t, x_src, y_src, x_sink, y_sink);
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
        hide_dot(dot_solar_home);
        hide_dot(dot_solar_home_2);
        hide_dot(dot_solar_home_3);
        hide_dot(dot_solar_batt);
        hide_dot(dot_solar_batt_2);
        hide_dot(dot_solar_batt_3);
        hide_dot(dot_solar_grid);
        hide_dot(dot_solar_grid_2);
        hide_dot(dot_solar_grid_3);
        hide_dot(dot_grid_home);
        hide_dot(dot_grid_home_2);
        hide_dot(dot_grid_home_3);
        hide_dot(dot_grid_batt);
        hide_dot(dot_grid_batt_2);
        hide_dot(dot_grid_batt_3);
        hide_dot(dot_batt_home);
        hide_dot(dot_batt_home_2);
        hide_dot(dot_batt_home_3);
        hide_dot(dot_batt_grid);
        hide_dot(dot_batt_grid_2);
        hide_dot(dot_batt_grid_3);
        return;
    }

    // Calculate elapsed time
    const unsigned long now = millis();
    const unsigned long last_anim = g_last_anim_ms;
    unsigned long elapsed_ms;
    
    if (last_anim == 0 || now < last_anim) {
        elapsed_ms = DEFAULT_FRAME_MS;
    } else {
        elapsed_ms = now - last_anim;
    }
    
    const float dt_seconds = (float)elapsed_ms / 1000.0f;
    g_last_anim_ms = now;

    ph_master = advance_master_phase(ph_master, max_active, dt_seconds);

    // Calculate phases for 3 dots evenly spaced
    const float phase_offset_1 = 1.0f / 3.0f;
    const float phase_offset_2 = 2.0f / 3.0f;
    
    const float phase_1 = ph_master;
    const float phase_2 = fmodf(ph_master + phase_offset_1, 1.0f);
    const float phase_3 = fmodf(ph_master + phase_offset_2, 1.0f);

    const float eased_t1 = ease_linear(phase_1);
    const float eased_t2 = ease_linear(phase_2);
    const float eased_t3 = ease_linear(phase_3);

    // Animate all flows
    // Solar flows (yellow dots)
    animate(dot_solar_home, eased_t1, f_s2h, SX, SY, HX, HY);
    animate(dot_solar_home_2, eased_t2, f_s2h, SX, SY, HX, HY);
    animate(dot_solar_home_3, eased_t3, f_s2h, SX, SY, HX, HY);
    
    animate(dot_solar_batt, eased_t1, f_s2b, SX, SY, BX, BY);
    animate(dot_solar_batt_2, eased_t2, f_s2b, SX, SY, BX, BY);
    animate(dot_solar_batt_3, eased_t3, f_s2b, SX, SY, BX, BY);
    
    animate(dot_solar_grid, eased_t1, f_s2g, SX, SY, GX, GY);
    animate(dot_solar_grid_2, eased_t2, f_s2g, SX, SY, GX, GY);
    animate(dot_solar_grid_3, eased_t3, f_s2g, SX, SY, GX, GY);

    // Grid flows (gray dots)
    animate(dot_grid_home, eased_t1, f_g2h, GX, GY, HX, HY);
    animate(dot_grid_home_2, eased_t2, f_g2h, GX, GY, HX, HY);
    animate(dot_grid_home_3, eased_t3, f_g2h, GX, GY, HX, HY);
    
    animate(dot_grid_batt, eased_t1, f_g2b, GX, GY, BX, BY);
    animate(dot_grid_batt_2, eased_t2, f_g2b, GX, GY, BX, BY);
    animate(dot_grid_batt_3, eased_t3, f_g2b, GX, GY, BX, BY);

    // Battery flows (green dots)
    animate(dot_batt_home, eased_t1, f_b2h, BX, BY, HX, HY);
    animate(dot_batt_home_2, eased_t2, f_b2h, BX, BY, HX, HY);
    animate(dot_batt_home_3, eased_t3, f_b2h, BX, BY, HX, HY);
    
    animate(dot_batt_grid, eased_t1, f_b2g, BX, BY, GX, GY);
    animate(dot_batt_grid_2, eased_t2, f_b2g, BX, BY, GX, GY);
    animate(dot_batt_grid_3, eased_t3, f_b2g, BX, BY, GX, GY);
}
