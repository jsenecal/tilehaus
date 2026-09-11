#pragma once
#include "lvgl.h"
#include <cstdint>
#include <string>
#include <vector>
#include "card.h"
#include "card_style.h"
#include "overlay.h"
#include "pin_keypad.h"
#include "ha.h"

namespace tilehaus {

// Alarm detail modal: a column of big action buttons (Disarm / Arm Home / Away /
// Night). Each calls its alarm_control_panel service; the button matching the
// current state is highlighted. No code entry — this panel doesn't require one.
// Built once (hidden) at boot so the state subscription registers early.
struct AlarmModal {
  Overlay overlay_;
  std::string entity_, title_;
  CardFonts fonts_;
  bool built_ = false;
  struct Btn {
    lv_obj_t *obj;
    lv_obj_t *lbl;
    std::string match;    // state that highlights this button
    std::string service;  // service to call on tap
    std::string action;   // imperative label ("Arm Away") when not current
    std::string status;   // status label ("Armed Away") when this is the state
    bool needs_code;      // route through the PIN pad instead of calling direct
  };
  std::vector<Btn> btns_;
  std::string state_;
  PinPad pin_;
  std::string pending_service_;  // service to run after the PIN pad submits

  AlarmModal(const std::string &entity, const std::string &title,
             const CardFonts &fonts)
      : entity_(entity), title_(title), fonts_(fonts) {}

  static void tap_cb(lv_event_t *e) {
    auto *self = static_cast<AlarmModal *>(lv_event_get_user_data(e));
    lv_obj_t *b = lv_event_get_target_obj(e);
    int i = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(b)));
    if (i < 0 || i >= static_cast<int>(self->btns_.size())) return;
    auto &bt = self->btns_[i];
    if (bt.needs_code) {
      self->pending_service_ = bt.service;
      self->pin_.show();  // disarm requires a code -> keypad
    } else {
      ha_call(bt.service.c_str(), self->entity_);
    }
  }

  void add_btn(lv_obj_t *parent, const char *action, const char *status,
               const char *service, const char *match, uint32_t on_color, int w,
               bool needs_code) {
    int idx = static_cast<int>(btns_.size());
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, 72);
    lv_obj_set_style_radius(b, 14, 0);
    style_button_feedback(b, 0x2A2A2A, on_color);
    lv_obj_set_user_data(b, reinterpret_cast<void *>(static_cast<intptr_t>(idx)));
    lv_obj_add_event_cb(b, tap_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *l = lv_label_create(b);
    if (fonts_.body) lv_obj_set_style_text_font(l, fonts_.body, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(l, action);
    lv_obj_center(l);
    btns_.push_back({b, l, match, service, action, status, needs_code});
  }

  void highlight() {
    for (auto &bt : btns_) {
      bool active = bt.match == state_;
      if (active) lv_obj_add_state(bt.obj, LV_STATE_CHECKED);
      else lv_obj_remove_state(bt.obj, LV_STATE_CHECKED);
      lv_label_set_text(bt.lbl, (active ? bt.status : bt.action).c_str());
    }
  }

  void build() {
    overlay_.build(title_, fonts_.icon, fonts_.body);
    overlay_.on_close_ = [this]() { hide(); };
    lv_obj_t *c = overlay_.content();
    int cw = overlay_.content_w();
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(c, kTileInset, 0);
    lv_obj_set_style_pad_row(c, 16, 0);

    int w = cw * 5 / 10;  // 50%
    add_btn(c, "Disarm", "Disarmed", "alarm_control_panel.alarm_disarm",
            "disarmed", 0x2E7D32, w, true);
    add_btn(c, "Arm Home", "Armed Home", "alarm_control_panel.alarm_arm_home",
            "armed_home", 0xC24E1E, w, false);
    add_btn(c, "Arm Away", "Armed Away", "alarm_control_panel.alarm_arm_away",
            "armed_away", 0xC24E1E, w, false);
    add_btn(c, "Arm Night", "Armed Night", "alarm_control_panel.alarm_arm_night",
            "armed_night", 0xC24E1E, w, false);

    // PIN pad for disarm: submit runs the pending service with the typed code.
    pin_.build("Enter Code", fonts_);
    pin_.on_submit_ = [this](const std::string &code) {
      ha_call_kv(pending_service_.c_str(), entity_, "code", code);
      pin_.hide();
    };

    ha_subscribe(entity_, nullptr, [this](const std::string &s) {
      state_ = s;
      highlight();
    });
    built_ = true;
  }

  void show() {
    if (!built_) build();
    overlay_.show();
  }
  void hide() { overlay_.hide(); }
};

}  // namespace tilehaus
