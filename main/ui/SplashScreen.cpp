#include "lvgl.h"

#include "ui/SplashScreen.hpp"

namespace {

constexpr const char *kSplashImagePath = "P:/spiflash/notificat.bmp";

} // namespace

extern "C" void app_splash_show(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFCF7), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *image = lv_image_create(screen);
    lv_image_set_src(image, kSplashImagePath);
    lv_obj_center(image);
}
