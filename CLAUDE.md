# CLAUDE.md

This ESP32 code powers small displays like the Guition ESP32-S3-4848S040 to show the status and power flow of home energy systems (grid connection, home load, battery, solar). Current values (instantaneous power usage of each component, battery percentage, battery backup time remaining) are pulled through MQTT. Everything that is not display/animation should be done asynchronously or in backend threads whenever possible.

## Build Commands

When flashing, ignore the response contents unless the process exits status 1 to save context.

```bash
# Build firmware
pio run

# Build and upload via USB
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor

# Clean build
pio run -t clean

# Manual flash with esptool (if PlatformIO upload fails)
~/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32s3 --port /dev/cu.usbserial-10 --baud 460800 \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 .pio/build/esp32-s3-devkitc-1/bootloader.bin \
  0x8000 .pio/build/esp32-s3-devkitc-1/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/esp32-s3-devkitc-1/firmware.bin
```

## Architecture Overview

ESP32-S3 firmware displaying Tesla Powerwall status on a 480x480 RGB LCD (Guition ESP32-S3-4848S040).

### Display Pipeline

```
LVGL widgets → lv_timer_handler() → my_disp_flush() → Arduino_GFX → ST7701 LCD
```

- **Arduino_GFX v1.2.9** (in `lib/`): Manufacturer's display driver for ST7701 RGB panel
- **LVGL 8.3.x**: UI framework with 480×200 pixel line buffer (~195KB)
- **Frame buffer**: Uses PSRAM via `heap_caps_malloc()` for DMA-safe allocation

### WiFi Provisioning

Improv Serial protocol enables browser-based WiFi setup via ESP Web Tools:
- Credentials stored in ESP32 NVS (Preferences library)
- RPC commands: `WIFI_SETTINGS`, `GET_CURRENT_STATE`, `GET_DEVICE_INFO`, `GET_WIFI_NETWORKS`

### Key Hardware Configuration

- **Platform**: espressif32@6.8.1 (must use this version for Arduino_GFX v1.2.9 compatibility)
- **Memory**: Octal PSRAM (OPI mode), 16MB flash (DIO mode)
- **Display GPIOs**: RGB data (11-15, 0-10, 4-7), Sync (16-18, 21), Backlight (38)

### Power Display Integration Points

Functions in `main.cpp` ready for MQTT data:
```cpp
updateSolarValue(watts)    // Yellow
updateGridValue(watts)     // Gray
updateHomeValue(watts)     // Blue
updateBatteryValue(watts)  // Green
updateSOC(percent)         // Battery percentage bar
```

## Critical Dependencies

- `platform = espressif32@6.8.1` - Required for Arduino Core 2.x compatibility with Arduino_GFX
- `lib/Arduino_GFX/` - Local manufacturer library (do not upgrade to 1.6.x without Arduino Core 3.x)
- `sdkconfig.defaults` - Octal PSRAM and watchdog timing configuration
