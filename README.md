# ESP32 Powerwall Screen

Display Tesla Powerwall system status on a 480x480 ESP32-S3 display.

## Features

- 🎨 **LVGL-based UI**: Beautiful graphics on a 480x480 pixel display
- 📡 **Improv WiFi**: Easy WiFi configuration via browser
- 🔌 **Web-based Flashing**: Install firmware directly from your browser using ESP Web Tools
- 💾 **ESP32-S3**: Optimized for ESP32-S3 devices with 16MB flash
- ⚡ **Hardware Support**: Designed for Guition ESP32-S3-4848S040
- 🌐 **Web Configuration**: Configure MQTT settings via web interface
- 📊 **Real-time MQTT**: Async MQTT client for real-time power data updates

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

## MQTT Configuration

Once WiFi is connected, configure MQTT to receive real-time power data:

1. Note the IP address shown on the display status bar
2. Open a web browser and navigate to `http://<device-ip>/config`
3. Configure the following settings:
   - **MQTT Host**: Your MQTT broker hostname or IP
   - **MQTT Port**: Usually 1883 (default)
   - **MQTT Username**: Optional authentication username
   - **MQTT Password**: Optional authentication password
   - **Topic Prefix**: Match your pypowerwall setup (default: `pypowerwall/`)
4. Click "Save Configuration"
5. The display will automatically connect to your MQTT broker

### Expected MQTT Topics

The display subscribes to the following topics (with configurable prefix):

- `{prefix}solar/instant_power` - Solar power (W)
- `{prefix}site/instant_power` - Grid power (W) 
- `{prefix}load/instant_power` - Home/Load power (W)
- `{prefix}battery/instant_power` - Battery power (W)
- `{prefix}battery/level` - Battery state of charge (%)
- `{prefix}site/offgrid` - Grid connection status (1=off-grid, 0=on-grid)
- `{prefix}battery/time_remaining` - Backup time remaining (hours)

These topics are published by [pypowerwall](https://github.com/jasonacox/pypowerwall) when configured with MQTT support.

## Development

### Project Structure

```
esp32-powerwall-screen/
├── src/
│   ├── main.cpp           # Main application code
│   └── ui_assets/         # Generated UI assets (fonts & images)
│       ├── space_bold_21.c      # SpaceGrotesk font (21px)
│       ├── space_bold_30.c      # SpaceGrotesk font (30px)
│       ├── layout_img.c         # Background layout with icons
│       ├── grid_offline_img.c   # Grid offline overlay
│       └── ui_assets.h          # Asset declarations
├── assets/                # Source assets (fonts & icons)
│   ├── SpaceGrotesk-*.ttf       # SpaceGrotesk font family
│   ├── layout.svg               # UI layout with power icons
│   └── grid_offline.svg         # Grid offline indicator
├── include/
│   └── lv_conf.h          # LVGL configuration
│   ├── mqtt_client.cpp    # Async MQTT client implementation
│   └── web_server.cpp     # Web server for configuration
├── include/
│   ├── lv_conf.h          # LVGL configuration
│   ├── mqtt_client.h      # MQTT client header
│   └── web_server.h       # Web server header
├── platformio.ini         # PlatformIO configuration
└── .github/
    └── workflows/
        └── build.yml      # CI/CD pipeline
```

### Regenerating UI Assets

If you modify fonts or icons in the `assets/` directory, regenerate the LVGL assets:

```bash
# Install Node.js conversion tools
npm install -g lv_font_conv

# Install Python dependencies for image conversion
pip3 install cairosvg pillow

# Convert fonts (already done, included in src/ui_assets/)
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 21 \
  --font assets/SpaceGrotesk-Bold.ttf --range 0x20-0x7F \
  --format lvgl -o src/ui_assets/space_bold_21.c

lv_font_conv --no-compress --no-prefilter --bpp 4 --size 30 \
  --font assets/SpaceGrotesk-Bold.ttf --range 0x20-0x7F \
  --format lvgl -o src/ui_assets/space_bold_30.c

# Convert SVG icons to C arrays using the provided script
python3 tools/convert_svg_to_lvgl.py assets/layout.svg \
  src/ui_assets/layout_img.c layout_img 480 480

python3 tools/convert_svg_to_lvgl.py assets/grid_offline.svg \
  src/ui_assets/grid_offline_img.c grid_offline_img 79 81
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

If PlatformIO upload fails, use esptool directly.

**Using PlatformIO's esptool (recommended):**

```bash
# Full flash with all partitions (replace /dev/cu.usbserial-10 with your port)
~/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32s3 \
  --port /dev/cu.usbserial-10 \
  --baud 921600 \
  --before default_reset \
  --after hard_reset \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 .pio/build/esp32-s3-devkitc-1/bootloader.bin \
  0x8000 .pio/build/esp32-s3-devkitc-1/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/esp32-s3-devkitc-1/firmware.bin
```

**Using system esptool (if installed via pip):**

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbserial-10 --baud 921600 \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 .pio/build/esp32-s3-devkitc-1/bootloader.bin \
  0x8000 .pio/build/esp32-s3-devkitc-1/partitions.bin \
  0x10000 .pio/build/esp32-s3-devkitc-1/firmware.bin
```

**To fully erase the device first (recommended for clean install):**

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbserial-10 erase_flash
```

**Find your serial port:**
- macOS: `ls /dev/cu.usb*`
- Linux: `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`
- Windows: Check Device Manager for COM port

### GitHub Actions

The project automatically builds firmware binaries on every push to main branch and deploys them to GitHub Pages for web-based installation.

## License

See LICENSE file for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.
