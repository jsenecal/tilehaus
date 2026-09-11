#pragma once
#include "lvgl.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "card.h"        // CardFonts
#include "ha.h"

namespace tilehaus {

// The thermostat dial: an lv_arc over 15-25C at 0.5-step resolution (integer
// range 30..50 = temp*2). Center shows the hvac action ("Idle"/"Heating") above
// the big target temperature. Owns the temperature + hvac_action subscriptions
// and the climate.set_temperature call (on arc release and on step()). Live
// drags track the finger (HA pushes ignored mid-drag); the target is sent once
// on release, mirroring the light slider's discipline.
struct ClimateArc {
  lv_obj_t *arc_ = nullptr;
  lv_obj_t *action_lbl_ = nullptr;
  lv_obj_t *target_lbl_ = nullptr;
  lv_obj_t *current_lbl_ = nullptr;
  std::string entity_;
  int touch_phase_ = 0;      // 0 idle, 1 dragging

  static int temp_to_arc(float t) {
    return static_cast<int>(std::lround(t * 2.0f));
  }
  static float arc_to_temp(int v) { return v / 2.0f; }

  static void set_target_text(lv_obj_t *lbl, int arc_val) {
    char b[16];
    std::snprintf(b, sizeof(b), "%.1f\xC2\xB0", arc_to_temp(arc_val));
    lv_label_set_text(lbl, b);
  }

  void send_temp(int arc_val) {
    char b[8];
    std::snprintf(b, sizeof(b), "%.1f", arc_to_temp(arc_val));
    ha_call_kv("climate.set_temperature", entity_, "temperature", b);
  }

  // Nudge the target by delta arc units (1 = 0.5C), clamped, and send it.
  void step(int delta) {
    int v = lv_arc_get_value(arc_) + delta;
    if (v < 30) v = 30;
    if (v > 50) v = 50;
    lv_arc_set_value(arc_, v);
    set_target_text(target_lbl_, v);
    send_temp(v);
  }

  static void changed_cb(lv_event_t *e) {
    auto *self = static_cast<ClimateArc *>(lv_event_get_user_data(e));
    set_target_text(self->target_lbl_, lv_arc_get_value(self->arc_));
  }
  static void pressed_cb(lv_event_t *e) {
    static_cast<ClimateArc *>(lv_event_get_user_data(e))->touch_phase_ = 1;
  }
  static void released_cb(lv_event_t *e) {
    auto *self = static_cast<ClimateArc *>(lv_event_get_user_data(e));
    self->touch_phase_ = 0;
    self->send_temp(lv_arc_get_value(self->arc_));
  }

  void build(lv_obj_t *parent, const std::string &entity,
             const CardFonts &fonts, int size) {
    entity_ = entity;
    arc_ = lv_arc_create(parent);
    lv_obj_set_size(arc_, size, size);
    lv_obj_center(arc_);
    lv_arc_set_range(arc_, 30, 50);        // 15.0 - 25.0, 0.5 steps
    lv_arc_set_bg_angles(arc_, 135, 45);   // open-bottom ring, HA-like
    lv_arc_set_value(arc_, 40);
    lv_obj_set_style_arc_width(arc_, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0x3A3A3A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0xB5451B), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_INDICATOR);
    // White circular knob (HA look).
    lv_obj_set_style_bg_color(arc_, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc_, 6, LV_PART_KNOB);
    lv_obj_add_event_cb(arc_, changed_cb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(arc_, pressed_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(arc_, released_cb, LV_EVENT_RELEASED, this);

    action_lbl_ = lv_label_create(parent);
    if (fonts.body) lv_obj_set_style_text_font(action_lbl_, fonts.body, 0);
    lv_obj_set_style_text_color(action_lbl_, lv_color_hex(0xC8C8C8), 0);
    lv_obj_align(action_lbl_, LV_ALIGN_CENTER, 0, -72);
    lv_label_set_text(action_lbl_, "");

    target_lbl_ = lv_label_create(parent);
    if (fonts.value) lv_obj_set_style_text_font(target_lbl_, fonts.value, 0);
    lv_obj_set_style_text_color(target_lbl_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(target_lbl_, LV_ALIGN_CENTER, 0, -6);
    set_target_text(target_lbl_, 40);

    current_lbl_ = lv_label_create(parent);
    if (fonts.body) lv_obj_set_style_text_font(current_lbl_, fonts.body, 0);
    lv_obj_set_style_text_color(current_lbl_, lv_color_hex(0xB0B0B0), 0);
    lv_obj_align(current_lbl_, LV_ALIGN_CENTER, 0, 50);
    lv_label_set_text(current_lbl_, "");
  }

  void bind() {
    ha_subscribe(entity_, "temperature", [this](const std::string &s) {
      if (touch_phase_ == 1) return;  // don't fight the finger mid-drag
      bool valid = !s.empty() && s != "unknown" && s != "unavailable";
      if (!valid) return;
      int v = temp_to_arc(static_cast<float>(std::atof(s.c_str())));
      lv_arc_set_value(arc_, v);
      set_target_text(target_lbl_, v);
    });
    ha_subscribe(entity_, "hvac_action", [this](const std::string &s) {
      const char *t;
      uint32_t col;
      if (s == "heating") { t = "Heating"; col = 0xE1662E; }        // warm
      else if (s == "cooling") { t = "Cooling"; col = 0x3B9BE3; }   // cool
      else if (s == "idle") { t = "Idle"; col = 0xC8C8C8; }         // dim
      else { t = "Off"; col = 0x808080; }                          // dimmer
      lv_label_set_text(action_lbl_, t);
      lv_obj_set_style_text_color(action_lbl_, lv_color_hex(col), 0);
    });
    ha_subscribe(entity_, "current_temperature", [this](const std::string &s) {
      bool valid = !s.empty() && s != "unknown" && s != "unavailable";
      if (!valid) {
        lv_label_set_text(current_lbl_, "");
        return;
      }
      char b[24];
      std::snprintf(b, sizeof(b), "Current: %d\xC2\xB0""C",
                    static_cast<int>(std::lround(std::atof(s.c_str()))));
      lv_label_set_text(current_lbl_, b);
    });
  }
};

}  // namespace tilehaus
