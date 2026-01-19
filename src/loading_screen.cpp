#include "loading_screen.h"

// Display dimensions
#define TFT_WIDTH 480
#define TFT_HEIGHT 480

// Theme colors
#define COLOR_BG        0x0A0C10
#define COLOR_WHITE     0xFFFFFF
#define COLOR_GRAY      0x4A4A4A

// UI elements - Loading screen overlay
static lv_obj_t *loading_screen = nullptr;
static lv_obj_t *loading_spinner = nullptr;
static lv_obj_t *loading_label = nullptr;

void createLoadingScreen(lv_obj_t* parent) {
    // Create full-screen overlay
    loading_screen = lv_obj_create(parent);
    lv_obj_set_size(loading_screen, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_set_pos(loading_screen, 0, 0);
    lv_obj_set_style_bg_color(loading_screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(loading_screen, LV_OPA_90, 0);
    lv_obj_set_style_border_width(loading_screen, 0, 0);
    lv_obj_clear_flag(loading_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(loading_screen, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(loading_screen, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // Create spinner (centered)
    loading_spinner = lv_spinner_create(loading_screen, 1000, 60);
    lv_obj_set_size(loading_spinner, 80, 80);
    lv_obj_center(loading_spinner);
    lv_obj_set_style_arc_color(loading_spinner, lv_color_hex(COLOR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(loading_spinner, lv_color_hex(COLOR_GRAY), LV_PART_INDICATOR);

    // Create label below spinner
    loading_label = lv_label_create(loading_screen);
    lv_label_set_text(loading_label, "Waiting for data...");
    lv_obj_set_style_text_color(loading_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(loading_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(loading_label, LV_ALIGN_CENTER, 0, 60);
}

void showLoadingScreen() {
    if (loading_screen) {
        lv_obj_clear_flag(loading_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(loading_screen);
    }
}

void hideLoadingScreen() {
    if (loading_screen) {
        lv_obj_add_flag(loading_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

bool isLoadingScreenVisible() {
    if (loading_screen) {
        return !lv_obj_has_flag(loading_screen, LV_OBJ_FLAG_HIDDEN);
    }
    return false;
}
