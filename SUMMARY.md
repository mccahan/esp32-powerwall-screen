# ESP32 Powerwall Display - Project Summary

## Overview

This project implements a real-time energy flow visualization system for Tesla Powerwall installations using an ESP32-S3 microcontroller with a 480x480 RGB display. The system displays live power data from solar panels, grid connection, battery storage, and home consumption in an intuitive graphical interface.

## Key Features

### Hardware Support
- **ESP32-S3** with 16MB Flash and PSRAM
- **480x480 ST7701S RGB Display** (direct parallel interface)
- **GT911 Capacitive Touchscreen** (I2C)
- Optimized for Guition ESP32-S3-4848S040 hardware

### Software Features
- **LVGL 8.3** Graphics library for smooth, modern UI
- **WiFi Manager** with captive portal for easy configuration
- **MQTT Client** for real-time data subscription
- **Animated Power Flows** showing energy movement
- **Web-based Firmware Installation** via ESP Web Tools
- **Automatic WiFi Reconnection** and MQTT retry logic

### User Experience
- Zero-code configuration via web interface
- Support for decimal power values (e.g., 50.4%)
- Color-coded components:
  - Grid: White (left)
  - Solar: Yellow (top)
  - Battery: Green (bottom)
  - Home: Blue (right)

## Project Structure

```
esp32-powerwall-screen/
├── src/
│   └── main.cpp              # Main application (600+ lines)
├── include/
│   ├── config.h              # Pin definitions and constants
│   └── lv_conf.h             # LVGL configuration
├── docs/
│   ├── index.html            # Web installer page
│   ├── manifest.json         # ESP Web Tools manifest
│   └── firmware/             # Built firmware files (generated)
├── examples/
│   ├── mqtt_test_publisher.py # Python MQTT test script
│   └── README.md             # Examples documentation
├── .github/
│   └── workflows/
│       └── build.yml         # CI/CD pipeline
├── platformio.ini            # Build configuration
├── README.md                 # Main documentation
├── CONFIG.md                 # Configuration guide
└── LICENSE                   # MIT License
```

## MQTT Integration

The system subscribes to pypowerwall-compatible topics:

```
{prefix}/site/instant_power      - Grid power (W)
{prefix}/solar/instant_power     - Solar generation (W)
{prefix}/battery/instant_power   - Battery power (W)
{prefix}/battery/level           - Battery charge (%)
{prefix}/load/instant_power      - Home consumption (W)
```

Power values are interpreted as:
- **Grid**: Positive = importing, Negative = exporting
- **Battery**: Positive = discharging, Negative = charging
- **Solar/Load**: Always positive

## Build & Deploy

### GitHub Actions Workflow
The project includes automated CI/CD:
1. Builds firmware on every push to main
2. Generates bootloader, partitions, and firmware binaries
3. Deploys to GitHub Pages
4. Creates downloadable artifacts

### Local Development
```bash
# Install PlatformIO
pip install platformio

# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

## Installation Methods

### 1. Web Installer (Recommended)
Visit https://mccahan.github.io/esp32-powerwall-screen/
- No software installation required
- Works in Chrome, Edge, Opera
- One-click firmware installation

### 2. Manual Upload
Download firmware from GitHub releases:
- bootloader.bin @ 0x0
- partitions.bin @ 0x8000
- firmware.bin @ 0x10000

## Configuration Process

1. **First Boot**: Device creates AP "Powerwall-Display"
2. **Connect**: Join AP with password "configure"
3. **Configure**: Web portal opens automatically
4. **Enter Settings**:
   - WiFi SSID and password
   - MQTT server address and port
   - MQTT credentials (if required)
   - MQTT topic prefix (e.g., "pypowerwall")
5. **Save**: Device connects and begins displaying data

## Display Layout

```
        ┌───────────┐
        │   SOLAR   │  ← Yellow, shows generation
        │  3.2 kW   │
        └─────┬─────┘
              │
    ┌─────────┼─────────┐
    │         │         │
┌───┴───┐     │     ┌───┴───┐
│ GRID  │─────┼─────│ HOME  │  ← Blue, shows consumption
│1.5 kW │     │     │2.4 kW │
└───────┘     │     └───────┘
              │
        ┌─────┴─────┐
        │  BATTERY  │  ← Green, shows charge/discharge
        │  800 W    │
        │   50%     │
        └───────────┘
```

Animated lines show power flow:
- Brightness indicates power magnitude
- Direction shows energy flow
- Multiple simultaneous flows supported

## Testing

### MQTT Test Publisher
Included Python script simulates pypowerwall data:

```bash
pip install paho-mqtt
python examples/mqtt_test_publisher.py --broker 192.168.1.100 --prefix pypowerwall
```

Generates realistic power data for testing without actual hardware.

## Code Quality

- ✅ Code review completed and all issues resolved
- ✅ Security scan with CodeQL passed
- ✅ No dead code or unused definitions
- ✅ Thread-safe implementation
- ✅ Proper error handling
- ✅ Comprehensive documentation

## Libraries Used

| Library | Version | Purpose |
|---------|---------|---------|
| LVGL | 8.3.11 | Graphics and UI |
| PubSubClient | 2.8 | MQTT client |
| ArduinoJson | 6.21.3 | JSON parsing |
| WiFiManager | 2.0.16-rc.2 | Captive portal |
| GFX Library | 1.4.7 | Display driver |

## Future Enhancements

Potential improvements for future versions:
- Historical data graphs
- Energy usage statistics
- Cost calculations
- Weather integration
- Touch controls for settings
- Multi-language support
- Custom color themes
- Over-the-air (OTA) updates

## Contributing

Contributions welcome! Areas for improvement:
- Additional display layouts
- Support for more MQTT brokers
- Alternative hardware configurations
- UI enhancements
- Documentation improvements

## License

MIT License - See LICENSE file for details

## Credits

- Hardware reference: [esphome-modular-lvgl-buttons](https://github.com/agillis/esphome-modular-lvgl-buttons)
- Data source: [pypowerwall](https://github.com/jasonacox/pypowerwall)
- Graphics: [LVGL](https://lvgl.io/)

## Support

- **Issues**: GitHub Issues tracker
- **Discussions**: GitHub Discussions
- **Documentation**: README.md and CONFIG.md

## Version History

### v1.0.0 (2026-01-09)
- Initial release
- ESP32-S3 support with RGB display
- LVGL UI with animated power flows
- WiFi Manager captive portal
- MQTT integration
- Web-based installer
- Comprehensive documentation

---

**Built with ❤️ for the Tesla Powerwall community**
