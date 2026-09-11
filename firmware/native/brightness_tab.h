#pragma once
#include "lvgl.h"
#include <cstdint>
#include <cstdlib>
#include <string>
#include "card.h"         // CardFonts
#include "card_style.h"   // style_fill_slider, kTileInset
#include "ha.h"
#include "slider_glide.h"
#include "slider_map.h"

namespace tilehaus {

// Brightness tab: a tall vertical fill slider bound to its own value subject,
// with a centered % readout, gliding to tapped/HA values. Owns its brightness
// subscription.
struct BrightnessTab {
  lv_subject_t value_{};
  lv_subject_t text_{};
  char buf_[8] = {0};
  char prev_[8] = {0};
  lv_obj_t *slider_ = nullptr;
  lv_obj_t *lbl_ = nullptr;
  SliderGlide glide_;
  std::string entity_;

  // Override the slider fill colour (follow-colour lights tint it to the light's
  // own colour, mirroring the tile). No-op before the slider is built.
  void set_fill_color(uint32_t rgb) {
    if (slider_) lv_obj_set_style_bg_color(slider_, lv_color_hex(rgb), LV_PART_INDICATOR);
  }

  static void relabel_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<BrightnessTab *>(lv_observer_get_user_data(o));
    int v = lv_subject_get_int(s);
    std::string t = format_brightness_pct(v);
    lv_subject_copy_string(&self->text_, t.c_str());
  }

  void build(lv_obj_t *panel, const std::string &entity, const CardFonts &fonts,
             int panel_h) {
    entity_ = entity;
    slider_ = lv_slider_create(panel);
    lv_slider_set_orientation(slider_, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(slider_, 0, 100);
    lv_obj_set_size(slider_, 140, panel_h - 2 * kTileInset);
    lv_obj_center(slider_);
    style_fill_slider(slider_, 16);
    glide_.attach(slider_);

    lbl_ = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_, lv_color_hex(0xFFFFFF), 0);
    if (fonts.value) lv_obj_set_style_text_font(lbl_, fonts.value, 0);
    lv_obj_align(lbl_, LV_ALIGN_CENTER, 0, 0);

    lv_subject_init_int(&value_, 0);
    lv_subject_init_string(&text_, buf_, prev_, sizeof(buf_), "0%");
    lv_slider_bind_value(slider_, &value_);
    lv_subject_add_observer(&value_, relabel_cb, this);
    lv_label_bind_text(lbl_, &text_, nullptr);

    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      auto *self = static_cast<BrightnessTab *>(lv_event_get_user_data(e));
      int pct = static_cast<int>(lv_slider_get_value(self->slider_));
      uint8_t bri = slider_to_brightness(pct);
      if (bri == 0) ha_call("light.turn_off", self->entity_);
      else ha_call_kv("light.turn_on", self->entity_, "brightness",
                      std::to_string(static_cast<int>(bri)));
    }, LV_EVENT_RELEASED, this);

    lv_subject_t *subj = &value_;
    ha_subscribe(entity_, "brightness", [subj](const std::string &s) {
      float b = s.empty() ? 0.0f : static_cast<float>(atof(s.c_str()));
      if (b != b) b = 0.0f;  // NaN guard
      lv_subject_set_int(subj, brightness_to_slider(static_cast<uint8_t>(b)));
    });
  }
};

}  // namespace tilehaus
