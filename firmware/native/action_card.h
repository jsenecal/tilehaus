#pragma once
#include "lvgl.h"
#include <string>
#include "esphome/core/application.h"
#include "action_target.h"
#include "card.h"
#include "card_style.h"
#include "ha.h"

namespace tilehaus {

// Momentary action tile: icon + name, press-dip and a one-shot activation flash
// on tap, then a fire-and-forget call.
//
// Scene (8) and Button (9) pass their fixed service. Action (23) passes none and
// resolves one from the entity's domain (action_target.h); with no entity it
// fires kActionEvent carrying its Event name, for an HA automation to catch. A
// script target also tints the tile while the script runs.
struct ActionCard : Card {
  lv_obj_t *cell_ = nullptr;
  std::string entity_;
  std::string event_name_;  // Action with no entity: the event's "action" value
  const char *service_;     // null = resolve from the entity (Action)
  int32_t cfg_active_color_ = -1;
  uint32_t rest_color_ = kTileBg;
  bool running_ = false;

  explicit ActionCard(const char *service = nullptr) : service_(service) {}

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    cfg_active_color_ = cfg.active_color;
    add_icon(cell, fonts.icon, cfg.icon);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    // inactive_color = resting bg, active_color = the momentary tap flash.
    rest_color_ = resolve_color(cfg.inactive_color, kTileBg);
    style_action_feedback(cell, rest_color_,
                          resolve_color(cfg_active_color_, tile_on_color()));
  }

  // At rest the tile only tints while a script runs; the LV_STATE_USER_1
  // flash colour is updated for the next tap.
  void restyle() override {
    const uint32_t on = resolve_color(cfg_active_color_, tile_on_color());
    lv_obj_set_style_bg_color(cell_, lv_color_hex(on), LV_PART_MAIN | LV_STATE_USER_1);
    paint_state_cell(cell_, running_, on, rest_color_);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    event_name_ = !cfg.entity2.empty() ? cfg.entity2 : cfg.title;
    if (service_ == nullptr && action_tracks_running(entity_)) {
      ha_subscribe(entity_, nullptr, [this](const std::string &s) {
        running_ = (s == "on");
        restyle();
      });
    }
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<ActionCard *>(lv_event_get_user_data(e));
      self->fire();
      self->flash();
    }, LV_EVENT_CLICKED, this);
  }

  void fire() {
    const char *service = service_ != nullptr ? service_ : action_service(entity_);
    if (service != nullptr) {
      ha_call(service, entity_);
      return;
    }
    ha_fire_event(kActionEvent, {{"action", event_name_},
                                 {"panel", esphome::App.get_name()}});
  }

  void flash() {
    lv_obj_add_state(cell_, LV_STATE_USER_1);
    // One timer per tile, kept paused between taps (owned, so a live rebuild
    // cannot leave it pointing at a deleted cell).
    if (flash_timer_ == nullptr) {
      flash_timer_ = own_timer(lv_timer_create([](lv_timer_t *tm) {
        auto *self = static_cast<ActionCard *>(lv_timer_get_user_data(tm));
        lv_obj_remove_state(self->cell_, LV_STATE_USER_1);  // eases back to rest color
        lv_timer_pause(tm);
      }, 180, this));
    }
    lv_timer_reset(flash_timer_);
    lv_timer_resume(flash_timer_);
  }

  lv_timer_t *flash_timer_ = nullptr;
};

}  // namespace tilehaus
