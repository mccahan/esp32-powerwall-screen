#include "screenshot.h"
#include <SPIFFS.h>
#include <lvgl.h>

#define SCREENSHOT_PATH "/screenshot.bmp"
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

// BMP file header structures
#pragma pack(push, 1)
typedef struct {
    uint16_t signature;      // "BM"
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;
} BMPFileHeader;

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pixels_per_meter;
    int32_t  y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t important_colors;
} BMPInfoHeader;
#pragma pack(pop)

void initScreenshot() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully");
}

// Convert RGB565 to RGB888 (24-bit BMP)
void rgb565_to_rgb888(uint16_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = ((rgb565 >> 11) & 0x1F) << 3;  // 5 bits red
    *g = ((rgb565 >> 5) & 0x3F) << 2;   // 6 bits green
    *b = (rgb565 & 0x1F) << 3;          // 5 bits blue
}

bool captureScreenshot() {
    Serial.println("Capturing screenshot...");
    
    // Get LVGL display and screen buffer
    lv_disp_t* disp = lv_disp_get_default();
    if (!disp) {
        Serial.println("No display found");
        return false;
    }

    // Force LVGL to render everything
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(disp);
    
    // Get the active draw buffer
    lv_disp_draw_buf_t* draw_buf = lv_disp_get_draw_buf(disp);
    if (!draw_buf || !draw_buf->buf_act) {
        Serial.println("No active buffer");
        return false;
    }
    
    lv_color_t* buf = (lv_color_t*)draw_buf->buf_act;
    
    // Delete old screenshot if exists
    if (SPIFFS.exists(SCREENSHOT_PATH)) {
        SPIFFS.remove(SCREENSHOT_PATH);
    }
    
    // Create BMP file
    File file = SPIFFS.open(SCREENSHOT_PATH, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to create screenshot file");
        return false;
    }
    
    // Calculate sizes
    uint32_t row_size = ((SCREEN_WIDTH * 3 + 3) / 4) * 4;  // BMP rows must be multiple of 4 bytes
    uint32_t image_size = row_size * SCREEN_HEIGHT;
    uint32_t file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + image_size;
    
    // Write BMP file header
    BMPFileHeader file_header = {0};
    file_header.signature = 0x4D42;  // "BM"
    file_header.file_size = file_size;
    file_header.data_offset = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    file.write((uint8_t*)&file_header, sizeof(BMPFileHeader));
    
    // Write BMP info header
    BMPInfoHeader info_header = {0};
    info_header.header_size = sizeof(BMPInfoHeader);
    info_header.width = SCREEN_WIDTH;
    info_header.height = SCREEN_HEIGHT;  // Positive = bottom-up
    info_header.planes = 1;
    info_header.bits_per_pixel = 24;
    info_header.compression = 0;  // No compression
    info_header.image_size = image_size;
    file.write((uint8_t*)&info_header, sizeof(BMPInfoHeader));
    
    // Write pixel data (BMP is bottom-up, so write rows in reverse)
    uint8_t* row_buffer = (uint8_t*)malloc(row_size);
    if (!row_buffer) {
        Serial.println("Failed to allocate row buffer");
        file.close();
        return false;
    }
    
    for (int y = SCREEN_HEIGHT - 1; y >= 0; y--) {
        memset(row_buffer, 0, row_size);
        
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int pixel_index = y * SCREEN_WIDTH + x;
            uint16_t rgb565 = buf[pixel_index].full;
            
            uint8_t r, g, b;
            rgb565_to_rgb888(rgb565, &r, &g, &b);
            
            // BMP uses BGR order
            row_buffer[x * 3 + 0] = b;
            row_buffer[x * 3 + 1] = g;
            row_buffer[x * 3 + 2] = r;
        }
        
        file.write(row_buffer, row_size);
    }
    
    free(row_buffer);
    file.close();
    
    Serial.printf("Screenshot saved: %d bytes\n", file_size);
    return true;
}

String getScreenshotPath() {
    return String(SCREENSHOT_PATH);
}

bool hasScreenshot() {
    return SPIFFS.exists(SCREENSHOT_PATH);
}

void deleteScreenshot() {
    if (SPIFFS.exists(SCREENSHOT_PATH)) {
        SPIFFS.remove(SCREENSHOT_PATH);
    }
}
