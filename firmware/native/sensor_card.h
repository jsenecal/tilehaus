#pragma once
#include "lvgl.h"
#include <cstdlib>
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "sensor_format.h"

namespace tilehaus {

struct SensorCard : Card {
  lv_subject_t text_{};
  char buf_[32];
  char prev_[32];
  lv_obj_t *label_ = nullptr;

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    // Value takes the icon slot (top-left); name sits bottom-left.
    label_ = lv_label_create(cell);
    lv_obj_set_style_text_color(label_, lv_color_hex(0xFFFFFF), 0);
    if (fonts.value) lv_obj_set_style_text_font(label_, fonts.value, 0);
    lv_obj_align(label_, LV_ALIGN_TOP_LEFT, 0, 0);
    add_name(cell, fonts.body, cfg.title, cfg.hide_label);
    buf_[0] = prev_[0] = '\0';
    lv_subject_init_string(&text_, buf_, prev_, sizeof(buf_), "-");
    lv_label_bind_text(label_, &text_, nullptr);
  }

  void bind(const CardConfig &cfg) override {
    // State and unit are two independent subscriptions that land in either
    // order, so keep both and re-render whenever one arrives. Taking the unit
    // from HA is what stops a humidity or air-quality tile reading "°C".
    ha_subscribe(cfg.entity, nullptr, [this](const std::string &s) {
      state_ = s;
      have_state_ = true;
      render();
    });
    ha_subscribe(cfg.entity, "unit_of_measurement", [this](const std::string &s) {
      unit_ = s;
      render();
    });
  }

 private:
  void render() {
    const bool valid = have_state_ && !state_.empty() &&
                       state_ != "unavailable" && state_ != "unknown";
    const std::string text = format_sensor_value(state_, unit_, valid);
    lv_subject_copy_string(&text_, text.c_str());
  }

  std::string state_;
  std::string unit_;
  bool have_state_ = false;
};

}  // namespace tilehaus
