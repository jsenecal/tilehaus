#pragma once
#include "lvgl.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "ha_hold.h"
#include "slider_glide.h"

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
    SliderGlide glide;            // tap glides, drag tracks the finger 1:1
    std::function<void(int)> send;
    HaHold hold;  // no HA pushes onto the slider mid-drag or while it settles
    Row() = default;
    Row(const Row &) = delete;
    ~Row() {
      if (hide) lv_timer_delete(hide);
    }
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
    lv_obj_set_style_text_color(cap, lv_color_hex(kTileInkDim), 0);
    lv_label_set_text(cap, caption);
    lv_obj_set_style_pad_top(cap, n_ > 1 ? 16 : 0, 0);  // group gap before row

    lv_obj_t *sl = lv_slider_create(panel);
    row->slider = sl;
    lv_obj_set_width(sl, LV_PCT(100));
    lv_obj_set_height(sl, 64);
    lv_slider_set_range(sl, min, max);
    style_fill_slider(sl);  // tall dimmer fill look (no knob)
    // Attached before the callbacks below, so its tap replay has run by the
    // time the RELEASED handler reads the value to send.
    row->glide.attach(sl);

    // Centered value readout overlaid on the fill (a label child of the slider,
    // so touches fall through); flashes on change, fades via hide_cb.
    row->val = lv_label_create(sl);
    if (fonts.medium) lv_obj_set_style_text_font(row->val, fonts.medium, 0);
    lv_obj_set_style_text_color(row->val, lv_color_hex(kTileInk), 0);
    lv_obj_center(row->val);
    lv_obj_set_style_opa(row->val, LV_OPA_TRANSP, 0);
    lv_label_set_text(row->val, "");
    row->hide = lv_timer_create(hide_cb, 1500, row);
    lv_timer_pause(row->hide);

    lv_obj_add_event_cb(sl, [](lv_event_t *e) {  // flash the readout on any change
      auto *r = static_cast<Row *>(lv_event_get_user_data(e));
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
      if (r->send) r->send(static_cast<int>(lv_slider_get_value(r->slider)));
    }, LV_EVENT_RELEASED, row);

    row->hold.attach(row->slider);
    ha_subscribe(follow_entity, follow_attr, [row](const std::string &s) {
      bool bad =
          s.empty() || s == "unknown" || s == "unavailable" || s == "None";
      const int v = bad ? 0 : std::atoi(s.c_str());
      row->hold.gate([row, v]() { lv_slider_set_value(row->slider, v, LV_ANIM_ON); });
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
