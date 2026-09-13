#pragma once
#include "lvgl.h"
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "toggle_state.h"
#include "page_nav.h"

namespace tilehaus {

struct PageCard : Card {
  lv_subject_t on_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  std::string target_, icon_on_, icon_off_;
  uint32_t on_color_ = kTileOnBuiltin;
  uint32_t off_color_ = 0x313131;

  TileSpan default_size() const override { return {2, 2}; }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<PageCard *>(lv_observer_get_user_data(o));
    const bool on = lv_subject_get_int(s) != 0;
    lv_obj_set_style_bg_color(self->cell_, lv_color_hex(on ? self->on_color_ : self->off_color_), 0);
    set_icon_glyph(self->icon_lbl_, on ? self->icon_on_ : self->icon_off_);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg, const CardFonts &fonts) override {
    cell_ = cell;
    const std::string glyph = cfg.icon.empty() ? std::string("\U000F024B") : cfg.icon;  // mdi-folder default
    icon_lbl_ = add_icon(cell, fonts.icon, glyph);
    icon_on_ = glyph;
    icon_off_ = cfg.icon_alt.empty() ? glyph : cfg.icon_alt;
    on_color_ = resolve_color(cfg.active_color, tile_on_color());
    off_color_ = resolve_color(cfg.inactive_color, 0x313131);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    lv_subject_init_int(&on_, 0);
    lv_subject_add_observer(&on_, recolor_cb, this);
  }

  void bind(const CardConfig &cfg) override {
    target_ = cfg.entity2;
    if (!cfg.entity.empty()) {
      lv_subject_t *subj = &on_;
      ha_subscribe(cfg.entity, nullptr, [subj](const std::string &s) { lv_subject_set_int(subj, (s == "on") ? 1 : 0); });
    }
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<PageCard *>(lv_event_get_user_data(e));
      page_nav().go(self->target_);
    }, LV_EVENT_CLICKED, this);
  }
};

}  // namespace tilehaus
