#pragma once
#include "lvgl.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "weather_glyph.h"
#include "forecast_helpers.h"

namespace tilehaus {

// Read-only weather tile: an icon that follows the condition, the condition
// name as the tile label, today's High/Low just above that label, and the
// current temperature small in the top-right corner. Subscribes to the weather
// entity (state = condition, temperature attribute) plus the two forecast
// input_numbers: high via entity2, low via icon_alt (or, when icon_alt is
// blank, a "…_low" id derived from the high helper's name).
struct WeatherCard : Card {
  lv_subject_t temp_{};
  char tbuf_[8] = {0};
  char tprev_[8] = {0};
  lv_obj_t *icon_lbl_ = nullptr;
  lv_obj_t *temp_lbl_ = nullptr;   // current temperature (corner)
  lv_obj_t *cond_lbl_ = nullptr;   // condition name (label)
  lv_obj_t *hilo_lbl_ = nullptr;   // today's high / low
  int hi_ = INT_MIN;
  int lo_ = INT_MIN;

  TileSpan default_size() const override { return {2, 2}; }

  // Round a numeric state string to a whole-degree string ("20°"); em dash when
  // the value is invalid.
  static std::string deg(const std::string &s) {
    bool valid = !s.empty() && s != "unknown" && s != "unavailable";
    if (!valid) return "\xE2\x80\x94";
    char b[8];
    std::snprintf(b, sizeof(b), "%d\xC2\xB0",
                  static_cast<int>(std::lround(std::atof(s.c_str()))));
    return b;
  }

  void update_hilo() {
    if (!hilo_lbl_) return;
    if (hi_ == INT_MIN || lo_ == INT_MIN) return;  // wait for both
    char b[24];
    std::snprintf(b, sizeof(b), "%d\xC2\xB0/%d\xC2\xB0", lo_, hi_);  // low/high
    lv_label_set_text(hilo_lbl_, b);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    icon_lbl_ = add_icon(cell, fonts.icon,
                         cfg.icon.empty() ? "\U000F0590" : cfg.icon);

    temp_lbl_ = lv_label_create(cell);
    lv_obj_set_style_text_color(temp_lbl_, lv_color_hex(0xFFFFFF), 0);
    const lv_font_t *temp_font = fonts.medium ? fonts.medium : fonts.body;
    if (temp_font) lv_obj_set_style_text_font(temp_lbl_, temp_font, 0);
    lv_obj_align(temp_lbl_, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_subject_init_string(&temp_, tbuf_, tprev_, sizeof(tbuf_), "\xE2\x80\x94");
    lv_label_bind_text(temp_lbl_, &temp_, nullptr);

    // Condition name as the tile label (bottom), High/Low just above it.
    cond_lbl_ = lv_label_create(cell);
    lv_obj_set_style_text_color(cond_lbl_, lv_color_hex(0xFFFFFF), 0);
    if (fonts.body) lv_obj_set_style_text_font(cond_lbl_, fonts.body, 0);
    lv_label_set_long_mode(cond_lbl_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(cond_lbl_, LV_PCT(100));
    lv_label_set_text(cond_lbl_, "");
    lv_obj_align(cond_lbl_, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // High/Low sits directly under the current temp, smaller and dimmer.
    hilo_lbl_ = lv_label_create(cell);
    lv_obj_set_style_text_color(hilo_lbl_, lv_color_hex(0xC8C8C8), 0);
    if (fonts.body) lv_obj_set_style_text_font(hilo_lbl_, fonts.body, 0);
    lv_label_set_text(hilo_lbl_, "");
    lv_obj_align(hilo_lbl_, LV_ALIGN_TOP_RIGHT, 0, 42);
  }

  void bind(const CardConfig &cfg) override {
    ha_subscribe(cfg.entity, nullptr, [this](const std::string &s) {
      set_icon_glyph(icon_lbl_, weather_glyph(s));
      lv_label_set_text(cond_lbl_, weather_label(s));
    });
    lv_subject_t *subj = &temp_;
    ha_subscribe(cfg.entity, "temperature", [subj](const std::string &s) {
      lv_subject_copy_string(subj, WeatherCard::deg(s).c_str());
    });

    // Forecast high/low, resolved exactly as the Header does: derived from this
    // tile's own weather entity unless overridden. entity2 carries the high
    // override (icon is taken — it is this tile's condition glyph) and icon_alt
    // the low. A deck that names its helpers by convention configures neither.
    const ForecastHelpers helpers =
        resolve_forecast_helpers(cfg.entity, cfg.entity2, cfg.icon_alt);
    if (helpers.high.empty()) return;
    ha_subscribe(helpers.high, nullptr, [this](const std::string &s) {
      if (!s.empty() && s != "unknown" && s != "unavailable")
        hi_ = static_cast<int>(std::lround(std::atof(s.c_str())));
      update_hilo();
    });
    if (!helpers.low.empty()) {
      ha_subscribe(helpers.low, nullptr, [this](const std::string &s) {
        if (!s.empty() && s != "unknown" && s != "unavailable")
          lo_ = static_cast<int>(std::lround(std::atof(s.c_str())));
        update_hilo();
      });
    }
  }
};

}  // namespace tilehaus
