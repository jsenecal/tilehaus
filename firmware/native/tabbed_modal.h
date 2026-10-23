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
  // Rail actions (add_action): a tap callback instead of a page, and a subject
  // whose non-zero value lights the button. Empty / null for ordinary tabs.
  std::vector<std::function<void()>> actions_;
  std::vector<lv_subject_t *> lit_;
  std::vector<lv_obj_t *> tab_btn_;
  std::vector<lv_obj_t *> panels_;
  int active_ = 0;
  bool built_ = false;

  static constexpr int kRailW = 112;
  static constexpr int kRailPad = 12, kRailGap = 12;

  // The pieces of a tabbed modal, shared with LightModal (which keeps its own
  // capability-driven tab logic but builds the same rail).

  // The left icon rail: a column of buttons centred down its height.
  static lv_obj_t *make_rail(lv_obj_t *content, int h, bool scrollable) {
    lv_obj_t *rail = lv_obj_create(content);
    lv_obj_remove_style_all(rail);
    lv_obj_set_size(rail, kRailW, h);
    lv_obj_align(rail, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rail, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rail, kRailPad, 0);
    lv_obj_set_style_pad_row(rail, kRailGap, 0);
    if (scrollable) lv_obj_set_scroll_dir(rail, LV_DIR_VER);
    else lv_obj_clear_flag(rail, LV_OBJ_FLAG_SCROLLABLE);
    return rail;
  }

  // One square rail button: a glyph, the tile on/off colours, and `cb` on tap
  // with the button's index in its user data.
  static lv_obj_t *make_rail_button(lv_obj_t *rail, int size, const char *glyph,
                                    const lv_font_t *icon_font, int idx,
                                    lv_event_cb_t cb, void *user_data) {
    lv_obj_t *b = lv_button_create(rail);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, size, size);
    style_button_feedback(b, toggle_color(false), toggle_color(true));
    lv_obj_set_user_data(b, reinterpret_cast<void *>(static_cast<intptr_t>(idx)));
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *g = lv_label_create(b);
    if (icon_font) lv_obj_set_style_text_font(g, icon_font, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(kTileInk), 0);
    lv_label_set_text(g, glyph);
    lv_obj_center(g);
    return b;
  }

  // A rail action is set apart from the tabs below it.
  static void set_apart(lv_obj_t *b) { lv_obj_set_style_margin_bottom(b, kRailGap, 0); }

  // A tab's content panel, right of the rail.
  static lv_obj_t *make_tab_panel(lv_obj_t *content, int x, int w, int h) {
    lv_obj_t *p = lv_obj_create(content);
    lv_obj_remove_style_all(p);
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(p, w, h);
    lv_obj_align(p, LV_ALIGN_TOP_LEFT, x, 0);
    return p;
  }

  void add_tab(const std::string &glyph,
               std::function<void(lv_obj_t *, int, int)> builder) {
    glyphs_.push_back(glyph);
    builders_.push_back(std::move(builder));
    actions_.emplace_back();
    lit_.push_back(nullptr);
  }

  // A rail button that acts rather than opening a page — a light's power
  // button, as in Home Assistant's light dialog. Lit while `lit` is non-zero.
  // Set apart from the tabs below it; add actions before tabs.
  void add_action(const std::string &glyph, std::function<void()> tap,
                  lv_subject_t *lit) {
    glyphs_.push_back(glyph);
    builders_.emplace_back();
    actions_.push_back(std::move(tap));
    lit_.push_back(lit);
  }

  bool is_action(size_t k) const { return k < actions_.size() && actions_[k]; }

  int first_tab() const {
    for (size_t k = 0; k < glyphs_.size(); ++k)
      if (!is_action(k)) return static_cast<int>(k);
    return 0;
  }

  static void lit_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *b = static_cast<lv_obj_t *>(lv_observer_get_user_data(o));
    if (lv_subject_get_int(s) != 0) lv_obj_add_state(b, LV_STATE_CHECKED);
    else lv_obj_remove_state(b, LV_STATE_CHECKED);
  }

  void select(int i) {
    if (i < 0 || i >= static_cast<int>(tab_btn_.size()) || is_action(i))
      i = first_tab();
    active_ = i;
    for (size_t k = 0; k < tab_btn_.size(); ++k) {
      if (is_action(k)) continue;  // lit by its subject, and has no page
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
    if (self->is_action(idx)) {
      self->actions_[idx]();  // the subject repaints it when state comes back
      return;
    }
    self->select(idx);
  }

  // Build the rail + panels after all add_tab() calls.
  void build_chrome() {
    overlay_.build(title_, fonts_);
    overlay_.on_close_ = [this]() { hide(); };
    lv_obj_t *c = overlay_.content();
    int cw = overlay_.content_w();
    int ch = overlay_.content_h();
    int n = static_cast<int>(glyphs_.size());

    int bs = kRailW - 2 * kRailPad;
    if (n > 0) {
      int fit = (ch - 2 * kRailPad - (n - 1) * kRailGap) / n;
      bs = std::min(bs, fit);
      if (bs < 40) bs = 40;  // floor; the rail scrolls if they overflow
    }

    lv_obj_t *rail = make_rail(c, ch, true);  // scrolls if buttons overflow

    int px = kRailW + kTileInset;
    int pw = cw - px - kTileInset;

    for (int i = 0; i < n; ++i) {
      lv_obj_t *b = make_rail_button(rail, bs, glyphs_[i].c_str(), fonts_.icon,
                                     i, tab_cb, this);
      tab_btn_.push_back(b);

      if (is_action(i)) {
        set_apart(b);
        if (lit_[i]) lv_subject_add_observer_obj(lit_[i], lit_cb, b, b);
        panels_.push_back(nullptr);
        continue;
      }

      lv_obj_t *p = make_tab_panel(c, px, pw, ch);
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
