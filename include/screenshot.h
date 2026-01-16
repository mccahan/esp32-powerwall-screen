#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <Arduino.h>

// Initialize screenshot storage
void initScreenshot();

// Capture screenshot from LVGL framebuffer and save to SPIFFS
bool captureScreenshot();

// Get path to latest screenshot file
String getScreenshotPath();

// Check if screenshot exists
bool hasScreenshot();

// Delete screenshot file
void deleteScreenshot();

#endif // SCREENSHOT_H
