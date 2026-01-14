#include "time_config.h"

// Global instance
TimeConfigManager timeConfig;

TimeConfigManager::TimeConfigManager() {
}

void TimeConfigManager::begin() {
    preferences.begin("time", false);
    
    config.ntpServer = preferences.getString("ntpServer", DEFAULT_NTP_SERVER);
    config.timezone = preferences.getString("timezone", DEFAULT_TIMEZONE);
    config.ntpEnabled = preferences.getBool("ntpEnabled", true);
    
    preferences.end();
    
    // Initialize NTP if enabled
    if (config.ntpEnabled) {
        syncTime();
    }
}

void TimeConfigManager::saveConfig() {
    preferences.begin("time", false);
    
    preferences.putString("ntpServer", config.ntpServer);
    preferences.putString("timezone", config.timezone);
    preferences.putBool("ntpEnabled", config.ntpEnabled);
    
    preferences.end();
    
    // Re-sync time with new settings
    if (config.ntpEnabled) {
        syncTime();
    }
}

TimeConfig& TimeConfigManager::getConfig() {
    return config;
}

void TimeConfigManager::syncTime() {
    if (!config.ntpEnabled) {
        return;
    }
    
    Serial.printf("Syncing time with NTP server: %s\n", config.ntpServer.c_str());
    Serial.printf("Timezone: %s\n", config.timezone.c_str());
    
    // Configure NTP with timezone
    configTime(0, 0, config.ntpServer.c_str());
    setenv("TZ", config.timezone.c_str(), 1);
    tzset();
    
    // Wait for time to be set (with timeout)
    struct tm timeinfo;
    int retries = 0;
    while (!getLocalTime(&timeinfo) && retries < 10) {
        delay(500);
        retries++;
    }
    
    if (retries < 10) {
        timeSynced = true;
        Serial.printf("Time synced: %02d:%02d:%02d\n", 
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        Serial.println("Failed to sync time with NTP server");
        timeSynced = false;
    }
}

bool TimeConfigManager::isTimeSynced() {
    return timeSynced;
}

bool TimeConfigManager::getLocalTime(struct tm *timeinfo) {
    if (!::getLocalTime(timeinfo)) {
        return false;
    }
    return true;
}

int TimeConfigManager::getCurrentHour() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return -1;  // Error getting time
    }
    return timeinfo.tm_hour;
}
