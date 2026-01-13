#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <esp_psram.h>

// Display pins for Guition ESP32-S3-4848S040
#define TFT_BL 38
#define TFT_WIDTH 480
#define TFT_HEIGHT 480

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[TFT_WIDTH * 40];
static lv_color_t buf2[TFT_WIDTH * 40];

// Forward declarations
void setupDisplay();
void setupLVGL();
void setupImprovWiFi();
void createHelloWorldUI();

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 Powerwall Status Display Starting...");
    
    // Initialize PSRAM
    if (psramFound()) {
        Serial.println("PSRAM found and initialized");
    } else {
        Serial.println("PSRAM not found");
    }
    
    // Setup display hardware
    setupDisplay();
    
    // Initialize LVGL
    setupLVGL();
    
    // Setup Improv WiFi
    setupImprovWiFi();
    
    // Create UI
    createHelloWorldUI();
    
    Serial.println("Setup complete!");
}

void loop() {
    // Handle LVGL tasks
    lv_timer_handler();
    delay(5);
}

void setupDisplay() {
    // Initialize backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    Serial.println("Display hardware initialized");
}

void setupLVGL() {
    lv_init();
    
    // Initialize the display buffer
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, TFT_WIDTH * 40);
    
    // Initialize the display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = [](lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
        // Placeholder flush callback - in a real implementation, this would
        // transfer the pixel data to the ST7701S display controller
        // For now, just mark as flushed
        lv_disp_flush_ready(disp);
    };
    
    lv_disp_drv_register(&disp_drv);
    
    Serial.println("LVGL initialized");
}

void setupImprovWiFi() {
    // Improv WiFi setup
    // This is a placeholder for Improv WiFi functionality
    // The improv-wifi library will handle WiFi credential configuration
    // via serial port when connected to ESP Web Tools
    
    WiFi.mode(WIFI_STA);
    Serial.println("WiFi mode set to STA");
    Serial.println("Improv WiFi ready for configuration");
    
    // In a full implementation, this would use the improv-wifi library
    // to listen for WiFi credentials via serial and configure WiFi accordingly
}

void createHelloWorldUI() {
    // Create a screen
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_scr_load(scr);
    
    // Set background color
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    
    // Create a label for "Hello World"
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello World");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);
    
    // Create a label for device info
    lv_obj_t *info_label = lv_label_create(scr);
    lv_label_set_text(info_label, "ESP32-S3 Powerwall Display\nWaiting for WiFi configuration...");
    lv_obj_set_style_text_color(info_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 40);
    
    Serial.println("Hello World UI created");
}
