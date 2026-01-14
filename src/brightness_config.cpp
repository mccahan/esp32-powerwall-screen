#include "brightness_config.h"

// Global instance
BrightnessConfigManager brightnessConfig;

BrightnessConfigManager::BrightnessConfigManager() {
}

void BrightnessConfigManager::begin() {
    preferences.begin("brightness", false);
    
    config.dayBrightness = preferences.getUChar("dayBright", 100);
    config.nightBrightness = preferences.getUChar("nightBright", 60);
    config.dayStartHour = preferences.getUChar("dayStart", 7);
    config.dayEndHour = preferences.getUChar("dayEnd", 22);
    config.idleDimmingEnabled = preferences.getBool("idleEnabled", false);
    config.idleTimeout = static_cast<IdleTimeout>(preferences.getUChar("idleTimeout", IDLE_NEVER));
    config.idleBrightness = preferences.getUChar("idleBright", 80);
    
    preferences.end();
}

void BrightnessConfigManager::saveConfig() {
    preferences.begin("brightness", false);
    
    preferences.putUChar("dayBright", config.dayBrightness);
    preferences.putUChar("nightBright", config.nightBrightness);
    preferences.putUChar("dayStart", config.dayStartHour);
    preferences.putUChar("dayEnd", config.dayEndHour);
    preferences.putBool("idleEnabled", config.idleDimmingEnabled);
    preferences.putUChar("idleTimeout", static_cast<uint8_t>(config.idleTimeout));
    preferences.putUChar("idleBright", config.idleBrightness);
    
    preferences.end();
}

BrightnessConfig& BrightnessConfigManager::getConfig() {
    return config;
}

IdleTimeout BrightnessConfigManager::secondsToTimeout(int seconds) {
    switch (seconds) {
        case 5:  return IDLE_5_SEC;
        case 15: return IDLE_15_SEC;
        case 30: return IDLE_30_SEC;
        case 60: return IDLE_60_SEC;
        default: return IDLE_NEVER;
    }
}

int BrightnessConfigManager::timeoutToSeconds(IdleTimeout timeout) {
    return static_cast<int>(timeout);
}
