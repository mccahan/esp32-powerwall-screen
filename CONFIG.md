# Configuration Example

This file shows example configuration values. 
The actual configuration is done through the captive portal web interface on first boot.

## WiFi Configuration
- **AP Name:** Powerwall-Display
- **AP Password:** configure

## MQTT Configuration Examples

### Example 1: pypowerwall
```
MQTT Server: 192.168.1.100
MQTT Port: 1883
MQTT User: (leave empty if no auth)
MQTT Password: (leave empty if no auth)
MQTT Topic Prefix: pypowerwall
```

This will subscribe to:
- pypowerwall/site/instant_power
- pypowerwall/solar/instant_power
- pypowerwall/battery/instant_power
- pypowerwall/battery/level
- pypowerwall/load/instant_power

### Example 2: Custom prefix
```
MQTT Server: mqtt.example.com
MQTT Port: 1883
MQTT User: powerwall_user
MQTT Password: your_password_here
MQTT Topic Prefix: home/energy
```

This will subscribe to:
- home/energy/site/instant_power
- home/energy/solar/instant_power
- home/energy/battery/instant_power
- home/energy/battery/level
- home/energy/load/instant_power

## MQTT Message Format

All topics should publish numeric values as strings:

```
Topic: pypowerwall/site/instant_power
Payload: "1500.5"

Topic: pypowerwall/solar/instant_power
Payload: "3200.0"

Topic: pypowerwall/battery/instant_power
Payload: "-800.2"

Topic: pypowerwall/battery/level
Payload: "75.3"

Topic: pypowerwall/load/instant_power
Payload: "2400.7"
```

### Power Values
- **Positive** grid power = importing from grid
- **Negative** grid power = exporting to grid
- **Positive** battery power = discharging
- **Negative** battery power = charging
- Solar and load are always positive

## Reconfiguration

To reconfigure the device:
1. The device will enter configuration mode automatically if it cannot connect to WiFi
2. Or, connect to the "Powerwall-Display" AP manually at any time
3. Access the configuration portal and update your settings
