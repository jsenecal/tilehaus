#pragma once
#include "lvgl.h"
#include <string>
#include "esphome/core/hal.h"
#include "card.h"
#include "card_style.h"
#include "confirm_dialog.h"
#include "garage_state.h"
#include "ha.h"
#include "theme.h"

namespace tilehaus {

// A garage door or gate (type 25): a `cover` with one big action. Tap asks to
// open or close it; while it moves, a tap stops it. A mono caption reads the
// state ("OPEN · 14 MIN", "CLOSING"); once it has been open kOverdueMin the
// tile tints amber.
//
// Unlike the Cover card's shade slider, a garage has no useful in-between
// position, and opening one by accident matters, so every open and close goes
// through a confirm step.
struct GarageCard : Card {
  static constexpr int kOverdueMin = 10;
  static constexpr const char *kGarageShut = "\U000F06D9";  // mdi-garage
  static constexpr const char *kGarageOpen = "\U000F06DA";  // mdi-garage-open
  static constexpr const char *kGateShut = "\U000F0299";    // mdi-gate
  static constexpr const char *kGateOpen = "\U000F116A";    // mdi-gate-open

  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  lv_obj_t *cap_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_;
  std::string icon_shut_, icon_open_;
  bool custom_icon_ = false;
  uint32_t warn_color_ = kStateWarn;
  uint32_t off_color_ = kTileBg;
  int32_t cfg_active_color_ = -1;
  DoorTracker door_;

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg, const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    custom_icon_ = !cfg.icon.empty();
    icon_shut_ = custom_icon_ ? cfg.icon : kGarageShut;
    icon_open_ = !cfg.icon_alt.empty() ? cfg.icon_alt : (custom_icon_ ? cfg.icon : kGarageOpen);
    cfg_active_color_ = cfg.active_color;
    warn_color_ = resolve_color(cfg_active_color_, kStateWarn);
    off_color_ = resolve_color(cfg.inactive_color, kTileBg);

    icon_lbl_ = add_icon(cell, fonts.icon, icon_shut_);
    cap_lbl_ = lv_label_create(cell);
    if (fonts.caption) lv_obj_set_style_text_font(cap_lbl_, fonts.caption, 0);
    lv_obj_set_style_text_letter_space(cap_lbl_, 2, 0);
    lv_obj_align(cap_lbl_, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(cap_lbl_, "");
    add_name(cell, fonts, cfg.title, cfg.hide_label);

    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    render();
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    ha_subscribe(entity_, nullptr, [this](const std::string &s) {
      door_.on_state(s, static_cast<int64_t>(esphome::millis()));
      render();
    });
    // A gate gets gate icons, unless the tile chose its own.
    ha_subscribe(entity_, "device_class", [this](const std::string &s) {
      if (custom_icon_) return;
      const bool gate = s == "gate";
      icon_shut_ = gate ? kGateShut : kGarageShut;
      icon_open_ = gate ? kGateOpen : kGarageOpen;
      render();
    });

    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      static_cast<GarageCard *>(lv_event_get_user_data(e))->on_tap();
    }, LV_EVENT_CLICKED, this);

    // The minutes in the caption, and the overdue tint, advance on their own.
    lv_timer_create([](lv_timer_t *t) {
      auto *self = static_cast<GarageCard *>(lv_timer_get_user_data(t));
      if (self->door_.phase == DoorPhase::Open) self->render();
    }, 30000, this);
  }

  void restyle() override {
    warn_color_ = resolve_color(cfg_active_color_, kStateWarn);
    render();
  }

 private:
  void on_tap() {
    const std::string entity = entity_;
    const std::string what = title_.empty() ? "the door" : title_;
    switch (door_.phase) {
      case DoorPhase::Closed:
        ConfirmDialog::show("Open " + what + "?", fonts_, "Open",
                            [entity]() { ha_call("cover.open_cover", entity); });
        break;
      case DoorPhase::Open:
        ConfirmDialog::show("Close " + what + "?", fonts_, "Close",
                            [entity]() { ha_call("cover.close_cover", entity); });
        break;
      case DoorPhase::Opening:
      case DoorPhase::Closing:
        ha_call("cover.stop_cover", entity);  // stopping is always safe
        break;
      case DoorPhase::Unknown:
        break;
    }
  }

  void render() {
    if (cell_ == nullptr) return;
    const int64_t now = static_cast<int64_t>(esphome::millis());
    const bool overdue = door_.overdue(now, kOverdueMin);
    paint_state_cell(cell_, overdue, warn_color_, off_color_);

    const bool open_ish = door_.phase == DoorPhase::Open || door_.phase == DoorPhase::Opening;
    set_icon_glyph(icon_lbl_, open_ish ? icon_open_ : icon_shut_);

    // Caption ink: dark or white on the amber fill; on charcoal, amber while
    // open (the warning before the tint) and dim otherwise.
    uint32_t ink = kTileInkDim;
    if (overdue) ink = needs_dark_ink(warn_color_) ? kTileBg : kTileInk;
    else if (door_.phase == DoorPhase::Open) ink = warn_color_;
    lv_obj_set_style_text_color(cap_lbl_, lv_color_hex(ink), 0);
    lv_label_set_text(cap_lbl_, door_.caption(now).c_str());
  }
};

}  // namespace tilehaus
