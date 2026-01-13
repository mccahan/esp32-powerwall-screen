#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <LovyanGFX.hpp>

// Include ESP32-S3 specific panels and buses
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

// Display configuration for Guition ESP32-S3-4848S040
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_RGB _panel_instance;
    lgfx::Bus_RGB _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_GT911 _touch_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            // RGB data pins - red (5 pins)
            cfg.pin_d0  = GPIO_NUM_11;  // R1
            cfg.pin_d1  = GPIO_NUM_12;  // R2
            cfg.pin_d2  = GPIO_NUM_13;  // R3
            cfg.pin_d3  = GPIO_NUM_14;  // R4
            cfg.pin_d4  = GPIO_NUM_0;   // R5
            // RGB data pins - green (6 pins)
            cfg.pin_d5  = GPIO_NUM_8;   // G0
            cfg.pin_d6  = GPIO_NUM_20;  // G1
            cfg.pin_d7  = GPIO_NUM_3;   // G2
            cfg.pin_d8  = GPIO_NUM_46;  // G3
            cfg.pin_d9  = GPIO_NUM_9;   // G4
            cfg.pin_d10 = GPIO_NUM_10;  // G5
            // RGB data pins - blue (5 pins)
            cfg.pin_d11 = GPIO_NUM_4;   // B1
            cfg.pin_d12 = GPIO_NUM_5;   // B2
            cfg.pin_d13 = GPIO_NUM_6;   // B3
            cfg.pin_d14 = GPIO_NUM_7;   // B4
            cfg.pin_d15 = GPIO_NUM_15;  // B5

            // Control pins
            cfg.pin_henable = GPIO_NUM_18; // DE (Display Enable)
            cfg.pin_vsync = GPIO_NUM_17;   // VSYNC
            cfg.pin_hsync = GPIO_NUM_16;   // HSYNC
            cfg.pin_pclk = GPIO_NUM_21;    // PCLK (Pixel Clock)
            cfg.freq_write = 12000000;

            cfg.hsync_polarity = 0;
            cfg.hsync_front_porch = 10;
            cfg.hsync_pulse_width = 8;
            cfg.hsync_back_porch = 20;

            cfg.vsync_polarity = 0;
            cfg.vsync_front_porch = 10;
            cfg.vsync_pulse_width = 8;
            cfg.vsync_back_porch = 10;

            cfg.pclk_active_neg = 0;
            cfg.de_idle_high = 0;
            cfg.pclk_idle_high = 0;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.memory_width = 480;
            cfg.memory_height = 480;
            cfg.panel_width = 480;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }

        {
            // ST7701S SPI configuration for sending init commands
            auto cfg = _panel_instance.config_detail();
            cfg.pin_cs = GPIO_NUM_39;
            cfg.pin_sclk = GPIO_NUM_48;
            cfg.pin_mosi = GPIO_NUM_47;
            cfg.use_psram = 2;  // Use PSRAM for frame buffer
            _panel_instance.config_detail(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = GPIO_NUM_38;
            cfg.invert = false;
            cfg.freq = 1200;
            cfg.pwm_channel = 1;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = 479;
            cfg.y_min = 0;
            cfg.y_max = 479;
            cfg.pin_int = GPIO_NUM_NC;
            cfg.pin_rst = GPIO_NUM_NC;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;

            cfg.i2c_port = 0;  // I2C port 0
            cfg.pin_sda = GPIO_NUM_19;
            cfg.pin_scl = GPIO_NUM_45;
            cfg.freq = 400000;
            // GT911 I2C address - 0x14 for most boards, 0x5D for some variants
            // Hardware specific to guition-esp32-s3-4848s040
            cfg.i2c_addr = 0x14;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

static LGFX gfx;

// Display pins for Guition ESP32-S3-4848S040
#define TFT_BL 38
#define TFT_WIDTH 480
#define TFT_HEIGHT 480

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

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
    // Handle LVGL tasks
    lv_timer_handler();
    delay(5);
}

void setupDisplay() {
    // Initialize LovyanGFX display
    gfx.init();
    gfx.setRotation(0);
    gfx.setBrightness(255);
    gfx.fillScreen(TFT_BLACK);
    
    Serial.println("Display hardware initialized");
}

void setupLVGL() {
    lv_init();
    
    // Allocate buffer memory from PSRAM if available
    size_t buffer_size = TFT_WIDTH * 40;
    if (psramFound()) {
        buf1 = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * buffer_size);
        buf2 = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * buffer_size);
        Serial.println("LVGL buffers allocated in PSRAM");
    } else {
        buf1 = (lv_color_t *)malloc(sizeof(lv_color_t) * buffer_size);
        buf2 = (lv_color_t *)malloc(sizeof(lv_color_t) * buffer_size);
        Serial.println("LVGL buffers allocated in heap");
    }
    
    // Initialize the display buffer
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buffer_size);
    
    // Initialize the display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = [](lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);
        
        gfx.startWrite();
        gfx.setAddrWindow(area->x1, area->y1, w, h);
        gfx.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
        gfx.endWrite();
        
        lv_disp_flush_ready(disp);
    };
    
    lv_disp_drv_register(&disp_drv);
    
    // Initialize the touch driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
        uint16_t touchX, touchY;
        bool touched = gfx.getTouch(&touchX, &touchY);
        
        if (touched) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touchX;
            data->point.y = touchY;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    };
    lv_indev_drv_register(&indev_drv);
    
    Serial.println("LVGL initialized");
}

void setupImprovWiFi() {
    // Configure WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    
    Serial.println("WiFi mode set to STA");
    Serial.println("Improv WiFi ready for configuration");
    Serial.println("Connect via ESP Web Tools to configure WiFi credentials");
    
    // Note: Full Improv WiFi protocol implementation would be added here
    // The improv-wifi SDK provides the protocol for receiving WiFi credentials
    // via serial communication from ESP Web Tools in the browser.
    // This typically involves:
    // 1. Advertising Improv service via serial
    // 2. Listening for credential commands
    // 3. Connecting to WiFi with provided credentials
    // 4. Reporting connection status back to the browser
    
    // For initial setup, WiFi credentials can also be hardcoded or 
    // configured via other means (e.g., WiFiManager, BLE, etc.)
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
