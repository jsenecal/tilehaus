#pragma once
#include "lvgl.h"
#include <memory>
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "toggle_state.h"
#include "detail_modal.h"

namespace tilehaus {

struct ToggleCard : Card {
  lv_subject_t on_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_;
  std::string icon_on_;   // glyph when on
  std::string icon_off_;  // glyph when off (empty = no swap)
  uint32_t on_color_ = kTileOnBuiltin;
  uint32_t off_color_ = 0x313131;
  int32_t cfg_active_color_ = -1;
  std::unique_ptr<DetailModal> detail_;

  TileSpan default_size() const override { return {2, 2}; }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<ToggleCard *>(lv_observer_get_user_data(o));
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
    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon);
    icon_on_ = cfg.icon;
    icon_off_ = cfg.icon_alt;
    cfg_active_color_ = cfg.active_color;
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    off_color_ = resolve_color(cfg.inactive_color, 0x313131);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
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
      auto *self = static_cast<ToggleCard *>(lv_event_get_user_data(e));
      // Domain-agnostic so a light / fan / switch tile all toggle correctly.
      ha_call("homeassistant.toggle", self->entity_);
    }, LV_EVENT_CLICKED, this);

    // Opt-in "more-info" chevron (entities with a detail modal, e.g. lights).
    if (cfg.detail && has_detail_modal(entity_)) {
      detail_ = make_detail_modal(entity_, title_, fonts_);
      add_detail_chevron(cell_, fonts_.icon, 0, [](lv_event_t *e) {
        auto *self = static_cast<ToggleCard *>(lv_event_get_user_data(e));
        if (self->detail_) self->detail_->open();
      }, this);
    }
  }

  // See PageCard::restyle: repaint directly from current state rather than
  // relying on lv_subject_set_int, which does not re-notify on an unchanged
  // value. The optional detail_ modal (light/cover more-info) is a separate,
  // independently-built widget and is not re-themed here — see the task
  // report for that limitation.
  void restyle() override {
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    const bool on = lv_subject_get_int(&on_) != 0;
    lv_obj_set_style_bg_color(cell_, lv_color_hex(on ? on_color_ : off_color_), 0);
  }
};

}  // namespace tilehaus
