#ifndef BRIGHTNESS_CONFIG_H
#define BRIGHTNESS_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

// Idle timeout options (in seconds)
enum IdleTimeout {
    IDLE_NEVER = 0,
    IDLE_5_SEC = 5,
    IDLE_15_SEC = 15,
    IDLE_30_SEC = 30,
    IDLE_60_SEC = 60
};

struct BrightnessConfig {
    // Day/Night brightness settings (0-100)
    uint8_t dayBrightness = 100;
    uint8_t nightBrightness = 60;
    
    // Time range for day mode (hours 0-23)
    uint8_t dayStartHour = 7;   // 7 AM
    uint8_t dayEndHour = 22;     // 10 PM
    
    // Idle dimming settings
    bool idleDimmingEnabled = false;
    IdleTimeout idleTimeout = IDLE_NEVER;
    uint8_t idleBrightness = 80;  // Percentage to dim to when idle
};

class BrightnessConfigManager {
public:
    BrightnessConfigManager();

    void begin();
    void saveConfig();

    BrightnessConfig& getConfig();

    // Helper to convert timeout value to enum
    static IdleTimeout secondsToTimeout(int seconds);
    static int timeoutToSeconds(IdleTimeout timeout);

private:
    BrightnessConfig config;
    Preferences preferences;
};

extern BrightnessConfigManager brightnessConfig;

#endif // BRIGHTNESS_CONFIG_H
