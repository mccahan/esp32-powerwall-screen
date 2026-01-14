#ifndef TIME_CONFIG_H
#define TIME_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

// NTP server defaults
#define DEFAULT_NTP_SERVER "pool.ntp.org"
#define DEFAULT_TIMEZONE "UTC0"  // UTC with no DST

struct TimeConfig {
    String ntpServer = DEFAULT_NTP_SERVER;
    String timezone = DEFAULT_TIMEZONE;  // POSIX timezone string (e.g., "PST8PDT,M3.2.0,M11.1.0")
    bool ntpEnabled = true;
};

class TimeConfigManager {
public:
    TimeConfigManager();

    void begin();
    void saveConfig();
    
    TimeConfig& getConfig();
    
    // NTP synchronization
    void syncTime();
    bool isTimeSynced();
    
    // Get current local time
    bool getLocalTime(struct tm *timeinfo);
    int getCurrentHour();  // Returns current hour (0-23) in local time

private:
    TimeConfig config;
    Preferences preferences;
    bool timeSynced = false;
};

extern TimeConfigManager timeConfig;

#endif // TIME_CONFIG_H
