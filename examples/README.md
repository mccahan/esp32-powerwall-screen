# Examples

This directory contains example scripts and configurations for testing and using the ESP32 Powerwall Display.

## MQTT Test Publisher

The `mqtt_test_publisher.py` script publishes random power data to MQTT topics for testing the display without a real Powerwall system.

### Installation

```bash
pip install paho-mqtt
```

### Usage

```bash
# Basic usage
python mqtt_test_publisher.py --broker 192.168.1.100

# With authentication
python mqtt_test_publisher.py --broker mqtt.example.com --user myuser --password mypass

# Custom prefix and update interval
python mqtt_test_publisher.py --broker 192.168.1.100 --prefix powerwall --interval 1.0
```

### Options

- `--broker`: MQTT broker address (required)
- `--port`: MQTT broker port (default: 1883)
- `--prefix`: MQTT topic prefix (default: pypowerwall)
- `--user`: MQTT username (optional)
- `--password`: MQTT password (optional)
- `--interval`: Update interval in seconds (default: 2.0)

The script will publish to these topics:
- `{prefix}/solar/instant_power`
- `{prefix}/site/instant_power`
- `{prefix}/battery/instant_power`
- `{prefix}/battery/level`
- `{prefix}/load/instant_power`
