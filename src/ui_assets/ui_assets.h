#ifndef UI_ASSETS_H
#define UI_ASSETS_H

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

// Font declarations
LV_FONT_DECLARE(space_bold_21);
LV_FONT_DECLARE(space_bold_30);

// Image declarations
extern const lv_img_dsc_t icon_battery_img;
extern const lv_img_dsc_t icon_center_img;
extern const lv_img_dsc_t icon_grid_img;
extern const lv_img_dsc_t icon_grid_offline_img;
extern const lv_img_dsc_t icon_home_img;
extern const lv_img_dsc_t icon_solar_img;
extern const lv_img_dsc_t info_icon_img;

#endif // UI_ASSETS_H
