#pragma once
#include "lvgl.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "card.h"
#include "card_style.h"
#include "overlay.h"
#include "option_list_tab.h"
#include "toggle_state.h"
#include "ha.h"

namespace tilehaus {

// Fan detail modal: a big round power toggle, a percentage speed slider, and the
// fan's preset modes as a pick-list — stacked and centered. Speed/preset are
// inert when the fan doesn't support them (slider at 0 / empty list). Built once
// (hidden) at boot so subscriptions register before HA's initial-state push.
struct FanModal {
  Overlay overlay_;
  std::string entity_, title_;
  CardFonts fonts_;
  bool built_ = false;
  lv_obj_t *power_btn_ = nullptr;
  lv_obj_t *speed_ = nullptr;
  lv_obj_t *speed_lbl_ = nullptr;
  int speed_touch_ = 0;
  OptionListTab presets_;

  FanModal(const std::string &entity, const std::string &title,
           const CardFonts &fonts)
      : entity_(entity), title_(title), fonts_(fonts) {}

  static void set_speed_lbl(lv_obj_t *lbl, int pct) {
    char b[24];
    std::snprintf(b, sizeof(b), "Speed  %d%%", pct);
    lv_label_set_text(lbl, b);
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
    lv_obj_set_style_pad_row(c, 22, 0);

    int col_w = cw * 6 / 10;  // 60%

    // Power toggle (round).
    power_btn_ = lv_button_create(c);
    lv_obj_remove_style_all(power_btn_);
    lv_obj_set_size(power_btn_, 120, 120);
    lv_obj_set_style_radius(power_btn_, 60, 0);
    style_button_feedback(power_btn_, toggle_color(false), toggle_color(true));
    lv_obj_t *pg = lv_label_create(power_btn_);
    if (fonts_.icon) lv_obj_set_style_text_font(pg, fonts_.icon, 0);
    lv_obj_set_style_text_color(pg, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(pg, "\U000F0425");  // mdi-power
    lv_obj_center(pg);
    lv_obj_add_event_cb(power_btn_, [](lv_event_t *e) {
      auto *s = static_cast<FanModal *>(lv_event_get_user_data(e));
      ha_call("fan.toggle", s->entity_);
    }, LV_EVENT_CLICKED, this);

    // Speed slider (percentage).
    lv_obj_t *sbox = lv_obj_create(c);
    lv_obj_remove_style_all(sbox);
    lv_obj_set_style_bg_opa(sbox, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(sbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sbox, col_w, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sbox, 10, 0);

    speed_lbl_ = lv_label_create(sbox);
    if (fonts_.body) lv_obj_set_style_text_font(speed_lbl_, fonts_.body, 0);
    lv_obj_set_style_text_color(speed_lbl_, lv_color_hex(0xB0B0B0), 0);
    set_speed_lbl(speed_lbl_, 0);

    // Reuse the dimmer's tall fill look (horizontal): no protruding knob, so
    // nothing to clip, and it reads like an actual tile.
    speed_ = lv_slider_create(sbox);
    lv_obj_set_width(speed_, LV_PCT(100));
    lv_obj_set_height(speed_, 72);
    lv_slider_set_range(speed_, 0, 100);
    style_fill_slider(speed_, 14);
    lv_obj_add_event_cb(speed_, [](lv_event_t *e) {
      static_cast<FanModal *>(lv_event_get_user_data(e))->speed_touch_ = 1;
    }, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(speed_, [](lv_event_t *e) {
      static_cast<FanModal *>(lv_event_get_user_data(e))->speed_touch_ = 1;
    }, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(speed_, [](lv_event_t *e) {
      auto *s = static_cast<FanModal *>(lv_event_get_user_data(e));
      set_speed_lbl(s->speed_lbl_,
                    static_cast<int>(lv_slider_get_value(s->speed_)));
    }, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(speed_, [](lv_event_t *e) {
      auto *s = static_cast<FanModal *>(lv_event_get_user_data(e));
      s->speed_touch_ = 0;
      ha_call_kv("fan.set_percentage", s->entity_, "percentage",
                 std::to_string(static_cast<int>(lv_slider_get_value(s->speed_))));
    }, LV_EVENT_RELEASED, this);

    // Preset modes.
    lv_obj_t *pbox = lv_obj_create(c);
    lv_obj_remove_style_all(pbox);
    lv_obj_set_style_bg_opa(pbox, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(pbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pbox, col_w, 168);
    presets_.build(pbox, fonts_, entity_, "preset_modes", entity_, "preset_mode",
                   "fan.set_preset_mode", entity_, "preset_mode");

    // Subscriptions.
    ha_subscribe(entity_, nullptr, [this](const std::string &s) {
      if (s == "on") lv_obj_add_state(power_btn_, LV_STATE_CHECKED);
      else lv_obj_remove_state(power_btn_, LV_STATE_CHECKED);
    });
    ha_subscribe(entity_, "percentage", [this](const std::string &s) {
      if (speed_touch_ == 1) return;
      bool bad =
          s.empty() || s == "unknown" || s == "unavailable" || s == "None";
      int v = bad ? 0 : static_cast<int>(std::lround(std::atof(s.c_str())));
      lv_slider_set_value(speed_, v, LV_ANIM_ON);
      set_speed_lbl(speed_lbl_, v);
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
