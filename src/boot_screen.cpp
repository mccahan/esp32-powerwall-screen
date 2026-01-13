#include "boot_screen.h"

// Theme colors (matching ESPHome config)
#define COLOR_BG        0x0A0C10
#define COLOR_WHITE     0xFFFFFF

// Boot screen elements
static lv_obj_t *boot_screen = nullptr;
static lv_obj_t *boot_spinner = nullptr;

// Boot screen timing
unsigned long boot_start_time = 0;

void createBootScreen(lv_obj_t *parent_screen) {
    // Boot screen overlay
    boot_screen = lv_obj_create(parent_screen);
    lv_obj_set_size(boot_screen, 480, 480);
    lv_obj_set_pos(boot_screen, 0, 0);
    lv_obj_set_style_bg_color(boot_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(boot_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(boot_screen, 0, 0);
    lv_obj_set_style_radius(boot_screen, 0, 0);
    lv_obj_clear_flag(boot_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Title on boot screen
    lv_obj_t *boot_title = lv_label_create(boot_screen);
    lv_label_set_text(boot_title, "Powerwall Display");
    lv_obj_set_style_text_color(boot_title, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(boot_title, &lv_font_montserrat_32, 0);
    lv_obj_align(boot_title, LV_ALIGN_CENTER, 0, -80);

    // Spinner
    boot_spinner = lv_spinner_create(boot_screen, 1000, 60);
    lv_obj_set_size(boot_spinner, 80, 80);
    lv_obj_align(boot_spinner, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_arc_color(boot_spinner, lv_color_hex(0x313336), LV_PART_MAIN);
    lv_obj_set_style_arc_width(boot_spinner, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(boot_spinner, lv_color_hex(0x137FEC), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(boot_spinner, 10, LV_PART_INDICATOR);

    // Status label on boot screen
    lv_obj_t *boot_status = lv_label_create(boot_screen);
    lv_label_set_text(boot_status, "Connecting...");
    lv_obj_set_style_text_color(boot_status, lv_color_hex(0x6A6A6A), 0);
    lv_obj_set_style_text_font(boot_status, &lv_font_montserrat_16, 0);
    lv_obj_align(boot_status, LV_ALIGN_CENTER, 0, 100);
}

void showBootScreen() {
    if (boot_screen) {
        lv_obj_clear_flag(boot_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void hideBootScreen() {
    if (boot_screen) {
        lv_obj_add_flag(boot_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

bool isBootScreenVisible() {
    if (boot_screen) {
        return !lv_obj_has_flag(boot_screen, LV_OBJ_FLAG_HIDDEN);
    }
    return false;
}
