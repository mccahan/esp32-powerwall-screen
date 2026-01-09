/*
 * ESP32-S3 Powerwall Display - ESP-IDF Version
 * Power flow visualization with LVGL, MQTT, and WiFi provisioning
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "lvgl.h"
#include "esp_wifi_prov.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

#include "config.h"

static const char *TAG = "powerwall";

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Power data structure
typedef struct {
    float grid_power;      // Positive = importing, Negative = exporting
    float solar_power;     // Always positive
    float battery_power;   // Positive = discharging, Negative = charging
    float battery_level;   // Percentage 0-100
    float home_power;      // Always positive
} PowerData;

static PowerData power_data = {0};

// Configuration
static char mqtt_server[64] = "";
static char mqtt_port[6] = "1883";
static char mqtt_user[32] = "";
static char mqtt_password[64] = "";
static char mqtt_prefix[32] = DEFAULT_MQTT_PREFIX;

// MQTT topics
static char topic_grid_power[128];
static char topic_solar_power[128];
static char topic_battery_power[128];
static char topic_battery_level[128];
static char topic_home_power[128];

// LVGL objects
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;
static lv_obj_t *screen;
static lv_obj_t *grid_label, *solar_label, *battery_label, *home_label;
static lv_obj_t *grid_power_label, *solar_power_label, *battery_power_label;
static lv_obj_t *home_power_label, *battery_level_label;
static lv_obj_t *grid_to_home_line, *solar_to_home_line, *solar_to_battery_line;
static lv_obj_t *solar_to_grid_line, *battery_to_home_line, *battery_to_grid_line;

// MQTT client
static esp_mqtt_client_handle_t mqtt_client = NULL;

// LCD panel
static esp_lcd_panel_handle_t panel_handle = NULL;

// Forward declarations
static void wifi_init_sta(void);
static void mqtt_app_start(void);
static void load_config_from_nvs(void);
static void save_config_to_nvs(void);
static void build_mqtt_topics(void);
static void setup_display(void);
static void setup_lvgl(void);
static void create_ui(void);
static void update_ui(void);
static void draw_flow_line(lv_obj_t *line, int x1, int y1, int x2, int y2, lv_color_t color, bool active);

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, attempting reconnect...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize WiFi station mode
static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Try to load saved WiFi credentials
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("powerwall", NVS_READONLY, &nvs_handle);
    
    wifi_config_t wifi_config = {0};
    if (err == ESP_OK) {
        size_t ssid_len = sizeof(wifi_config.sta.ssid);
        size_t pass_len = sizeof(wifi_config.sta.password);
        
        nvs_get_str(nvs_handle, "wifi_ssid", (char*)wifi_config.sta.ssid, &ssid_len);
        nvs_get_str(nvs_handle, "wifi_pass", (char*)wifi_config.sta.password, &pass_len);
        nvs_close(nvs_handle);
    }

    // If no saved credentials, start WiFi provisioning
    if (strlen((char*)wifi_config.sta.ssid) == 0) {
        ESP_LOGI(TAG, "Starting WiFi provisioning...");
        
        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_softap,
            .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
        };
        
        ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
        
        bool provisioned = false;
        ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
        
        if (!provisioned) {
            ESP_LOGI(TAG, "Not provisioned, starting AP...");
            
            ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1,
                                                             "powerwall_prov",
                                                             "powerwall",
                                                             "Powerwall-Display"));
        } else {
            wifi_prov_mgr_deinit();
        }
    } else {
        ESP_LOGI(TAG, "Using saved WiFi credentials");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        // Subscribe to all topics
        esp_mqtt_client_subscribe(mqtt_client, topic_grid_power, 0);
        esp_mqtt_client_subscribe(mqtt_client, topic_solar_power, 0);
        esp_mqtt_client_subscribe(mqtt_client, topic_battery_power, 0);
        esp_mqtt_client_subscribe(mqtt_client, topic_battery_level, 0);
        esp_mqtt_client_subscribe(mqtt_client, topic_home_power, 0);
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Disconnected");
        break;
        
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT Data: %.*s = %.*s", event->topic_len, event->topic, event->data_len, event->data);
        
        // Parse the data
        char value_str[32] = {0};
        if (event->data_len < sizeof(value_str)) {
            memcpy(value_str, event->data, event->data_len);
            float value = atof(value_str);
            
            // Match topic and update power data
            if (strncmp(event->topic, topic_grid_power, event->topic_len) == 0) {
                power_data.grid_power = value;
            } else if (strncmp(event->topic, topic_solar_power, event->topic_len) == 0) {
                power_data.solar_power = value;
            } else if (strncmp(event->topic, topic_battery_power, event->topic_len) == 0) {
                power_data.battery_power = value;
            } else if (strncmp(event->topic, topic_battery_level, event->topic_len) == 0) {
                power_data.battery_level = value;
            } else if (strncmp(event->topic, topic_home_power, event->topic_len) == 0) {
                power_data.home_power = value;
            }
        }
        break;
        
    default:
        break;
    }
}

// Start MQTT client
static void mqtt_app_start(void)
{
    if (strlen(mqtt_server) == 0) {
        ESP_LOGW(TAG, "MQTT server not configured");
        return;
    }
    
    char mqtt_uri[128];
    snprintf(mqtt_uri, sizeof(mqtt_uri), "mqtt://%s:%s", mqtt_server, mqtt_port);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_uri,
    };
    
    if (strlen(mqtt_user) > 0) {
        mqtt_cfg.credentials.username = mqtt_user;
        mqtt_cfg.credentials.authentication.password = mqtt_password;
    }
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

// Load configuration from NVS
static void load_config_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("powerwall", NVS_READONLY, &nvs_handle);
    
    if (err == ESP_OK) {
        size_t len;
        
        len = sizeof(mqtt_server);
        nvs_get_str(nvs_handle, "mqtt_server", mqtt_server, &len);
        
        len = sizeof(mqtt_port);
        nvs_get_str(nvs_handle, "mqtt_port", mqtt_port, &len);
        
        len = sizeof(mqtt_user);
        nvs_get_str(nvs_handle, "mqtt_user", mqtt_user, &len);
        
        len = sizeof(mqtt_password);
        nvs_get_str(nvs_handle, "mqtt_pass", mqtt_password, &len);
        
        len = sizeof(mqtt_prefix);
        nvs_get_str(nvs_handle, "mqtt_prefix", mqtt_prefix, &len);
        
        nvs_close(nvs_handle);
        
        ESP_LOGI(TAG, "Loaded config - Server: %s, Prefix: %s", mqtt_server, mqtt_prefix);
    } else {
        ESP_LOGI(TAG, "No saved config, using defaults");
        strcpy(mqtt_prefix, DEFAULT_MQTT_PREFIX);
    }
}

// Save configuration to NVS
static void save_config_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("powerwall", NVS_READWRITE, &nvs_handle);
    
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, "mqtt_server", mqtt_server);
        nvs_set_str(nvs_handle, "mqtt_port", mqtt_port);
        nvs_set_str(nvs_handle, "mqtt_user", mqtt_user);
        nvs_set_str(nvs_handle, "mqtt_pass", mqtt_password);
        nvs_set_str(nvs_handle, "mqtt_prefix", mqtt_prefix);
        
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        ESP_LOGI(TAG, "Configuration saved");
    }
}

// Build MQTT topic strings from prefix
static void build_mqtt_topics(void)
{
    snprintf(topic_grid_power, sizeof(topic_grid_power), "%s/site/instant_power", mqtt_prefix);
    snprintf(topic_solar_power, sizeof(topic_solar_power), "%s/solar/instant_power", mqtt_prefix);
    snprintf(topic_battery_power, sizeof(topic_battery_power), "%s/battery/instant_power", mqtt_prefix);
    snprintf(topic_battery_level, sizeof(topic_battery_level), "%s/battery/level", mqtt_prefix);
    snprintf(topic_home_power, sizeof(topic_home_power), "%s/load/instant_power", mqtt_prefix);
    
    ESP_LOGI(TAG, "MQTT topics built with prefix: %s", mqtt_prefix);
}

// LVGL flush callback
static bool lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

// LVGL display flush
static void lvgl_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

// Setup RGB LCD panel
static void setup_display(void)
{
    ESP_LOGI(TAG, "Initializing RGB LCD panel");
    
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TFT_BACKLIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(TFT_BACKLIGHT, 1);
    
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = 16 * 1000 * 1000,
            .h_res = DISPLAY_WIDTH,
            .v_res = DISPLAY_HEIGHT,
            .hsync_pulse_width = 10,
            .hsync_back_porch = 10,
            .hsync_front_porch = 20,
            .vsync_pulse_width = 10,
            .vsync_back_porch = 10,
            .vsync_front_porch = 10,
        },
        .data_width = 16,
        .psram_trans_align = 64,
        .hsync_gpio_num = TFT_HSYNC,
        .vsync_gpio_num = TFT_VSYNC,
        .de_gpio_num = TFT_DE,
        .pclk_gpio_num = TFT_PCLK,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            TFT_D0, TFT_D1, TFT_D2, TFT_D3, TFT_D4, TFT_D5, TFT_D6, TFT_D7,
            TFT_D8, TFT_D9, TFT_D10, TFT_D11, TFT_D12, TFT_D13, TFT_D14, TFT_D15,
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
}

// Setup LVGL
static void setup_lvgl(void)
{
    ESP_LOGI(TAG, "Initializing LVGL");
    
    lv_init();
    
    // Allocate draw buffers in PSRAM
    disp_draw_buf = heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(disp_draw_buf != NULL);
    
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, DISPLAY_WIDTH * DISPLAY_HEIGHT);
    
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = lvgl_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_drv_register(&disp_drv);
}

// Create LVGL UI
static void create_ui(void)
{
    ESP_LOGI(TAG, "Creating UI");
    
    screen = lv_obj_create(NULL);
    lv_scr_load(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    
    // Component positions (circular layout)
    int center_x = DISPLAY_WIDTH / 2;
    int center_y = DISPLAY_HEIGHT / 2;
    int radius = 150;
    
    // Grid (left, white)
    grid_label = lv_label_create(screen);
    lv_label_set_text(grid_label, "GRID");
    lv_obj_set_style_text_color(grid_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(grid_label, &lv_font_montserrat_24, 0);
    lv_obj_align(grid_label, LV_ALIGN_LEFT_MID, 20, -30);
    
    grid_power_label = lv_label_create(screen);
    lv_label_set_text(grid_power_label, "0 W");
    lv_obj_set_style_text_color(grid_power_label, lv_color_white(), 0);
    lv_obj_align(grid_power_label, LV_ALIGN_LEFT_MID, 20, 0);
    
    // Solar (top, yellow)
    solar_label = lv_label_create(screen);
    lv_label_set_text(solar_label, "SOLAR");
    lv_obj_set_style_text_color(solar_label, lv_color_make(255, 255, 0), 0);
    lv_obj_set_style_text_font(solar_label, &lv_font_montserrat_24, 0);
    lv_obj_align(solar_label, LV_ALIGN_TOP_MID, 0, 20);
    
    solar_power_label = lv_label_create(screen);
    lv_label_set_text(solar_power_label, "0 W");
    lv_obj_set_style_text_color(solar_power_label, lv_color_make(255, 255, 0), 0);
    lv_obj_align(solar_power_label, LV_ALIGN_TOP_MID, 0, 50);
    
    // Battery (bottom, green)
    battery_label = lv_label_create(screen);
    lv_label_set_text(battery_label, "BATTERY");
    lv_obj_set_style_text_color(battery_label, lv_color_make(0, 255, 0), 0);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_24, 0);
    lv_obj_align(battery_label, LV_ALIGN_BOTTOM_MID, 0, -50);
    
    battery_power_label = lv_label_create(screen);
    lv_label_set_text(battery_power_label, "0 W");
    lv_obj_set_style_text_color(battery_power_label, lv_color_make(0, 255, 0), 0);
    lv_obj_align(battery_power_label, LV_ALIGN_BOTTOM_MID, 0, -30);
    
    battery_level_label = lv_label_create(screen);
    lv_label_set_text(battery_level_label, "0%");
    lv_obj_set_style_text_color(battery_level_label, lv_color_make(0, 255, 0), 0);
    lv_obj_align(battery_level_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // Home (right, blue)
    home_label = lv_label_create(screen);
    lv_label_set_text(home_label, "HOME");
    lv_obj_set_style_text_color(home_label, lv_color_make(0, 128, 255), 0);
    lv_obj_set_style_text_font(home_label, &lv_font_montserrat_24, 0);
    lv_obj_align(home_label, LV_ALIGN_RIGHT_MID, -20, -30);
    
    home_power_label = lv_label_create(screen);
    lv_label_set_text(home_power_label, "0 W");
    lv_obj_set_style_text_color(home_power_label, lv_color_make(0, 128, 255), 0);
    lv_obj_align(home_power_label, LV_ALIGN_RIGHT_MID, -20, 0);
    
    // Create flow lines (initially hidden)
    grid_to_home_line = lv_line_create(screen);
    solar_to_home_line = lv_line_create(screen);
    solar_to_battery_line = lv_line_create(screen);
    solar_to_grid_line = lv_line_create(screen);
    battery_to_home_line = lv_line_create(screen);
    battery_to_grid_line = lv_line_create(screen);
}

// Draw flow line
static void draw_flow_line(lv_obj_t *line, int x1, int y1, int x2, int y2, lv_color_t color, bool active)
{
    if (!active) {
        lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
    
    static lv_point_t points[2];
    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;
    
    lv_line_set_points(line, points, 2);
    lv_obj_set_style_line_width(line, 3, 0);
    lv_obj_set_style_line_color(line, color, 0);
}

// Update UI with current power data
static void update_ui(void)
{
    char buf[32];
    
    // Update power values
    snprintf(buf, sizeof(buf), "%.0f W", power_data.grid_power);
    lv_label_set_text(grid_power_label, buf);
    
    snprintf(buf, sizeof(buf), "%.0f W", power_data.solar_power);
    lv_label_set_text(solar_power_label, buf);
    
    snprintf(buf, sizeof(buf), "%.0f W", power_data.battery_power);
    lv_label_set_text(battery_power_label, buf);
    
    snprintf(buf, sizeof(buf), "%.0f W", power_data.home_power);
    lv_label_set_text(home_power_label, buf);
    
    snprintf(buf, sizeof(buf), "%.1f%%", power_data.battery_level);
    lv_label_set_text(battery_level_label, buf);
    
    // Update flow lines based on power flow
    int center_x = DISPLAY_WIDTH / 2;
    int center_y = DISPLAY_HEIGHT / 2;
    
    // Grid to/from home
    draw_flow_line(grid_to_home_line, 120, center_y, DISPLAY_WIDTH - 120, center_y,
                   lv_color_white(), power_data.grid_power > 50);
    
    // Solar to home
    draw_flow_line(solar_to_home_line, center_x, 80, DISPLAY_WIDTH - 100, center_y - 50,
                   lv_color_make(255, 255, 0), power_data.solar_power > 50);
    
    // Solar to battery
    draw_flow_line(solar_to_battery_line, center_x, 80, center_x, DISPLAY_HEIGHT - 100,
                   lv_color_make(255, 255, 0), power_data.solar_power > 50 && power_data.battery_power < -50);
    
    // Battery to home
    draw_flow_line(battery_to_home_line, center_x + 50, DISPLAY_HEIGHT - 100, DISPLAY_WIDTH - 100, center_y + 50,
                   lv_color_make(0, 255, 0), power_data.battery_power > 50);
}

// LVGL tick task
static void lvgl_tick_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
    }
}

// UI update task
static void ui_update_task(void *arg)
{
    while (1) {
        update_ui();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 Powerwall Display Starting (ESP-IDF)");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Setup backlight
    gpio_set_direction(TFT_BACKLIGHT, GPIO_MODE_OUTPUT);
    gpio_set_level(TFT_BACKLIGHT, 1);
    
    // Initialize display
    setup_display();
    
    // Initialize LVGL
    setup_lvgl();
    
    // Create UI
    create_ui();
    
    // Start LVGL task
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 4096, NULL, 5, NULL);
    xTaskCreate(ui_update_task, "ui_update", 4096, NULL, 4, NULL);
    
    // Load configuration
    load_config_from_nvs();
    build_mqtt_topics();
    
    // Initialize WiFi
    wifi_init_sta();
    
    // Wait for WiFi connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected, starting MQTT");
        mqtt_app_start();
    } else {
        ESP_LOGW(TAG, "WiFi connection failed");
    }
    
    ESP_LOGI(TAG, "Setup complete!");
}
