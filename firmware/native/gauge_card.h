#pragma once
#include "lvgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "card.h"
#include "card_style.h"
#include "gauge_state.h"
#include "ha.h"
#include "sensor_format.h"
#include "theme.h"

namespace tilehaus {

// A numeric sensor as a 240° dial (type 26): battery, humidity, CO2, power. The
// dial fills with the tile's accent, or red once the value crosses the warning
// threshold. The range and threshold come from the Range field ("0..100 <20"),
// or from the sensor's unit when it is blank (gauge_state.h, host-tested).
//
// Same two layouts as the Timer. A roomy tile (3x3 and up) puts a mono caption
// top-left — the range, or LOW / HIGH while warning — with the dial in the
// middle and the name bottom-left. A small tile has the dial at the top and the
// name under it.
struct GaugeCard : Card {
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *arc_ = nullptr;
  lv_obj_t *value_lbl_ = nullptr;
  lv_obj_t *cap_lbl_ = nullptr;  // roomy layout only
  CardFonts fonts_{};
  int ring_ = 0;
  std::string range_spec_, state_, unit_;
  bool have_state_ = false;
  int32_t cfg_active_color_ = -1;
  std::string shown_;

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg, const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    cfg_active_color_ = cfg.active_color;
    range_spec_ = cfg.entity2;

    const TileMetrics m = tile_metrics();
    const int content_w = m.px_w > 0 ? m.px_w - 2 * kTileInset : 120;
    const int content_h = m.px_h > 0 ? m.px_h - 2 * kTileInset : 120;
    const bool named = !cfg.hide_label && !cfg.title.empty();
    const bool roomy = content_w >= 180 && content_h >= 180;
    constexpr int kCaptionH = 22, kNameH = 34, kGap = 8;
    ring_ = roomy ? std::min(content_w, content_h - kCaptionH - kNameH - 2 * kGap)
                  : std::min(content_w, content_h - (named ? 30 : 0));
    ring_ = std::max(48, ring_);

    arc_ = lv_arc_create(cell);
    lv_obj_set_size(arc_, ring_, ring_);
    // The dial is open at the bottom, so its visual centre sits a little below
    // the box's: nudge it down by a twelfth to look centred in its band.
    if (roomy) lv_obj_align(arc_, LV_ALIGN_CENTER, 0, (kCaptionH - kNameH) / 2 + ring_ / 12);
    else lv_obj_align(arc_, LV_ALIGN_TOP_MID, 0, ring_ / 12);
    lv_arc_set_bg_angles(arc_, 150, 30);  // 240°, open at the bottom
    lv_arc_set_range(arc_, 0, 1000);
    lv_arc_set_value(arc_, 0);
    lv_obj_remove_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    const int stroke = std::max(6, ring_ / 9);
    lv_obj_set_style_arc_width(arc_, stroke, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, stroke, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(kBlack), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc_, LV_OPA_30, LV_PART_MAIN);

    value_lbl_ = lv_label_create(arc_);
    lv_obj_set_style_text_color(value_lbl_, lv_color_hex(kTileInk), 0);
    lv_obj_center(value_lbl_);

    if (roomy) {
      cap_lbl_ = lv_label_create(cell);
      if (fonts.caption) lv_obj_set_style_text_font(cap_lbl_, fonts.caption, 0);
      lv_obj_set_style_text_letter_space(cap_lbl_, 2, 0);
      lv_obj_align(cap_lbl_, LV_ALIGN_TOP_LEFT, 0, 0);
    }
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    // After the labels exist, so a bright custom fill gives them dark ink.
    paint_cell(cell, resolve_color(cfg.inactive_color, kTileBg));
    render();
  }

  void bind(const CardConfig &cfg) override {
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

  void restyle() override { render(); }

 private:
  static std::string num(float v) {
    char b[16];
    std::snprintf(b, sizeof(b), "%g", static_cast<double>(v));
    return b;
  }

  void render() {
    if (cell_ == nullptr) return;
    const bool valid = have_state_ && !state_.empty() && state_ != "unavailable" &&
                       state_ != "unknown";
    char *end = nullptr;
    const float v = valid ? std::strtof(state_.c_str(), &end) : NAN;
    const bool numeric = valid && end != state_.c_str();
    const float value = numeric ? v : NAN;

    const GaugeScale scale = parse_gauge_scale(range_spec_, unit_);
    const bool warn = scale.warns(value);
    const uint32_t fill = warn ? kStateAlert : resolve_color(cfg_active_color_, tile_on_color());
    lv_obj_set_style_arc_color(arc_, lv_color_hex(fill), LV_PART_INDICATOR);
    lv_arc_set_value(arc_, scale.permille(value));

    if (cap_lbl_) {
      std::string cap;
      if (warn) cap = scale.warn == GaugeScale::Warn::Below ? "LOW" : "HIGH";
      else cap = num(scale.min) + "–" + num(scale.max) + (unit_.empty() ? "" : " " + unit_);
      ascii_upper(cap.data());
      lv_obj_set_style_text_color(cap_lbl_, lv_color_hex(warn ? kStateAlert : kTileInkDim), 0);
      lv_label_set_text(cap_lbl_, cap.c_str());
    }

    const std::string text = format_sensor_value(state_, unit_, valid);
    if (text != shown_) {
      shown_ = text;
      // The largest face that fits inside the dial; a step down for long text.
      const lv_font_t *steps[] = {fonts_.value, fonts_.medium, fonts_.body, fonts_.small};
      int i = ring_ >= 200 ? 0 : ring_ >= 120 ? 1 : 2;
      if (text.size() > 5) ++i;
      if (steps[i] != nullptr) lv_obj_set_style_text_font(value_lbl_, steps[i], 0);
      lv_label_set_text(value_lbl_, text.c_str());
    }
  }
};

}  // namespace tilehaus
