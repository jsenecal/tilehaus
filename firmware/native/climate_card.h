#pragma once
#include "lvgl.h"
#include <memory>
#include <string>
#include "card.h"
#include "card_style.h"
#include "climate_modal.h"
#include "ha.h"

namespace tilehaus {

// Climate tile: icon + name, tinted warm while actively heating. Owns the
// ClimateModal (built hidden at boot so its subscriptions catch HA's initial
// push) and opens it on tap. Tile warmth follows hvac_action.
struct ClimateCard : Card {
  lv_subject_t heating_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_;
  uint32_t warm_color_ = 0xC24E1E;
  uint32_t idle_color_ = 0x313131;
  std::unique_ptr<ClimateModal> modal_;

  TileSpan default_size() const override { return {2, 2}; }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<ClimateCard *>(lv_observer_get_user_data(o));
    bool heating = lv_subject_get_int(s) != 0;
    lv_obj_set_style_bg_color(
        self->cell_,
        lv_color_hex(heating ? self->warm_color_ : self->idle_color_), 0);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon);
    add_name(cell, fonts.body, cfg.title, cfg.hide_label);
    warm_color_ = resolve_color(cfg.active_color, 0xC24E1E);
    idle_color_ = resolve_color(cfg.inactive_color, 0x313131);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    lv_subject_init_int(&heating_, 0);
    lv_subject_add_observer(&heating_, recolor_cb, this);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    lv_subject_t *subj = &heating_;
    ha_subscribe(cfg.entity, "hvac_action", [subj](const std::string &s) {
      lv_subject_set_int(subj, (s == "heating") ? 1 : 0);
    });
    modal_.reset(new ClimateModal(entity_, title_, fonts_));
    modal_->build();
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<ClimateCard *>(lv_event_get_user_data(e));
      if (self->modal_) self->modal_->show();
    }, LV_EVENT_CLICKED, this);
  }
};

}  // namespace tilehaus
