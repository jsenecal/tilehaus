#pragma once
#include "lvgl.h"
#include <cstdlib>
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "fit_number.h"
#include "sensor_format.h"

namespace tilehaus {

struct SensorCard : Card {
  FitNumber value_;  // the reading, number as large as the tile allows, unit small

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    // The value fills the tile above the name line (all of it without a name).
    const TileMetrics m = tile_metrics();
    const int content_w = m.px_w > 0 ? m.px_w - 2 * kTileInset : 120;
    const int content_h = m.px_h > 0 ? m.px_h - 2 * kTileInset : 120;
    const bool named = !cfg.hide_label && !cfg.title.empty();
    value_.create(cell, content_w, content_h - (named ? 34 : 0), fonts, false);
    lv_obj_align(value_.box, LV_ALIGN_TOP_LEFT, 0, 0);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    value_.set("-", "", false);
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
    const SensorParts p = split_sensor_value(state_, unit_, valid);
    value_.set(p.value, p.unit, p.numeric);
  }

  std::string state_;
  std::string unit_;
  bool have_state_ = false;
};

}  // namespace tilehaus
