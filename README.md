# ESP32 Powerwall Screen

Display Tesla Powerwall system status on a 480x480 ESP32-S3 display.

## Features

- 🎨 **LVGL-based UI**: Beautiful graphics on a 480x480 pixel display
- 📡 **Improv WiFi**: Easy WiFi configuration via browser
- 🔌 **Web-based Flashing**: Install firmware directly from your browser using ESP Web Tools
- 💾 **ESP32-S3**: Optimized for ESP32-S3 devices with 16MB flash
- ⚡ **Hardware Support**: Designed for Guition ESP32-S3-4848S040

## Hardware Requirements

- **Recommended**: Guition ESP32-S3-4848S040 (480x480 display)
- **MCU**: ESP32-S3 with 16MB flash and PSRAM
- **Display**: ST7701S-based 480x480 RGB LCD
- **Touch**: GT911 touch controller (I2C)

## Installation

### Option 1: Web Installer (Recommended)

Visit the [ESP32 Powerwall Display Web Installer](https://mccahan.github.io/esp32-powerwall-screen/) to flash firmware directly from your browser.

Requirements:
- Chrome, Edge, or Opera browser
- USB connection to your ESP32-S3 device

### Option 2: Build from Source

1. **Install PlatformIO**
   ```bash
   pip install platformio
   ```

2. **Clone the repository**
   ```bash
   git clone https://github.com/mccahan/esp32-powerwall-screen.git
   cd esp32-powerwall-screen
   ```

3. **Build and upload**
   ```bash
   pio run -t upload
   ```

## WiFi Configuration

After flashing the firmware, configure WiFi using Improv:

1. Connect to the device via USB
2. Open a serial terminal or use the web installer
3. Follow the Improv WiFi setup prompts
4. Enter your WiFi credentials

## Development

### Project Structure

```
esp32-powerwall-screen/
├── src/
│   └── main.cpp          # Main application code
├── include/
│   └── lv_conf.h         # LVGL configuration
├── platformio.ini        # PlatformIO configuration
└── .github/
    └── workflows/
        └── build.yml     # CI/CD pipeline
```

### Building

```bash
# Build firmware
pio run

# Clean build
pio run -t clean

# Build and upload via PlatformIO
pio run -t upload

# Monitor serial output
pio device monitor
```

### Manual Flashing with esptool

If PlatformIO upload fails, use esptool directly:

```bash
esptool.py --baud 460800 write_flash \
  0x0 .pio/build/esp32-s3-devkitc-1/bootloader.bin \
  0x8000 .pio/build/esp32-s3-devkitc-1/partitions.bin \
  0x10000 .pio/build/esp32-s3-devkitc-1/firmware.bin
```

To fully erase the device first (recommended for clean install):

```bash
esptool.py erase_flash
```

### GitHub Actions

The project automatically builds firmware binaries on every push to main branch and deploys them to GitHub Pages for web-based installation.

## License

See LICENSE file for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.
