# ESP32 Powerwall Display

A real-time energy flow visualization display for Tesla Powerwall systems using an ESP32-S3 with a 480x480 RGB display and LVGL graphics library.

![Powerwall Display](https://img.shields.io/badge/Platform-ESP32--S3-blue)
![Framework](https://img.shields.io/badge/Framework-Arduino-green)
![Graphics](https://img.shields.io/badge/Graphics-LVGL-orange)

## Features

- 🎨 **Beautiful LVGL Interface** - Modern, responsive UI with animated power flows
- ⚡ **Real-time Monitoring** - Live display of grid, solar, battery, and home power data
- 📡 **MQTT Integration** - Subscribes to pypowerwall or similar MQTT data sources
- 📶 **ImprovWiFi Support** - Configure WiFi credentials at flash time via web browser
- 🔧 **Captive Portal Setup** - Alternative WiFi and MQTT configuration via web interface
- 🌐 **Web-based Firmware Installation** - Flash firmware directly from your browser
- 🔋 **Battery Status** - Shows charge level and charging/discharging state
- 🏠 **Energy Flow Visualization** - Dynamic lines showing power flow direction

## Hardware Requirements

- **ESP32-S3** development board with:
  - 16MB Flash
  - PSRAM (octal PSRAM recommended)
- **480x480 ST7701S RGB Display** (e.g., Guition ESP32-S3-4848S040)
- **GT911 Touchscreen** controller (I2C)
- USB-C cable for power and programming

### Recommended Hardware

The [Guition ESP32-S3-4848S040](https://github.com/agillis/esphome-modular-lvgl-buttons/blob/main/hardware/guition-esp32-s3-4848s040.yaml) is a perfect all-in-one solution with:
- ESP32-S3-WROOM-1 module
- 480x480 RGB display with ST7701S controller
- GT911 capacitive touch
- 16MB Flash
- 8MB PSRAM
- Built-in backlight control

## Quick Start

### Web Installation (Easiest)

1. Visit the [Web Installer](https://mccahan.github.io/esp32-powerwall-screen/)
2. Connect your ESP32-S3 via USB
3. Click "Install Powerwall Display"
4. Select your device's COM port
5. Wait for installation to complete

### First Time Setup

**Option 1: ImprovWiFi (Recommended)**

1. After flashing via the web installer, you'll be prompted to configure WiFi
2. Enter your WiFi SSID and password directly in the browser
3. The device connects automatically - no manual AP setup needed!
4. Configure MQTT settings via the captive portal on subsequent boot

**Option 2: Captive Portal (Fallback)**

1. If ImprovWiFi wasn't used, the device creates a WiFi access point: `Powerwall-Display`
2. Connect to it using password: `configure`
3. Configure your settings in the captive portal:
   - WiFi SSID and password
   - MQTT server address
   - MQTT port (default: 1883)
   - MQTT username/password (if required)
   - MQTT topic prefix (e.g., `powerwall` or `pypowerwall`)
4. Save and the device will connect to your network

## MQTT Topics

The display subscribes to these topics (replace `{prefix}` with your configured prefix):

| Topic | Description | Example Value |
|-------|-------------|---------------|
| `{prefix}/site/instant_power` | Grid power in watts | `1500` (importing), `-500` (exporting) |
| `{prefix}/solar/instant_power` | Solar generation in watts | `3200` |
| `{prefix}/battery/instant_power` | Battery power in watts | `800` (discharging), `-1200` (charging) |
| `{prefix}/battery/level` | Battery charge percentage | `50.4` |
| `{prefix}/load/instant_power` | Home consumption in watts | `2400` |

### Example with pypowerwall

If you're using [pypowerwall](https://github.com/jasonacox/pypowerwall), set your MQTT prefix to `pypowerwall` and the display will automatically subscribe to the correct topics.

## Display Layout

```
        ┌─────────┐
        │  SOLAR  │  (Yellow, Top)
        │  3.2kW  │
        └─────────┘
             │
    ┌────────┼────────┐
    │        │        │
┌───┴───┐    │    ┌───┴───┐
│  GRID │────┼────│  HOME │  (Blue, Right)
│ 1.5kW │    │    │ 2.4kW │
└───────┘    │    └───────┘
             │
        ┌────┴────┐
        │ BATTERY │  (Green, Bottom)
        │ 800W    │
        │  50%    │
        └─────────┘
```

## Building from Source

### Prerequisites

- [PlatformIO](https://platformio.org/)
- USB drivers for your ESP32-S3 board (CH340, CP2102, or built-in USB)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/mccahan/esp32-powerwall-screen.git
cd esp32-powerwall-screen

# Build the firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

### Configuration

Edit `include/config.h` to customize:
- Default AP name and password
- Default MQTT prefix
- Display settings
- Pin assignments (if using different hardware)

## Development

The project uses:
- **PlatformIO** for build system
- **Arduino Framework** for ESP32-S3
- **LVGL 8.3** for graphics
- **WiFiManager** for captive portal
- **PubSubClient** for MQTT
- **Arduino_GFX** for display driver

### Project Structure

```
esp32-powerwall-screen/
├── src/
│   └── main.cpp              # Main application code
├── include/
│   ├── config.h              # Configuration constants
│   └── lv_conf.h             # LVGL configuration
├── docs/
│   ├── index.html            # Web installer page
│   └── manifest.json         # ESP Web Tools manifest
├── .github/
│   └── workflows/
│       └── build.yml         # GitHub Actions CI/CD
└── platformio.ini            # PlatformIO configuration
```

## GitHub Actions

The project includes automated builds via GitHub Actions:
- Builds firmware on every push
- Deploys to GitHub Pages
- Creates firmware artifacts for releases

## Troubleshooting

### Display not working
- Verify correct hardware (ESP32-S3 with ST7701S display)
- Check display ribbon cable connection
- Ensure backlight is powered (pin 38)

### WiFi connection fails
- Device will automatically enter AP mode
- Connect to `Powerwall-Display` AP to reconfigure

### MQTT not connecting
- Verify MQTT server is reachable from ESP32
- Check MQTT credentials
- Ensure topics match your MQTT broker

### No data showing
- Check MQTT broker is publishing to correct topics
- Monitor serial output for incoming messages
- Verify topic prefix matches your configuration

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is open source. See LICENSE file for details.

## Credits

- Inspired by Tesla Powerwall energy flow displays
- Hardware reference from [esphome-modular-lvgl-buttons](https://github.com/agillis/esphome-modular-lvgl-buttons)
- MQTT data source: [pypowerwall](https://github.com/jasonacox/pypowerwall)

## Support

For issues, questions, or suggestions:
- [GitHub Issues](https://github.com/mccahan/esp32-powerwall-screen/issues)
- [Discussions](https://github.com/mccahan/esp32-powerwall-screen/discussions)

