#pragma once
#include "lvgl.h"
#include <cstdint>
#include <string>
#include "card.h"         // CardFonts
#include "card_style.h"   // kTileInset, style_button_feedback
#include "overlay.h"
#include "toggle_state.h" // toggle_color
#include "power_tab.h"
#include "brightness_tab.h"
#include "temperature_tab.h"
#include "color_tab.h"

namespace tilehaus {

// The all-controls light modal: a left icon tab-rail over four content panels
// (power / brightness / temperature / colour), each a focused component. Hosted
// in a full-screen Overlay and built once (hidden) at boot so every tab's HA
// subscriptions are registered before HA's one-time initial-state push. The
// power tab shares the tile's on/off subject rather than subscribing again.
struct LightModal {
  Overlay overlay_;
  std::string entity_, title_;
  CardFonts fonts_;
  lv_subject_t *power_ = nullptr;  // shared on/off subject (owned by the card)
  lv_obj_t *rail_ = nullptr;
  lv_obj_t *tab_btn_[4] = {nullptr, nullptr, nullptr, nullptr};
  lv_obj_t *panels_[4] = {nullptr, nullptr, nullptr, nullptr};
  // Which tabs apply to this light, decided from supported_color_modes:
  // [0] power, [1] brightness, [2] temperature, [3] colour. A dimmable light
  // drops power (its brightness slider hits 0 = off); an on/off-only light
  // drops brightness/temp/colour and keeps power. All-on until caps arrive.
  bool enabled_[4] = {true, true, true, true};
  int active_ = 0;
  int cw_ = 0, px_ = 0, pw_ = 0;  // content width + panel x/width with the rail
  bool built_ = false;
  PowerTab power_tab_;
  BrightnessTab bright_tab_;
  TemperatureTab temp_tab_;
  ColorTab color_tab_;

  static constexpr int kRailW = 112;

  LightModal(const std::string &entity, const std::string &title,
             const CardFonts &fonts, lv_subject_t *power)
      : entity_(entity), title_(title), fonts_(fonts), power_(power) {}

  int first_enabled() const {
    for (int k = 0; k < 4; ++k)
      if (enabled_[k]) return k;
    return 0;
  }

  void select(int i) {
    if (i < 0 || i >= 4 || !enabled_[i]) i = first_enabled();
    active_ = i;
    for (int k = 0; k < 4; ++k) {
      if (tab_btn_[k]) {
        if (enabled_[k]) lv_obj_clear_flag(tab_btn_[k], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(tab_btn_[k], LV_OBJ_FLAG_HIDDEN);
        if (k == i) lv_obj_add_state(tab_btn_[k], LV_STATE_CHECKED);
        else lv_obj_remove_state(tab_btn_[k], LV_STATE_CHECKED);
      }
      if (panels_[k]) {
        if (enabled_[k] && k == i) lv_obj_clear_flag(panels_[k], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(panels_[k], LV_OBJ_FLAG_HIDDEN);
      }
    }
    // A single applicable tab needs no rail — hand its width to the panel.
    int visible = 0;
    for (int k = 0; k < 4; ++k) visible += enabled_[k] ? 1 : 0;
    bool solo = visible <= 1;
    if (rail_) {
      if (solo) lv_obj_add_flag(rail_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_clear_flag(rail_, LV_OBJ_FLAG_HIDDEN);
    }
    if (panels_[i] && cw_ > 0) {
      if (solo) {
        lv_obj_set_width(panels_[i], cw_ - 2 * kTileInset);
        lv_obj_align(panels_[i], LV_ALIGN_TOP_LEFT, kTileInset, 0);
      } else {
        lv_obj_set_width(panels_[i], pw_);
        lv_obj_align(panels_[i], LV_ALIGN_TOP_LEFT, px_, 0);
      }
    }
  }

  // Decide the tab set from supported_color_modes (a stringified HA list, e.g.
  // "['color_temp', 'xy']"). Any mode other than onoff implies brightness.
  void set_caps(const std::string &modes) {
    auto has = [&](const char *m) { return modes.find(m) != std::string::npos; };
    bool dimmable = has("brightness") || has("color_temp") || has("rgb") ||
                    has("hs") || has("xy") || has("white");
    bool has_temp = has("color_temp");
    bool has_color = has("rgb") || has("hs") || has("xy");
    enabled_[0] = !dimmable;              // power: only when not dimmable
    enabled_[1] = dimmable;               // brightness
    enabled_[2] = dimmable && has_temp;   // temperature
    enabled_[3] = dimmable && has_color;  // colour
    select(active_);
  }

  static void tab_cb(lv_event_t *e) {
    auto *self = static_cast<LightModal *>(lv_event_get_user_data(e));
    lv_obj_t *b = static_cast<lv_obj_t *>(lv_event_get_target(e));
    int idx =
        static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(b)));
    self->select(idx);
  }

  void build() {
    overlay_.build(title_, fonts_.icon, fonts_.body);
    overlay_.on_close_ = [this]() { hide(); };
    lv_obj_t *c = overlay_.content();

    // Deterministic geometry from the overlay's own content sizing (a PCT
    // layout may be unresolved when build() runs inside a click handler).
    int cw = overlay_.content_w();
    int ch = overlay_.content_h();

    rail_ = lv_obj_create(c);
    lv_obj_remove_style_all(rail_);
    lv_obj_set_size(rail_, kRailW, ch);
    lv_obj_align(rail_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(rail_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(rail_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rail_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rail_, 12, 0);
    lv_obj_set_style_pad_row(rail_, 12, 0);

    static const char *kGlyph[4] = {
        "\U000F0425",  // mdi-power
        "\U000F05A8",  // mdi-white-balance-sunny (brightness)
        "\U000F050F",  // mdi-thermometer (temperature)
        "\U000F03D8",  // mdi-palette (color)
    };

    int px = kRailW + kTileInset;
    int pw = cw - px - kTileInset;
    cw_ = cw;
    px_ = px;
    pw_ = pw;

    for (int i = 0; i < 4; ++i) {
      lv_obj_t *b = lv_button_create(rail_);
      lv_obj_remove_style_all(b);
      lv_obj_set_size(b, kRailW - 24, kRailW - 24);
      lv_obj_set_style_radius(b, 16, 0);
      style_button_feedback(b, toggle_color(false), toggle_color(true));
      lv_obj_set_user_data(b, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
      lv_obj_add_event_cb(b, tab_cb, LV_EVENT_CLICKED, this);
      lv_obj_t *g = lv_label_create(b);
      if (fonts_.icon) lv_obj_set_style_text_font(g, fonts_.icon, 0);
      lv_obj_set_style_text_color(g, lv_color_hex(0xFFFFFF), 0);
      lv_label_set_text(g, kGlyph[i]);
      lv_obj_center(g);
      tab_btn_[i] = b;

      lv_obj_t *p = lv_obj_create(c);
      lv_obj_remove_style_all(p);
      lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
      lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_size(p, pw, ch);
      lv_obj_align(p, LV_ALIGN_TOP_LEFT, px, 0);
      panels_[i] = p;
    }

    power_tab_.build(panels_[0], entity_, fonts_, power_);
    bright_tab_.build(panels_[1], entity_, fonts_, ch);
    temp_tab_.build(panels_[2], entity_, fonts_, pw);
    color_tab_.build(panels_[3], entity_);

    // Trim the tab set to the light's real capabilities once HA reports them.
    ha_subscribe(entity_, "supported_color_modes",
                 [this](const std::string &s) { set_caps(s); });

    select(0);
    built_ = true;
  }

  // Tint the brightness slider fill (follow-colour lights track the light's own
  // colour, like the tile).
  void set_brightness_fill(uint32_t rgb) { bright_tab_.set_fill_color(rgb); }

  void show() { if (!built_) build(); overlay_.show(); }
  void hide() { overlay_.hide(); }
};

}  // namespace tilehaus
