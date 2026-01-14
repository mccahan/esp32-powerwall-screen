#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

// Display rotation options (user-friendly degrees)
enum DisplayRotation {
    ROTATION_0 = 0,     // Normal orientation
    ROTATION_90 = 1,    // 90 degrees clockwise
    ROTATION_180 = 2,   // 180 degrees (upside down)
    ROTATION_270 = 3    // 270 degrees clockwise (90 counter-clockwise)
};

struct DisplayConfig {
    DisplayRotation rotation = ROTATION_0;
};

class DisplayConfigManager {
public:
    DisplayConfigManager();

    void begin();
    void saveConfig();

    DisplayConfig& getConfig();

    // Convert user rotation (0, 90, 180, 270) to internal rotation value
    static DisplayRotation degreesToRotation(int degrees);
    static int rotationToDegrees(DisplayRotation rotation);

private:
    DisplayConfig config;
    Preferences preferences;
};

extern DisplayConfigManager displayConfig;

#endif // DISPLAY_CONFIG_H
