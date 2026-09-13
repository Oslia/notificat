#pragma once

#include "lvgl.h"

namespace UiTheme {

void init(lv_display_t *display);
void apply_page(lv_obj_t *object);
void apply_card(lv_obj_t *object);
void apply_button(lv_obj_t *object, bool primary = false);
void apply_title(lv_obj_t *object);
void apply_muted_text(lv_obj_t *object);
void enable_scroll(lv_obj_t *object, lv_dir_t direction);

lv_color_t background();
lv_color_t surface();
lv_color_t surface_tint();
lv_color_t text();
lv_color_t muted_text();
lv_color_t accent();
lv_color_t soft_accent();
lv_color_t border();
lv_color_t danger();
lv_color_t scrim();

} // namespace UiTheme
