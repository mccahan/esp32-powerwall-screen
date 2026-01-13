#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <Arduino_GFX_Library.h>

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

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

// Forward declarations
void setupDisplay();
void setupLVGL();
void setupImprovWiFi();
void createHelloWorldUI();

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

    Serial.println("ESP32 Powerwall Status Display Starting...");

    // Check PSRAM
    if (psramFound()) {
        Serial.printf("PSRAM found: %d bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    } else {
        Serial.println("WARNING: PSRAM not found");
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
    lv_timer_handler();
    delay(5);
}

void setupDisplay() {
    Serial.println("Initializing display...");

    // Initialize the display with 16MHz clock
    gfx->begin(16000000);
    gfx->fillScreen(BLACK);

    // Setup backlight
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    Serial.println("Display initialized");
}

void setupLVGL() {
    Serial.println("Initializing LVGL...");

    lv_init();

    // Allocate LVGL draw buffer from internal RAM (per manufacturer recommendation)
    // Using 200 lines for better performance
    size_t buf_size = TFT_WIDTH * 200;
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!disp_draw_buf) {
        Serial.println("LVGL draw buffer allocation failed, trying PSRAM...");
        disp_draw_buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * buf_size);
    }

    if (!disp_draw_buf) {
        Serial.println("FATAL: Failed to allocate LVGL draw buffer!");
        while (1) { delay(1000); }
    }

    Serial.printf("LVGL buffer allocated: %d bytes\n", sizeof(lv_color_t) * buf_size);

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, buf_size);

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    Serial.println("LVGL initialized");
}

void setupImprovWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin();

    Serial.println("WiFi mode set to STA");
    Serial.println("Improv WiFi ready for configuration");
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
