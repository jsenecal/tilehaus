#pragma once
#include "lvgl.h"
#include <string>
#include "card.h"         // CardFonts
#include "card_style.h"   // style_button_feedback
#include "ha.h"
#include "toggle_state.h" // toggle_color

namespace tilehaus {

// Power tab: a large round toggle. It observes the light's shared on/off subject
// (owned by the tile card) rather than registering its own whole-state
// subscription, and toggles via light.toggle so HA decides the new state.
struct PowerTab {
  lv_obj_t *btn_ = nullptr;
  lv_subject_t *power_ = nullptr;
  std::string entity_;

  static void power_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<PowerTab *>(lv_observer_get_user_data(o));
    bool on = lv_subject_get_int(s) != 0;
    if (self->btn_) {
      if (on) lv_obj_add_state(self->btn_, LV_STATE_CHECKED);
      else lv_obj_remove_state(self->btn_, LV_STATE_CHECKED);
    }
  }

  void build(lv_obj_t *panel, const std::string &entity, const CardFonts &fonts,
             lv_subject_t *power) {
    entity_ = entity;
    power_ = power;
    btn_ = lv_button_create(panel);
    lv_obj_remove_style_all(btn_);
    lv_obj_set_size(btn_, 180, 180);
    lv_obj_center(btn_);
    lv_obj_set_style_radius(btn_, 90, 0);
    style_button_feedback(btn_, toggle_color(false), toggle_color(true));
    lv_obj_t *g = lv_label_create(btn_);
    if (fonts.icon) lv_obj_set_style_text_font(g, fonts.icon, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(g, "\U000F0425");  // mdi-power
    lv_obj_center(g);
    lv_obj_add_event_cb(btn_, [](lv_event_t *e) {
      auto *self = static_cast<PowerTab *>(lv_event_get_user_data(e));
      ha_call("light.toggle", self->entity_);
    }, LV_EVENT_CLICKED, this);
    if (power_) lv_subject_add_observer(power_, power_cb, this);
  }
};

}  // namespace tilehaus
