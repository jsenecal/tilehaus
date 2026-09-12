#pragma once
#include "lvgl.h"
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "toggle_state.h"

namespace tilehaus {

// Read-only occupancy tile: icon + name, tinting occupied vs clear. Same look as
// the Toggle tile but not clickable and with no service call.
struct PresenceCard : Card {
  lv_subject_t on_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  std::string icon_on_;   // glyph when occupied
  std::string icon_off_;  // glyph when clear
  uint32_t on_color_ = 0xFF8C00;
  uint32_t off_color_ = 0x313131;

  TileSpan default_size() const override { return {2, 2}; }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<PresenceCard *>(lv_observer_get_user_data(o));
    bool on = lv_subject_get_int(s) != 0;
    lv_obj_set_style_bg_color(
        self->cell_, lv_color_hex(on ? self->on_color_ : self->off_color_), 0);
    set_icon_glyph(self->icon_lbl_, on ? self->icon_on_ : self->icon_off_);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon);
    icon_on_ = cfg.icon;
    icon_off_ = cfg.icon_alt;
    on_color_ = resolve_color(cfg.active_color, 0xFF8C00);
    off_color_ = resolve_color(cfg.inactive_color, 0x313131);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    lv_subject_init_int(&on_, 0);
    lv_subject_add_observer(&on_, recolor_cb, this);
  }

  void bind(const CardConfig &cfg) override {
    lv_subject_t *subj = &on_;
    ha_subscribe(cfg.entity, nullptr, [subj](const std::string &s) {
      lv_subject_set_int(subj, (s == "on") ? 1 : 0);
    });
  }
};

}  // namespace tilehaus
