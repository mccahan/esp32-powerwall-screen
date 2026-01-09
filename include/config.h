#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration - Will be configured via captive portal
// Default AP name for configuration
#define AP_NAME "Powerwall-Display"
#define AP_PASSWORD "configure"

// Configuration reset button (long press to reset settings)
#define CONFIG_RESET_PIN -1  // Set to a valid pin if needed, -1 to disable

// Default MQTT Configuration (can be overridden via captive portal)
#define DEFAULT_MQTT_PREFIX "powerwall"

// MQTT Topics for power data (prefix will be configured dynamically)
// Topics will be: {prefix}/site/instant_power, etc.
#define TOPIC_SITE_POWER "/site/instant_power"
#define TOPIC_SOLAR_POWER "/solar/instant_power"
#define TOPIC_BATTERY_POWER "/battery/instant_power"
#define TOPIC_BATTERY_LEVEL "/battery/level"
#define TOPIC_LOAD_POWER "/load/instant_power"

// Display Configuration
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

// Hardware Pins - Based on Guition ESP32-S3-4848S040
// Display ST7701S
#define TFT_CS 39
#define TFT_DE 18
#define TFT_VSYNC 17
#define TFT_HSYNC 16
#define TFT_PCLK 21
#define TFT_BACKLIGHT 38

// I2C for touchscreen GT911
#define I2C_SDA 19
#define I2C_SCL 45

// SPI for display
#define SPI_CLK 48
#define SPI_MOSI 47

// RGB data pins
#define TFT_D0  8   // Green 0
#define TFT_D1  11  // Red 1
#define TFT_D2  12  // Red 2
#define TFT_D3  13  // Red 3
#define TFT_D4  14  // Red 4
#define TFT_D5  0   // Red 5
#define TFT_D6  20  // Green 1
#define TFT_D7  3   // Green 2
#define TFT_D8  46  // Green 3
#define TFT_D9  9   // Green 4
#define TFT_D10 10  // Green 5
#define TFT_D11 4   // Blue 1
#define TFT_D12 5   // Blue 2
#define TFT_D13 6   // Blue 3
#define TFT_D14 7   // Blue 4
#define TFT_D15 15  // Blue 5

#endif // CONFIG_H
