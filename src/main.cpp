#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include "ui_assets/ui_assets.h"
#include "mqtt_client.h"
#include "web_server.h"
#include "boot_screen.h"
#include "main_screen.h"
#include "info_screen.h"
#include "improv_wifi.h"

// Touch controller pins for Guition ESP32-S3-4848S040
#define TOUCH_SDA 19
#define TOUCH_SCL 45
#define TOUCH_INT -1  // Not connected
#define TOUCH_RST -1  // Not connected

// Touch controller instance
TAMC_GT911 touchController(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, 480, 480);

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

// LVGL display buffers (double buffered)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf1;
static lv_color_t *disp_draw_buf2;
static lv_disp_drv_t disp_drv;

// LVGL tick tracking
static unsigned long last_tick = 0;

// LVGL touch input device
static lv_indev_drv_t indev_drv;
static lv_indev_t *touch_indev = nullptr;

// Touch read callback for LVGL
void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    touchController.read();

    if (touchController.isTouched) {
        data->state = LV_INDEV_STATE_PRESSED;

        // Invert coordinates to match screen orientation (origin at top-left)
        data->point.x = TFT_WIDTH - 1 - touchController.points[0].x;
        data->point.y = TFT_HEIGHT - 1 - touchController.points[0].y;

        // Log touch events for debugging
        Serial.printf("Touch: x=%d, y=%d\n", data->point.x, data->point.y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Forward declarations
void setupDisplay();
void setupLVGL();
void setupTouch();
void createUI();

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
    // Delay 10s for serial monitor attachment
    delay(10000);
    Serial.println("\n\nPowerwall Display Starting...");

    // Setup display hardware first
    setupDisplay();

    // Initialize LVGL
    setupLVGL();

    // Initialize touch
    setupTouch();

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

    if (wifi_preferences.begin("wifi", false)) {
        if (wifi_preferences.isKey("ssid")) {
            saved_ssid = wifi_preferences.getString("ssid", "");
            saved_pass = wifi_preferences.getString("password", "");
        }
        wifi_preferences.end();
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
    updateMQTTStatus();
    updatePowerFlowAnimation();

    // Update info screen data if visible
    if (isInfoScreenVisible()) {
        updateInfoScreenData();
    }

    // Check boot screen timeout
    if (isBootScreenVisible()) {
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

void setupTouch() {
    // Initialize I2C for touch controller
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    // Initialize GT911 touch controller
    touchController.begin();
    touchController.setRotation(ROTATION_NORMAL);

    Serial.println("GT911 touch controller initialized");

    // Register LVGL input device
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    touch_indev = lv_indev_drv_register(&indev_drv);

    if (touch_indev) {
        Serial.println("LVGL touch input device registered");
    } else {
        Serial.println("Failed to register LVGL touch input device!");
    }
}

void createUI() {
    createMainDashboard();
    createInfoScreen();
    createBootScreen(getMainScreen());
}
