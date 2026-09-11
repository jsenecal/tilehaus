#pragma once
#include "lvgl.h"
#include <cstdint>
#include <functional>
#include <string>
#include "card.h"
#include "card_style.h"
#include "overlay.h"

namespace tilehaus {

// A numeric PIN pad on its own overlay: a masked display over a 3x4 grid
// (1-9, Del, 0, OK). OK calls on_submit_ with the typed code; Del backspaces.
// Reusable and HA-agnostic — the owner sets on_submit_ and calls show()/hide().
// Stacks above another modal (both live on the top layer).
struct PinPad {
  Overlay overlay_;
  CardFonts fonts_;
  std::function<void(const std::string &)> on_submit_;
  std::string code_;
  lv_obj_t *disp_ = nullptr;
  bool built_ = false;

  void refresh() {
    std::string masked(code_.size(), '*');
    lv_label_set_text(disp_, masked.empty() ? " " : masked.c_str());
  }

  void key(char c) {
    if (c == 'D') {
      if (!code_.empty()) code_.pop_back();
    } else if (c == 'K') {
      if (on_submit_) on_submit_(code_);
      return;
    } else if (code_.size() < 12) {
      code_.push_back(c);
    }
    refresh();
  }

  void make_key(lv_obj_t *grid, const char *label, char code,
                const lv_font_t *font) {
    lv_obj_t *b = lv_button_create(grid);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 80, 80);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    style_press_dip(b);
    lv_obj_set_user_data(b,
                         reinterpret_cast<void *>(static_cast<intptr_t>(code)));
    lv_obj_add_event_cb(b, [](lv_event_t *e) {
      auto *self = static_cast<PinPad *>(lv_event_get_user_data(e));
      lv_obj_t *bt = lv_event_get_target_obj(e);
      char c = static_cast<char>(reinterpret_cast<intptr_t>(
          lv_obj_get_user_data(bt)));
      self->key(c);
    }, LV_EVENT_CLICKED, this);
    lv_obj_t *l = lv_label_create(b);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(l, label);
    lv_obj_center(l);
  }

  void build(const std::string &title, const CardFonts &fonts) {
    fonts_ = fonts;
    overlay_.build(title, fonts.icon, fonts.body);
    overlay_.on_close_ = [this]() { hide(); };
    lv_obj_t *c = overlay_.content();
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(c, kTileInset, 0);
    lv_obj_set_style_pad_row(c, 16, 0);

    disp_ = lv_label_create(c);
    if (fonts.value) lv_obj_set_style_text_font(disp_, fonts.value, 0);
    lv_obj_set_style_text_color(disp_, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(disp_, " ");

    lv_obj_t *grid = lv_obj_create(c);
    lv_obj_remove_style_all(grid);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(grid, 3 * 80 + 2 * 10, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_style_pad_column(grid, 10, 0);

    const lv_font_t *df = fonts.medium ? fonts.medium : fonts.body;
    make_key(grid, "1", '1', df);
    make_key(grid, "2", '2', df);
    make_key(grid, "3", '3', df);
    make_key(grid, "4", '4', df);
    make_key(grid, "5", '5', df);
    make_key(grid, "6", '6', df);
    make_key(grid, "7", '7', df);
    make_key(grid, "8", '8', df);
    make_key(grid, "9", '9', df);
    make_key(grid, "Del", 'D', fonts.body);
    make_key(grid, "0", '0', df);
    make_key(grid, "OK", 'K', fonts.body);

    built_ = true;
  }

  void show() {
    if (!built_) return;
    code_.clear();
    refresh();
    overlay_.show();
  }
  void hide() { overlay_.hide(); }
};

}  // namespace tilehaus
