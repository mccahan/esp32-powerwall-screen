# Powerwall Display

ESP32-S3 firmware for displaying Tesla Powerwall status on a 480x480 RGB LCD touchscreen.

## Hardware

**Guition ESP32-S3-4848S040** - 4" 480x480 IPS display with capacitive touch

Purchase:
- [AliExpress](https://www.aliexpress.us/item/3256809197960152.html)
- [Amazon](https://www.amazon.com/ESP32-S3-Development-Bluetooth-Interaction-Industrial/dp/B0FMK5JTLY)

## Build

```bash
pio run              # Build
pio run -t upload    # Flash via USB
pio device monitor   # Serial output (115200 baud)
```

## Architecture

- **Display**: LVGL 8.x → Arduino_GFX v1.2.9 → ST7701 RGB panel
- **Data**: AsyncMqttClient subscribes to pypowerwall MQTT topics
- **Config**: Web UI at `http://powerwall-display.local/config`
- **WiFi Setup**: Captive portal or Improv Serial (ESP Web Tools)

## Key Files

| File | Purpose |
|------|---------|
| `main.cpp` | Setup, main loop, display/touch init |
| `main_screen.cpp` | Power flow dashboard UI |
| `mqtt_client.cpp` | MQTT connection and topic subscriptions |
| `web_server.cpp` | Configuration web interface |
| `improv_wifi.cpp` | WiFi provisioning and connection management |
| `captive_portal.cpp` | AP mode setup when WiFi not configured |

## Dependencies

- `platform = espressif32@6.8.1` (required for Arduino_GFX compatibility)
- `lib/Arduino_GFX/` - Local display driver (do not upgrade without Arduino Core 3.x)
