#pragma once
#include "lvgl.h"
#include <memory>
#include <string>
#include "card.h"
#include "card_style.h"
#include "fan_modal.h"
#include "ha.h"

namespace tilehaus {

// Fan tile: icon + name, tinted while running. Tap toggles on/off; the
// bottom-right chevron opens the fan detail modal (speed + preset modes).
struct FanCard : Card {
  lv_subject_t on_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_, icon_on_, icon_off_;
  uint32_t on_color_ = 0x2C6E8A;  // cool teal while running
  uint32_t off_color_ = 0x313131;
  std::unique_ptr<FanModal> modal_;

  TileSpan default_size() const override { return {2, 2}; }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<FanCard *>(lv_observer_get_user_data(o));
    bool on = lv_subject_get_int(s) != 0;
    lv_obj_set_style_bg_color(
        self->cell_, lv_color_hex(on ? self->on_color_ : self->off_color_), 0);
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
    on_color_ = resolve_color(cfg.active_color, 0x2C6E8A);
    off_color_ = resolve_color(cfg.inactive_color, 0x313131);
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
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<FanCard *>(lv_event_get_user_data(e));
      ha_call("fan.toggle", self->entity_);
    }, LV_EVENT_CLICKED, this);
    modal_.reset(new FanModal(entity_, title_, fonts_));
    modal_->build();
    add_detail_chevron(cell_, fonts_.icon, 0, [](lv_event_t *e) {
      auto *self = static_cast<FanCard *>(lv_event_get_user_data(e));
      if (self->modal_) self->modal_->show();
    }, this);
  }
};

}  // namespace tilehaus
