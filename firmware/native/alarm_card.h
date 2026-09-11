#pragma once
#include "lvgl.h"
#include <cctype>
#include <memory>
#include <string>
#include "card.h"
#include "card_style.h"
#include "alarm_modal.h"
#include "ha.h"

namespace tilehaus {

// Alarm tile: shield icon + name + the current state (e.g. "Armed Away"), tinted
// red while armed/arming/triggered and neutral while disarmed. Tap opens the
// arm/disarm button modal.
struct AlarmCard : Card {
  lv_subject_t armed_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  lv_obj_t *state_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_, icon_on_, icon_off_;
  uint32_t on_color_ = 0xB23B3B;  // red while armed
  uint32_t off_color_ = 0x313131;
  std::unique_ptr<AlarmModal> modal_;

  TileSpan default_size() const override { return {2, 2}; }

  static std::string title_case(const std::string &s) {
    std::string out;
    bool up = true;
    for (char ch : s) {
      if (ch == '_') { out.push_back(' '); up = true; }
      else if (up) { out.push_back(static_cast<char>(std::toupper(
                         static_cast<unsigned char>(ch)))); up = false; }
      else out.push_back(ch);
    }
    return out;
  }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<AlarmCard *>(lv_observer_get_user_data(o));
    bool armed = lv_subject_get_int(s) != 0;
    lv_obj_set_style_bg_color(
        self->cell_, lv_color_hex(armed ? self->on_color_ : self->off_color_),
        0);
    set_icon_glyph(self->icon_lbl_, armed ? self->icon_on_ : self->icon_off_);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    icon_on_ = cfg.icon;
    icon_off_ = cfg.icon_alt.empty() ? cfg.icon : cfg.icon_alt;
    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon);
    add_name(cell, fonts.body, cfg.title, cfg.hide_label);
    on_color_ = resolve_color(cfg.active_color, 0xB23B3B);
    off_color_ = resolve_color(cfg.inactive_color, 0x313131);

    state_lbl_ = lv_label_create(cell);
    if (fonts.body) lv_obj_set_style_text_font(state_lbl_, fonts.body, 0);
    lv_obj_set_style_text_color(state_lbl_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(state_lbl_);
    lv_label_set_text(state_lbl_, "");

    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    lv_subject_init_int(&armed_, 0);
    lv_subject_add_observer(&armed_, recolor_cb, this);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    ha_subscribe(cfg.entity, nullptr, [this](const std::string &s) {
      bool bad = s.empty() || s == "unknown" || s == "unavailable";
      bool armed = !bad && s != "disarmed";
      lv_subject_set_int(&armed_, armed ? 1 : 0);
      lv_label_set_text(state_lbl_, bad ? "" : title_case(s).c_str());
    });
    modal_.reset(new AlarmModal(entity_, title_, fonts_));
    modal_->build();
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<AlarmCard *>(lv_event_get_user_data(e));
      if (self->modal_) self->modal_->show();
    }, LV_EVENT_CLICKED, this);
  }
};

}  // namespace tilehaus
