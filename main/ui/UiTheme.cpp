#include "ui/UiTheme.hpp"

namespace {

const lv_color_t kBackground = lv_color_hex(0x0F1115);
const lv_color_t kSurface = lv_color_hex(0x181C22);
const lv_color_t kSurfaceTint = lv_color_hex(0x242A33);
const lv_color_t kText = lv_color_hex(0xF5EEDF);
const lv_color_t kMutedText = lv_color_hex(0x9CA3AF);
const lv_color_t kAccent = lv_color_hex(0xF07A5A);
const lv_color_t kSoftAccent = lv_color_hex(0xD98C9C);
const lv_color_t kBorder = lv_color_hex(0x343B46);
const lv_color_t kDanger = lv_color_hex(0xD45D66);
const lv_color_t kScrim = lv_color_hex(0x090A0D);

} // namespace

namespace UiTheme {

void init(lv_display_t *display)
{
    lv_theme_t *theme = lv_theme_default_init(display, kAccent, kSoftAccent,
                                               true, &lv_font_montserrat_14);
    lv_display_set_theme(display, theme);
}

void apply_page(lv_obj_t *object)
{
    lv_obj_set_style_bg_color(object, kBackground, 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_radius(object, 0, 0);
    lv_obj_set_style_text_color(object, kText, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void apply_card(lv_obj_t *object)
{
    lv_obj_set_style_bg_color(object, kSurface, 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_border_color(object, kBorder, 0);
    lv_obj_set_style_radius(object, 18, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
    lv_obj_set_style_shadow_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(object, kText, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void apply_button(lv_obj_t *object, bool primary)
{
    lv_obj_set_style_bg_color(object, primary ? kAccent : kSurfaceTint, 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(object, primary ? kSurface : kText, 0);
    lv_obj_set_style_border_width(object, primary ? 0 : 1, 0);
    lv_obj_set_style_border_color(object, kBorder, 0);
    lv_obj_set_style_radius(object, 16, 0);
    lv_obj_set_style_bg_color(object,
                              primary ? lv_color_hex(0xC85F47) : lv_color_hex(0x303844),
                              LV_STATE_PRESSED);
    lv_obj_set_style_opa(object, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void apply_title(lv_obj_t *object)
{
    lv_obj_set_style_text_color(object, kText, 0);
    lv_obj_set_style_text_font(object, &lv_font_montserrat_14, 0);
}

void apply_muted_text(lv_obj_t *object)
{
    lv_obj_set_style_text_color(object, kMutedText, 0);
}

void enable_scroll(lv_obj_t *object, lv_dir_t direction)
{
    lv_obj_add_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_set_scroll_dir(object, direction);
}

lv_color_t background() { return kBackground; }
lv_color_t surface() { return kSurface; }
lv_color_t surface_tint() { return kSurfaceTint; }
lv_color_t text() { return kText; }
lv_color_t muted_text() { return kMutedText; }
lv_color_t accent() { return kAccent; }
lv_color_t soft_accent() { return kSoftAccent; }
lv_color_t border() { return kBorder; }
lv_color_t danger() { return kDanger; }
lv_color_t scrim() { return kScrim; }

} // namespace UiTheme
