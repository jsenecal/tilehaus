#pragma once
#include "lvgl.h"
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"

namespace tilehaus {

// Momentary action tile shared by Scene and Button: icon + name, press-dip and a
// one-shot activation flash on tap, then a fire-and-forget service call. No
// persistent state (scenes/buttons are stateless), so no subscription.
struct ActionCard : Card {
  lv_obj_t *cell_ = nullptr;
  std::string entity_;
  const char *service_;

  explicit ActionCard(const char *service) : service_(service) {}

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    add_icon(cell, fonts.icon, cfg.icon);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    // inactive_color = resting bg, active_color = the momentary tap flash.
    style_action_feedback(cell, resolve_color(cfg.inactive_color, 0x313131),
                          resolve_color(cfg.active_color, tile_on_color()));
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<ActionCard *>(lv_event_get_user_data(e));
      ha_call(self->service_, self->entity_);
      self->flash();
    }, LV_EVENT_CLICKED, this);
  }

  void flash() {
    lv_obj_add_state(cell_, LV_STATE_USER_1);
    // One-shot: the timer deletes itself on its first (and only) fire.
    lv_timer_create([](lv_timer_t *tm) {
      auto *c = static_cast<lv_obj_t *>(lv_timer_get_user_data(tm));
      lv_obj_remove_state(c, LV_STATE_USER_1);  // eases back to rest color
      lv_timer_delete(tm);
    }, 180, cell_);
  }
};

}  // namespace tilehaus
