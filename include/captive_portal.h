#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>

// AP configuration
#define AP_SSID "Powerwall-Display"
#define AP_PASSWORD ""  // Open network for easy setup

// Start the captive portal (AP mode + DNS + web server)
void startCaptivePortal();

// Stop the captive portal and switch to station mode
void stopCaptivePortal();

// Process captive portal requests (call in loop)
void loopCaptivePortal();

// Check if captive portal is active
bool isCaptivePortalActive();

#endif // CAPTIVE_PORTAL_H
