#pragma once
#include "lvgl.h"
#include <memory>
#include <string>
#include "card.h"
#include "card_style.h"
#include "wled_modal.h"
#include "ha.h"

namespace tilehaus {

// WLED tile: icon + name, tinted while the strip is on. Owns the WledModal
// (built hidden at boot) and opens it on tap.
struct WledCard : Card {
  lv_subject_t on_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_, icon_on_, icon_off_;
  uint32_t active_color_ = 0x5A3E8A;  // WLED-ish violet while on
  uint32_t idle_color_ = 0x313131;
  std::unique_ptr<WledModal> modal_;

  TileSpan default_size() const override { return {2, 2}; }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<WledCard *>(lv_observer_get_user_data(o));
    bool on = lv_subject_get_int(s) != 0;
    lv_obj_set_style_bg_color(
        self->cell_, lv_color_hex(on ? self->active_color_ : self->idle_color_),
        0);
    set_icon_glyph(self->icon_lbl_, on ? self->icon_on_ : self->icon_off_);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    icon_on_ = cfg.icon;
    icon_off_ = cfg.icon_alt.empty() ? cfg.icon : cfg.icon_alt;
    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    active_color_ = resolve_color(cfg.active_color, 0x5A3E8A);
    idle_color_ = resolve_color(cfg.inactive_color, 0x313131);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    lv_subject_init_int(&on_, 0);
    lv_subject_add_observer(&on_, recolor_cb, this);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    lv_subject_t *subj = &on_;
    ha_subscribe(cfg.entity, nullptr, [subj](const std::string &s) {
      lv_subject_set_int(subj, (s == "on") ? 1 : 0);
    });
    modal_.reset(new WledModal(entity_, title_, fonts_, &on_));
    modal_->build();
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<WledCard *>(lv_event_get_user_data(e));
      if (self->modal_) self->modal_->show();
    }, LV_EVENT_CLICKED, this);
  }
};

}  // namespace tilehaus
