#pragma once
#include "lvgl.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "card.h"
#include "card_style.h"
#include "clock.h"
#include "ha.h"
#include "tile_scale.h"
#include "weather_glyph.h"
#include "forecast_helpers.h"
#include "esphome/core/time.h"

namespace tilehaus {

// Transparent full-width header: greeting line + subtitle on the left; live
// clock, date, and weather on the right. The greeting is sourced verbatim from
// an HA template sensor (cfg.entity) so its wording — time of day, name, who's
// home, etc. — lives in Home Assistant and changes without reflashing. The
// clock/date stay on-device (1s lv_timer) so they keep ticking offline; weather
// comes from HA subscriptions.
struct HeaderCard : Card {
  lv_obj_t *greet_ = nullptr;
  lv_obj_t *sub_ = nullptr;
  lv_obj_t *clock_ = nullptr;
  lv_obj_t *date_ = nullptr;
  lv_obj_t *wglyph_ = nullptr;
  lv_obj_t *wtemp_ = nullptr;
  lv_obj_t *hilo_ = nullptr;   // today's high/low, when show_hilo is set
  lv_subject_t temp_{};
  char tbuf_[8] = {0};
  char tprev_[8] = {0};
  int hi_ = INT_MIN;
  int lo_ = INT_MIN;

  TileSpan default_size() const override { return {10, 2}; }
  bool wants_chrome() const override { return false; }

  static lv_obj_t *flex_box(lv_obj_t *parent, lv_flex_flow_t flow) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(b, flow);
    return b;
  }

  static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font,
                        const char *initial) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    lv_label_set_text(l, initial);
    return l;
  }

  // Render "low°/high°" once both forecast values have arrived.
  void update_hilo() {
    if (!hilo_) return;
    if (hi_ == INT_MIN || lo_ == INT_MIN) return;
    char b[24];
    std::snprintf(b, sizeof(b), "%d\xC2\xB0/%d\xC2\xB0", lo_, hi_);  // low/high
    lv_label_set_text(hilo_, b);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // Gap between the (grown) greeting/subtitle block and the weather+clock
    // cluster, so the scrolling subtitle never runs into the weather icon.
    lv_obj_set_style_pad_column(cell, 28, 0);

    // Left: greeting line + subtitle, both sourced from HA. Start hidden until
    // the subscription delivers text (or stays hidden if no subtitle entity).
    lv_obj_t *left = flex_box(cell, LV_FLEX_FLOW_COLUMN);
    // Fill the width between the left edge and the right (clock/weather) cluster
    // so the subtitle has a definite width to scroll within.
    lv_obj_set_flex_grow(left, 1);
    greet_ = text(left, fonts.medium, "");
    sub_ = text(left, fonts.body, "");
    lv_obj_set_style_text_color(sub_, lv_color_hex(0xB0B0B0), 0);
    // Marquee the subtitle back and forth (left→right→left) when it overflows
    // the available width; short subtitles just sit still.
    lv_obj_set_width(sub_, lv_pct(100));
    lv_label_set_long_mode(sub_, LV_LABEL_LONG_SCROLL);
    lv_obj_add_flag(sub_, LV_OBJ_FLAG_HIDDEN);

    // Right: weather cluster + time cluster, side by side.
    lv_obj_t *right = flex_box(cell, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 28, 0);

    if (cfg.show_weather) {
      lv_obj_t *wx = flex_box(right, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(wx, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_column(wx, 8, 0);  // small gap between glyph and temp
      wglyph_ = text(wx, fonts.icon, "\U000F0590");  // cloudy placeholder
      // Temperature (and optional high/low beneath it) in their own column so the
      // glyph keeps its distance from the text.
      lv_obj_t *tcol = flex_box(wx, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(tcol, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                            LV_FLEX_ALIGN_START);
      wtemp_ = text(tcol, fonts.medium, "\xE2\x80\x94");
      lv_subject_init_string(&temp_, tbuf_, tprev_, sizeof(tbuf_), "\xE2\x80\x94");
      lv_label_bind_text(wtemp_, &temp_, nullptr);
      if (cfg.show_hilo) {
        hilo_ = text(tcol, fonts.small ? fonts.small : fonts.body, "");
        lv_obj_set_style_text_color(hilo_, lv_color_hex(0xC8C8C8), 0);
      }
    }

    if (cfg.show_clock) {
      // This card's default size is 10x2; dropped into a one-row slot the 55px
      // clock's ~64px line height plus a 22px date overran the 72px tile and
      // sliced the date in half. The clock stays stacked over the date either
      // way — that is what a clock should look like — and both drop a rung when
      // the slot is short: 34px over 15px is ~58px, comfortably inside 72px.
      const bool full =
          header_clock_full_size(tile_metrics().px_h - 2 * kTileInset);
      lv_obj_t *tx = flex_box(right, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(tx, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END,
                            LV_FLEX_ALIGN_END);
      clock_ = text(tx, full ? fonts.value : fonts.medium, "--:--");
      date_ = text(tx, full ? fonts.body
                            : (fonts.small ? fonts.small : fonts.body), "");
      lv_obj_set_style_text_color(date_, lv_color_hex(0xB0B0B0), 0);
      update_clock();
      lv_timer_create(tick_cb, 1000, this);
    }
  }

  void bind(const CardConfig &cfg) override {
    // Greeting line: whatever the HA template sensor renders, verbatim.
    ha_subscribe(cfg.entity, nullptr, [this](const std::string &s) {
      bool valid = !s.empty() && s != "unknown" && s != "unavailable";
      lv_label_set_text(greet_, valid ? s.c_str() : "");
    });
    // Subtitle line: sourced from HA too (cfg.title = subtitle entity). Hidden
    // until it renders; skipped entirely when no subtitle entity is configured.
    if (!cfg.title.empty()) {
      ha_subscribe(cfg.title, nullptr, [this](const std::string &s) {
        bool valid = !s.empty() && s != "unknown" && s != "unavailable";
        lv_label_set_text(sub_, valid ? s.c_str() : "");
        if (valid) lv_obj_clear_flag(sub_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(sub_, LV_OBJ_FLAG_HIDDEN);
      });
    }
    // Weather cluster is driven by entity2 — only when it's shown (its widgets
    // aren't created otherwise, so the callbacks would touch null objects).
    if (!cfg.show_weather) return;
    ha_subscribe(cfg.entity2, nullptr, [this](const std::string &s) {
      set_icon_glyph(wglyph_, weather_glyph(s));
    });
    lv_subject_t *subj = &temp_;
    ha_subscribe(cfg.entity2, "temperature", [subj](const std::string &s) {
      bool valid = !s.empty() && s != "unknown" && s != "unavailable";
      if (!valid) { lv_subject_copy_string(subj, "\xE2\x80\x94"); return; }
      char b[8];
      std::snprintf(b, sizeof(b), "%d\xC2\xB0",
                    static_cast<int>(std::lround(std::atof(s.c_str()))));
      lv_subject_copy_string(subj, b);
    });

    // Optional today's high/low, mirroring the weather tile: the forecast values
    // live in input_number.<name>_temp_high / _low, derived from the weather
    // entity's object id (e.g. weather.forecast_home → …forecast_home_temp_high).
    // Missing helpers simply never push, so the line stays blank.
    if (cfg.show_hilo) {
      // Same resolution the Weather tile uses: derived from the weather entity
      // unless the tile overrides it. This card has no icon of its own, so its
      // icon / icon_alt fields carry the high / low overrides.
      const ForecastHelpers helpers =
          resolve_forecast_helpers(cfg.entity2, cfg.icon, cfg.icon_alt);
      if (!helpers.high.empty())
      ha_subscribe(helpers.high, nullptr,
                   [this](const std::string &s) {
        if (!s.empty() && s != "unknown" && s != "unavailable")
          hi_ = static_cast<int>(std::lround(std::atof(s.c_str())));
        update_hilo();
      });
      if (!helpers.low.empty())
      ha_subscribe(helpers.low, nullptr,
                   [this](const std::string &s) {
        if (!s.empty() && s != "unknown" && s != "unavailable")
          lo_ = static_cast<int>(std::lround(std::atof(s.c_str())));
        update_hilo();
      });
    }
  }

  static void tick_cb(lv_timer_t *t) {
    static_cast<HeaderCard *>(lv_timer_get_user_data(t))->update_clock();
  }

  void update_clock() {
    if (!app_clock()) return;
    esphome::ESPTime now = app_clock()->now();
    if (!now.is_valid()) return;

    char hm[8];
    std::snprintf(hm, sizeof(hm), "%02d:%02d", now.hour, now.minute);
    lv_label_set_text(clock_, hm);

    char ds[24];
    now.strftime(ds, sizeof(ds), "%a %b %e");
    lv_label_set_text(date_, ds);
  }
};

}  // namespace tilehaus
