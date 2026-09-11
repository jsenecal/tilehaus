#pragma once
#include "lvgl.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"

namespace tilehaus {

// WLED "Levels" tab: Brightness + Speed + Intensity as three labeled horizontal
// sliders in one panel. Each row sends on release and follows its HA source when
// not being dragged (HA pushes ignored while the finger is down). Brightness maps
// to light.turn_on/off (0 = off); Speed/Intensity to number.set_value. Rows are
// generic: a range, an HA (entity, attr) to follow, and a send callback.
struct LevelsTab {
  struct Row {
    lv_obj_t *slider = nullptr;
    lv_obj_t *val = nullptr;      // centered value readout (flashes then fades)
    lv_timer_t *hide = nullptr;   // fades the readout after inactivity
    int touch = 0;                // 1 while the finger is down
    int press_value = 0;          // slider value captured at press (tap replay)
    bool drag_moved = false;      // value changed during the press => real drag
    std::function<void(int)> send;
  };
  static constexpr int kMax = 3;
  Row rows_[kMax];
  int n_ = 0;

  // Fade the readout ~1.5s after the last change (mirrors the dimmer tile).
  static void hide_cb(lv_timer_t *t) {
    auto *r = static_cast<Row *>(lv_timer_get_user_data(t));
    lv_obj_fade_out(r->val, 400, 0);
    lv_timer_pause(t);
  }

  // Caption + slider are added directly to the (tall) panel rather than a tight
  // wrapper: LVGL clips a child to its parent's box, and a wrapper sized to just
  // caption+track would clip the round knob at the track ends and top/bottom.
  // With the panel as parent (and generous panel padding) the knob has room.
  void add_row(lv_obj_t *panel, const CardFonts &fonts, const char *caption,
               int min, int max, const std::string &follow_entity,
               const char *follow_attr, std::function<void(int)> send) {
    Row *row = &rows_[n_++];
    row->send = std::move(send);

    lv_obj_t *cap = lv_label_create(panel);
    if (fonts.body) lv_obj_set_style_text_font(cap, fonts.body, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xB0B0B0), 0);
    lv_label_set_text(cap, caption);
    lv_obj_set_style_pad_top(cap, n_ > 1 ? 16 : 0, 0);  // group gap before row

    lv_obj_t *sl = lv_slider_create(panel);
    row->slider = sl;
    lv_obj_set_width(sl, LV_PCT(100));
    lv_obj_set_height(sl, 64);
    lv_slider_set_range(sl, min, max);
    style_fill_slider(sl, 12);  // tall dimmer fill look (no knob)
    lv_obj_set_style_anim_duration(sl, kSliderAnimMs, 0);  // glide HA/tap changes

    // Centered value readout overlaid on the fill (a label child of the slider,
    // so touches fall through); flashes on change, fades via hide_cb.
    row->val = lv_label_create(sl);
    if (fonts.medium) lv_obj_set_style_text_font(row->val, fonts.medium, 0);
    lv_obj_set_style_text_color(row->val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(row->val);
    lv_obj_set_style_opa(row->val, LV_OPA_TRANSP, 0);
    lv_label_set_text(row->val, "");
    row->hide = lv_timer_create(hide_cb, 1500, row);
    lv_timer_pause(row->hide);

    lv_obj_add_event_cb(sl, [](lv_event_t *e) {
      auto *r = static_cast<Row *>(lv_event_get_user_data(e));
      r->touch = 1;
      r->press_value = static_cast<int>(lv_slider_get_value(r->slider));
      r->drag_moved = false;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSED | LV_EVENT_PREPROCESS), row);
    lv_obj_add_event_cb(sl, [](lv_event_t *e) {
      static_cast<Row *>(lv_event_get_user_data(e))->touch = 1;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSING | LV_EVENT_PREPROCESS), row);
    lv_obj_add_event_cb(sl, [](lv_event_t *e) {  // flash the readout on any change
      auto *r = static_cast<Row *>(lv_event_get_user_data(e));
      if (r->touch == 1) r->drag_moved = true;
      char b[8];
      std::snprintf(b, sizeof(b), "%d",
                    static_cast<int>(lv_slider_get_value(r->slider)));
      lv_label_set_text(r->val, b);
      lv_anim_del(r->val, nullptr);
      lv_obj_set_style_opa(r->val, LV_OPA_COVER, 0);
      lv_timer_reset(r->hide);
      lv_timer_resume(r->hide);
    }, LV_EVENT_VALUE_CHANGED, row);
    lv_obj_add_event_cb(sl, [](lv_event_t *e) {
      auto *r = static_cast<Row *>(lv_event_get_user_data(e));
      if (!r->drag_moved) {  // a tap: replay the jump as an animated glide
        int target = static_cast<int>(lv_slider_get_value(r->slider));
        if (target != r->press_value) {
          lv_slider_set_value(r->slider, r->press_value, LV_ANIM_OFF);
          lv_slider_set_value(r->slider, target, LV_ANIM_ON);
        }
      }
      r->touch = 0;
      if (r->send) r->send(static_cast<int>(lv_slider_get_value(r->slider)));
    }, LV_EVENT_RELEASED, row);

    ha_subscribe(follow_entity, follow_attr, [row](const std::string &s) {
      if (row->touch == 1) return;
      bool bad =
          s.empty() || s == "unknown" || s == "unavailable" || s == "None";
      lv_slider_set_value(row->slider, bad ? 0 : std::atoi(s.c_str()),
                          LV_ANIM_ON);
    });
  }

  void build(lv_obj_t *panel, const CardFonts &fonts, const std::string &light,
             const std::string &speed_entity,
             const std::string &intensity_entity) {
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    // Side margins so the fill sliders don't run to the panel edges; small
    // pad_row (caption hugs its slider), group gap via the caption's pad_top.
    lv_obj_set_style_pad_hor(panel, 28, 0);
    lv_obj_set_style_pad_ver(panel, 24, 0);
    lv_obj_set_style_pad_row(panel, 8, 0);

    add_row(panel, fonts, "Brightness", 0, 255, light, "brightness",
            [light](int v) {
              if (v <= 0) ha_call("light.turn_off", light);
              else ha_call_kv("light.turn_on", light, "brightness",
                              std::to_string(v));
            });
    add_row(panel, fonts, "Speed", 0, 255, speed_entity, nullptr,
            [speed_entity](int v) {
              ha_call_kv("number.set_value", speed_entity, "value",
                         std::to_string(v));
            });
    add_row(panel, fonts, "Intensity", 0, 255, intensity_entity, nullptr,
            [intensity_entity](int v) {
              ha_call_kv("number.set_value", intensity_entity, "value",
                         std::to_string(v));
            });
  }
};

}  // namespace tilehaus
