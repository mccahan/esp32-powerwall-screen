#include "brightness_controller.h"

// Global instance
BrightnessController brightnessController;

BrightnessController::BrightnessController() 
    : currentBrightness(100), targetBrightness(100), lastTouchTime(0), isDimmedByIdle(false) {
}

void BrightnessController::begin() {
    // Configure PWM for backlight control
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(BACKLIGHT_PIN, PWM_CHANNEL);
    
    // Set initial brightness based on time of day
    targetBrightness = getScheduledBrightness();
    setBrightness(targetBrightness);
    
    lastTouchTime = millis();
    
    Serial.printf("Brightness controller initialized at %d%%\n", currentBrightness);
}

void BrightnessController::update() {
    // Update scheduled brightness based on time of day
    uint8_t scheduledBrightness = getScheduledBrightness();
    
    // Check if we need to dim for idle
    if (shouldDimForIdle()) {
        if (!isDimmedByIdle) {
            // Transition to idle brightness
            BrightnessConfig& config = brightnessConfig.getConfig();
            applyBrightness(config.idleBrightness);
            isDimmedByIdle = true;
            Serial.printf("Dimming to idle brightness: %d%%\n", config.idleBrightness);
        }
    } else {
        // Not idle or idle dimming disabled
        if (isDimmedByIdle) {
            // Restore from idle
            applyBrightness(scheduledBrightness);
            isDimmedByIdle = false;
            Serial.printf("Restored from idle to %d%%\n", scheduledBrightness);
        } else if (currentBrightness != scheduledBrightness) {
            // Update to scheduled brightness (day/night transition)
            applyBrightness(scheduledBrightness);
            Serial.printf("Scheduled brightness change to %d%%\n", scheduledBrightness);
        }
    }
}

void BrightnessController::setBrightness(uint8_t brightness) {
    // Clamp to 0-100
    if (brightness > 100) brightness = 100;
    
    applyBrightness(brightness);
    targetBrightness = brightness;
}

uint8_t BrightnessController::getCurrentBrightness() {
    return currentBrightness;
}

void BrightnessController::onTouchDetected() {
    lastTouchTime = millis();
    
    // If we were dimmed by idle, restore brightness immediately
    if (isDimmedByIdle) {
        uint8_t scheduledBrightness = getScheduledBrightness();
        applyBrightness(scheduledBrightness);
        isDimmedByIdle = false;
        Serial.printf("Touch detected - restoring brightness to %d%%\n", scheduledBrightness);
    }
}

void BrightnessController::applyBrightness(uint8_t brightness) {
    currentBrightness = brightness;
    
    // Convert 0-100% to 0-255 PWM duty cycle
    uint32_t pwmValue = (brightness * 255) / 100;
    ledcWrite(PWM_CHANNEL, pwmValue);
}

uint8_t BrightnessController::getScheduledBrightness() {
    BrightnessConfig& config = brightnessConfig.getConfig();
    
    // Get current hour from time config
    int currentHour = timeConfig.getCurrentHour();
    
    // If time not available, default to day brightness
    if (currentHour < 0) {
        return config.dayBrightness;
    }
    
    // Check if we're in day or night mode
    // Handle wrap-around case (e.g., 22:00 to 07:00)
    bool isDayTime;
    if (config.dayStartHour <= config.dayEndHour) {
        // Normal case: 7:00 to 22:00
        isDayTime = (currentHour >= config.dayStartHour && currentHour < config.dayEndHour);
    } else {
        // Wrap-around case: 22:00 to 07:00 (night is actually day)
        isDayTime = (currentHour >= config.dayStartHour || currentHour < config.dayEndHour);
    }
    
    return isDayTime ? config.dayBrightness : config.nightBrightness;
}

bool BrightnessController::shouldDimForIdle() {
    BrightnessConfig& config = brightnessConfig.getConfig();
    
    // Idle dimming disabled
    if (!config.idleDimmingEnabled || config.idleTimeout == IDLE_NEVER) {
        return false;
    }
    
    // Check if enough time has passed
    unsigned long idleTimeMs = config.idleTimeout * 1000UL;
    unsigned long timeSinceTouch = millis() - lastTouchTime;
    
    return timeSinceTouch >= idleTimeMs;
}
