#pragma once
#include "lvgl.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "card.h"
#include "card_style.h"   // kTileInset, style_button_feedback
#include "overlay.h"
#include "toggle_state.h"  // toggle_color

namespace tilehaus {

// Generalized tabbed modal: a left icon rail over N full-height content panels,
// one per registered tab. Factored from the light modal's 4-tab mechanics but
// tab-count-agnostic — rail buttons size to fit the panel height (and the rail
// scrolls if they still overflow). Each tab registers a glyph + a builder
// closure that fills its panel. Built once (hidden) at boot so every tab's HA
// subscriptions register before HA's one-time initial-state push; shown on tap.
struct TabbedModal {
  Overlay overlay_;
  std::string title_;
  CardFonts fonts_;
  std::vector<std::string> glyphs_;
  // Builder receives its panel plus the panel's intended pixel size (pw, ph).
  // Passing the size explicitly matters: the panel's computed geometry is not yet
  // available (lv_obj_get_height == 0) when tabs build, since the modal is built
  // hidden at boot before any layout pass.
  std::vector<std::function<void(lv_obj_t *, int, int)>> builders_;
  std::vector<lv_obj_t *> tab_btn_;
  std::vector<lv_obj_t *> panels_;
  int active_ = 0;
  bool built_ = false;

  static constexpr int kRailW = 112;

  void add_tab(const std::string &glyph,
               std::function<void(lv_obj_t *, int, int)> builder) {
    glyphs_.push_back(glyph);
    builders_.push_back(std::move(builder));
  }

  void select(int i) {
    active_ = i;
    for (size_t k = 0; k < tab_btn_.size(); ++k) {
      bool on = static_cast<int>(k) == i;
      if (on) lv_obj_add_state(tab_btn_[k], LV_STATE_CHECKED);
      else lv_obj_remove_state(tab_btn_[k], LV_STATE_CHECKED);
      if (on) lv_obj_clear_flag(panels_[k], LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(panels_[k], LV_OBJ_FLAG_HIDDEN);
    }
  }

  static void tab_cb(lv_event_t *e) {
    auto *self = static_cast<TabbedModal *>(lv_event_get_user_data(e));
    lv_obj_t *b = lv_event_get_target_obj(e);
    int idx =
        static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(b)));
    self->select(idx);
  }

  // Build the rail + panels after all add_tab() calls.
  void build_chrome() {
    overlay_.build(title_, fonts_.icon, fonts_.body);
    overlay_.on_close_ = [this]() { hide(); };
    lv_obj_t *c = overlay_.content();
    int cw = overlay_.content_w();
    int ch = overlay_.content_h();
    int n = static_cast<int>(glyphs_.size());

    const int pad = 12, gap = 12;
    int bs = kRailW - 24;
    if (n > 0) {
      int fit = (ch - 2 * pad - (n - 1) * gap) / n;
      bs = std::min(bs, fit);
      if (bs < 40) bs = 40;  // floor; the rail scrolls if they overflow
    }

    lv_obj_t *rail = lv_obj_create(c);
    lv_obj_remove_style_all(rail);
    lv_obj_set_size(rail, kRailW, ch);
    lv_obj_align(rail, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rail, pad, 0);
    lv_obj_set_style_pad_row(rail, gap, 0);
    lv_obj_set_scroll_dir(rail, LV_DIR_VER);

    int px = kRailW + kTileInset;
    int pw = cw - px - kTileInset;

    for (int i = 0; i < n; ++i) {
      lv_obj_t *b = lv_button_create(rail);
      lv_obj_remove_style_all(b);
      lv_obj_set_size(b, bs, bs);
      lv_obj_set_style_radius(b, 16, 0);
      style_button_feedback(b, toggle_color(false), toggle_color(true));
      lv_obj_set_user_data(b,
                           reinterpret_cast<void *>(static_cast<intptr_t>(i)));
      lv_obj_add_event_cb(b, tab_cb, LV_EVENT_CLICKED, this);
      lv_obj_t *g = lv_label_create(b);
      if (fonts_.icon) lv_obj_set_style_text_font(g, fonts_.icon, 0);
      lv_obj_set_style_text_color(g, lv_color_hex(0xFFFFFF), 0);
      lv_label_set_text(g, glyphs_[i].c_str());
      lv_obj_center(g);
      tab_btn_.push_back(b);

      lv_obj_t *p = lv_obj_create(c);
      lv_obj_remove_style_all(p);
      lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
      lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_size(p, pw, ch);
      lv_obj_align(p, LV_ALIGN_TOP_LEFT, px, 0);
      panels_.push_back(p);
      builders_[i](p, pw, ch);  // pass the intended size; p's geometry is 0 here
    }
    select(0);
    built_ = true;
  }

  void show() {
    if (built_) overlay_.show();
  }
  void hide() { overlay_.hide(); }
};

}  // namespace tilehaus
