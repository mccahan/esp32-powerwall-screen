# Powerwall Display

A standalone touchscreen display for monitoring Tesla Powerwall systems. Shows real-time power flow between solar, battery, grid, and home.

## Features

- Real-time power flow visualization with animated flow indicators
- Battery state of charge with backup time remaining
- Off-grid status indicator
- Automatic WiFi/MQTT reconnection
- Web-based configuration at `http://powerwall-display.local`
- Captive portal for easy WiFi setup

## Hardware

**Guition ESP32-S3-4848S040** - 4" 480x480 IPS capacitive touchscreen

| Store | Link |
|-------|------|
| AliExpress | [ESP32-S3-4848S040](https://www.aliexpress.us/item/3256809197960152.html) |
| Amazon | [ESP32-S3 4848S040](https://www.amazon.com/ESP32-S3-Development-Bluetooth-Interaction-Industrial/dp/B0FMK5JTLY) |

## Requirements

- [pypowerwall](https://github.com/jasonacox/pypowerwall) with MQTT publishing enabled
- MQTT broker (e.g., Mosquitto)
- USB-C cable for flashing

## Quick Start

### 1. Flash Firmware

**Option A: Web Installer (Recommended)**

Visit the [Web Installer](https://mccahan.github.io/esp32-powerwall-screen/) to flash directly from your browser (Chrome/Edge).

**Option B: Build from Source**

```bash
pip install platformio
git clone https://github.com/mccahan/esp32-powerwall-screen.git
cd esp32-powerwall-screen
pio run -t upload
```

### 2. Connect to WiFi

1. Connect your phone to the **"Powerwall-Display"** WiFi network
2. A setup page opens automatically (or visit `http://192.168.4.1`)
3. Select your network and enter the password
4. Device restarts and connects

### 3. Configure MQTT

1. Open `http://powerwall-display.local/config` in your browser
2. Enter your MQTT settings:
   - **Host**: MQTT broker address
   - **Port**: 1883 (default)
   - **Topic Prefix**: `pypowerwall/` (default)
3. Click Save

## pypowerwall Configuration

Enable MQTT publishing in your `pypowerwall.conf`:

```ini
[MQTT]
enabled = true
broker = your-mqtt-broker
port = 1883
topic = pypowerwall
```

### Subscribed Topics

| Topic | Data |
|-------|------|
| `{prefix}solar/instant_power` | Solar production (W) |
| `{prefix}battery/instant_power` | Battery charge/discharge (W) |
| `{prefix}load/instant_power` | Home consumption (W) |
| `{prefix}site/instant_power` | Grid import/export (W) |
| `{prefix}battery/level` | State of charge (%) |
| `{prefix}battery/time_remaining` | Backup time (hours) |
| `{prefix}site/offgrid` | Off-grid status (0/1) |

## Settings

Access `http://powerwall-display.local/config` to configure:

- **Display Rotation**: 0°, 90°, 180°, or 270° (requires reboot)
- **MQTT Settings**: Broker connection details

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "WiFi not configured" | Connect to "Powerwall-Display" network |
| "Connection failed" | Check credentials, device retries every 10s |
| No data showing | Verify MQTT broker and pypowerwall are running |
| Can't access web UI | Try IP address instead of .local hostname |

## Development

```bash
pio run              # Build
pio run -t upload    # Flash
pio device monitor   # Serial output (115200 baud)
```

## License

MIT License - See [LICENSE](LICENSE) for details.
