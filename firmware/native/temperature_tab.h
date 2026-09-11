#pragma once
#include "lvgl.h"
#include <cstdlib>
#include <string>
#include "card.h"         // CardFonts
#include "card_style.h"   // kTileInset
#include "kelvin_map.h"
#include "ha.h"
#include "slider_glide.h"

namespace tilehaus {

// Temperature tab: a warm->cool gradient slider whose value is Kelvin over the
// bulb's advertised range, with a centered Kelvin readout. Owns its colour-temp
// and min/max range subscriptions.
struct TemperatureTab {
  lv_subject_t text_{};
  char buf_[12] = {0};
  char prev_[12] = {0};
  lv_obj_t *slider_ = nullptr;
  lv_obj_t *lbl_ = nullptr;
  SliderGlide glide_;
  int min_k_ = 2202;
  int max_k_ = 4000;
  std::string entity_;

  void relabel() {
    if (!slider_) return;
    int k = static_cast<int>(lv_slider_get_value(slider_));
    std::string t = format_kelvin(k);
    lv_subject_copy_string(&text_, t.c_str());
  }

  void build(lv_obj_t *panel, const std::string &entity, const CardFonts &fonts,
             int panel_w) {
    entity_ = entity;
    int sw = panel_w - 6 * kTileInset;  // wide margins so the knob clears the rail
    slider_ = lv_slider_create(panel);
    lv_slider_set_range(slider_, min_k_, max_k_);
    lv_obj_set_size(slider_, sw, 56);
    lv_obj_align(slider_, LV_ALIGN_CENTER, 0, 0);
    // Track: warm -> cool horizontal gradient; indicator hidden; visible knob.
    lv_obj_set_style_radius(slider_, 28, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_, lv_color_hex(0xFF8C2A), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(slider_, lv_color_hex(0xCFE0FF), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(slider_, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_opa(slider_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider_, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(slider_, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_, 6, LV_PART_KNOB);
    glide_.attach(slider_);

    lbl_ = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_, lv_color_hex(0xFFFFFF), 0);
    if (fonts.value) lv_obj_set_style_text_font(lbl_, fonts.value, 0);
    // Fixed width + centered text so the readout stays centered over the slider
    // as its content changes.
    lv_obj_set_width(lbl_, sw);
    lv_obj_set_style_text_align(lbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(lbl_, slider_, LV_ALIGN_OUT_TOP_MID, 0, -16);

    lv_subject_init_string(&text_, buf_, prev_, sizeof(buf_), "");
    lv_label_bind_text(lbl_, &text_, nullptr);
    lv_slider_set_value(slider_, (min_k_ + max_k_) / 2, LV_ANIM_OFF);
    relabel();

    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      static_cast<TemperatureTab *>(lv_event_get_user_data(e))->relabel();
    }, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      auto *self = static_cast<TemperatureTab *>(lv_event_get_user_data(e));
      int k = static_cast<int>(lv_slider_get_value(self->slider_));
      ha_call_kv("light.turn_on", self->entity_, "color_temp_kelvin",
                 std::to_string(k));
    }, LV_EVENT_RELEASED, this);

    ha_subscribe(entity_, "color_temp_kelvin", [this](const std::string &s) {
      if (s.empty() || s == "None" || s == "unknown") return;
      int k = atoi(s.c_str());
      if (k <= 0 || !slider_) return;
      lv_slider_set_value(slider_, k, LV_ANIM_ON);
      relabel();
    });
    ha_subscribe(entity_, "min_color_temp_kelvin", [this](const std::string &s) {
      int k = atoi(s.c_str());
      if (k > 0) { min_k_ = k;
        if (slider_) lv_slider_set_range(slider_, min_k_, max_k_); }
    });
    ha_subscribe(entity_, "max_color_temp_kelvin", [this](const std::string &s) {
      int k = atoi(s.c_str());
      if (k > 0) { max_k_ = k;
        if (slider_) lv_slider_set_range(slider_, min_k_, max_k_); }
    });
  }
};

}  // namespace tilehaus
