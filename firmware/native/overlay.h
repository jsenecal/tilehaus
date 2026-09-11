#pragma once
#include "lvgl.h"
#include <functional>
#include <string>
#include "card_style.h"    // kTileInset
#include "surface.h"   // app_grid_root

namespace tilehaus {

// Full-screen modal chrome on the top layer: opaque background that renders
// above the grid and swallows touches, a back arrow (top-left) + title, and an
// empty content() region the owner fills. Built once, reused via show()/hide().
//
// Lifecycle: built once and retained for the program's lifetime (the POC
// calls poc_build_cards() a single time at boot and never destroys cards
// at runtime). root_ lives on lv_layer_top(); there is intentionally no
// teardown path — do not destroy an Overlay/LightModal while the app runs,
// or its lv_objs would outlive their C++ owner.
struct Overlay {
  lv_obj_t *root_ = nullptr;
  lv_obj_t *content_ = nullptr;
  int content_w_ = 0;
  int content_h_ = 0;
  std::function<void()> on_close_;

  void build(const std::string &title, const lv_font_t *icon_font,
             const lv_font_t *title_font) {
    root_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);  // swallow grid touches
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *back = lv_button_create(root_);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 56, 56);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, kTileInset, kTileInset);
    lv_obj_t *bl = lv_label_create(back);
    if (icon_font) lv_obj_set_style_text_font(bl, icon_font, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(bl, "\U000F0141");  // mdi-chevron-left
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, [](lv_event_t *e) {
      auto *self = static_cast<Overlay *>(lv_event_get_user_data(e));
      if (self->on_close_) self->on_close_();
    }, LV_EVENT_CLICKED, this);

    lv_obj_t *tt = lv_label_create(root_);
    if (title_font) lv_obj_set_style_text_font(tt, title_font, 0);
    lv_obj_set_style_text_color(tt, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(tt, title.c_str());
    lv_obj_align(tt, LV_ALIGN_TOP_LEFT, kTileInset + 72, kTileInset + 16);

    int w = lv_display_get_horizontal_resolution(lv_display_get_default());
    int h = lv_display_get_vertical_resolution(lv_display_get_default());
    int top = kTileInset + 72;
    content_w_ = w;
    content_h_ = h - top - kTileInset;
    content_ = lv_obj_create(root_);
    lv_obj_remove_style_all(content_);
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(content_, content_w_, content_h_);
    lv_obj_align(content_, LV_ALIGN_TOP_LEFT, 0, top);
  }

  lv_obj_t *content() { return content_; }
  int content_w() const { return content_w_; }
  int content_h() const { return content_h_; }

  // Instant show/hide. The overlay background is opaque, so simply raising it
  // covers the grid and hiding it reveals the grid again — no animation. A
  // full-screen opacity fade here forced LVGL to render the whole 1024x600
  // screen to a layer and alpha-blend it every frame, which dropped the panel to
  // ~8fps during modal transitions. Covered grid cells aren't redrawn, so this
  // is both instant and cheap. (A cheap slide could be added later if desired.)
  void show() {
    if (!root_) return;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root_);
  }

  void hide() {
    if (!root_) return;
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
  }
};

}  // namespace tilehaus
