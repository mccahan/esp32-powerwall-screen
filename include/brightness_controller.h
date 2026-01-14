#ifndef BRIGHTNESS_CONTROLLER_H
#define BRIGHTNESS_CONTROLLER_H

#include <Arduino.h>
#include "brightness_config.h"
#include "time_config.h"

// Backlight pin for ESP32-S3-4848S040
#define BACKLIGHT_PIN 38

// PWM configuration for backlight
#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8  // 8-bit resolution (0-255)

class BrightnessController {
public:
    BrightnessController();

    void begin();
    void update();  // Call from main loop to handle time-based and idle dimming
    
    // Brightness control
    void setBrightness(uint8_t brightness);  // Set brightness 0-100%
    uint8_t getCurrentBrightness();
    
    // Idle detection
    void onTouchDetected();  // Call when touch is detected
    
private:
    uint8_t currentBrightness;
    uint8_t targetBrightness;  // For scheduled brightness
    unsigned long lastTouchTime;
    bool isDimmedByIdle;
    
    // Internal helpers
    void applyBrightness(uint8_t brightness);
    uint8_t getScheduledBrightness();  // Get brightness based on time of day
    bool shouldDimForIdle();
};

extern BrightnessController brightnessController;

#endif // BRIGHTNESS_CONTROLLER_H
