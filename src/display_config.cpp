#include "display_config.h"

// Global instance
DisplayConfigManager displayConfig;

DisplayConfigManager::DisplayConfigManager() {
}

void DisplayConfigManager::begin() {
    preferences.begin("display", false);
    config.rotation = static_cast<DisplayRotation>(preferences.getUChar("rotation", ROTATION_0));
    preferences.end();
}

void DisplayConfigManager::saveConfig() {
    preferences.begin("display", false);
    preferences.putUChar("rotation", static_cast<uint8_t>(config.rotation));
    preferences.end();
}

DisplayConfig& DisplayConfigManager::getConfig() {
    return config;
}

DisplayRotation DisplayConfigManager::degreesToRotation(int degrees) {
    switch (degrees) {
        case 90:  return ROTATION_90;
        case 180: return ROTATION_180;
        case 270: return ROTATION_270;
        default:  return ROTATION_0;
    }
}

int DisplayConfigManager::rotationToDegrees(DisplayRotation rotation) {
    switch (rotation) {
        case ROTATION_90:  return 90;
        case ROTATION_180: return 180;
        case ROTATION_270: return 270;
        default:           return 0;
    }
}
