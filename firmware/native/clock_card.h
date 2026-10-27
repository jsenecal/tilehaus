#pragma once
#include "lvgl.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include "esphome/components/time/posix_tz.h"
#include "card.h"
#include "card_style.h"
#include "clock.h"
#include "fit_number.h"
#include "theme.h"
#include "timer_state.h"  // days_from_civil

namespace tilehaus {

// A clock tile (type 27): the panel's own time, or a city's for a world clock.
// Entity 2 holds the city's POSIX TZ rule (the configurator picks it from a
// list); blank is the panel's own zone. A world clock is converted with
// ESPHome's own parser, without touching the system time zone the screensaver
// and HA sync rely on.
//
// Caption top-left in mono: the date ("FRI 24 OCT") for local time, or the day
// there and its offset from here ("FRI · +13 H") for a world clock. The time
// sits large in the middle and the name bottom-left; a world clock adds a sun
// or moon bottom-right for day or night there.
struct ClockCard : Card {
  static constexpr const char *kSun = "\U000F05A8";   // mdi-white-balance-sunny
  static constexpr const char *kMoon = "\U000F0594";  // mdi-weather-night

  lv_obj_t *cell_ = nullptr;
  FitNumber time_;
  lv_obj_t *cap_lbl_ = nullptr;
  lv_obj_t *glyph_lbl_ = nullptr;
  esphome::time::ParsedTimezone tz_{};
  bool world_ = false;
  std::string shown_cap_, shown_glyph_;

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg, const CardFonts &fonts) override {
    cell_ = cell;
    world_ = !cfg.entity2.empty() && esphome::time::parse_posix_tz(cfg.entity2.c_str(), tz_);

    cap_lbl_ = lv_label_create(cell);
    if (fonts.caption) lv_obj_set_style_text_font(cap_lbl_, fonts.caption, 0);
    lv_obj_set_style_text_letter_space(cap_lbl_, 2, 0);
    lv_obj_set_style_text_color(cap_lbl_, lv_color_hex(kTileInkDim), 0);
    lv_obj_align(cap_lbl_, LV_ALIGN_TOP_LEFT, 0, 0);

    // The time fills the band between the caption and the name, as large as
    // it fits (fit_number.h).
    const TileMetrics m = tile_metrics();
    const int content_w = m.px_w > 0 ? m.px_w - 2 * kTileInset : 120;
    const int content_h = m.px_h > 0 ? m.px_h - 2 * kTileInset : 120;
    const bool named = !cfg.hide_label && !cfg.title.empty();
    constexpr int kCaptionH = 22, kNameH = 34, kGap = 6;
    const int band_h = content_h - kCaptionH - (named ? kNameH : 0) - 2 * kGap;
    time_.create(cell, content_w, band_h, fonts, false);
    lv_obj_align(time_.box, LV_ALIGN_TOP_LEFT, 0, kCaptionH + kGap);

    add_name(cell, fonts, cfg.title, cfg.hide_label);
    if (world_) {
      glyph_lbl_ = lv_label_create(cell);
      if (fonts.icon) lv_obj_set_style_text_font(glyph_lbl_, fonts.icon, 0);
      lv_obj_set_style_text_color(glyph_lbl_, lv_color_hex(kTileInkDim), 0);
      lv_obj_set_style_transform_pivot_x(glyph_lbl_, lv_pct(100), 0);
      lv_obj_set_style_transform_pivot_y(glyph_lbl_, lv_pct(100), 0);
      lv_obj_set_style_transform_scale(glyph_lbl_, 180, 0);  // ~70%: an accent, not an icon
      lv_obj_align(glyph_lbl_, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }
    paint_cell(cell, resolve_color(cfg.inactive_color, kTileBg));
    render();
  }

  void bind(const CardConfig &) override {
    own_timer(lv_timer_create([](lv_timer_t *t) {
      static_cast<ClockCard *>(lv_timer_get_user_data(t))->render();
    }, 1000, this));
  }

 private:
  static int64_t seconds_of(const struct tm &t) {
    return days_from_civil(t.tm_year + 1900, static_cast<unsigned>(t.tm_mon + 1),
                           static_cast<unsigned>(t.tm_mday)) * 86400 +
           t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  }

  // "+13 H", "-4 H", "+5:30", "SAME TIME"
  static std::string offset_text(int64_t diff_s) {
    const int64_t mins = (diff_s >= 0 ? diff_s + 30 : diff_s - 30) / 60;
    if (mins == 0) return "SAME TIME";
    const char sign = mins > 0 ? '+' : '-';
    const int64_t a = mins > 0 ? mins : -mins;
    char b[16];
    if (a % 60 == 0) std::snprintf(b, sizeof(b), "%c%lld H", sign, static_cast<long long>(a / 60));
    else std::snprintf(b, sizeof(b), "%c%lld:%02lld", sign, static_cast<long long>(a / 60),
                       static_cast<long long>(a % 60));
    return b;
  }

  void set_text(lv_obj_t *lbl, std::string &shown, const std::string &text) {
    if (lbl == nullptr || text == shown) return;
    shown = text;
    lv_label_set_text(lbl, text.c_str());
  }

  void render() {
    if (cell_ == nullptr) return;
    static const char *const kDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char *const kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    // Before the clock syncs: a faint dash placeholder.
    const bool synced = app_clock() != nullptr && app_clock()->now().is_valid();
    lv_obj_set_style_text_opa(time_.num, synced ? LV_OPA_COVER : LV_OPA_30, 0);
    if (!synced) {
      time_.set("--:--", "", true);
      return;
    }
    const esphome::ESPTime now = app_clock()->now();
    struct tm here {};
    here.tm_year = now.year - 1900;
    here.tm_mon = now.month - 1;
    here.tm_mday = now.day_of_month;
    here.tm_hour = now.hour;
    here.tm_min = now.minute;
    here.tm_sec = now.second;
    here.tm_wday = now.day_of_week - 1;  // ESPTime: 1 = Sunday

    struct tm t = here;
    if (world_ && !esphome::time::epoch_to_local_tm(now.timestamp, tz_, &t)) t = here;

    char hm[8];
    std::snprintf(hm, sizeof(hm), "%02d:%02d", t.tm_hour, t.tm_min);
    time_.set(hm, "", true);

    std::string cap;
    if (world_) {
      cap = std::string(kDays[t.tm_wday % 7]) + " · " + offset_text(seconds_of(t) - seconds_of(here));
    } else {
      char d[24];
      std::snprintf(d, sizeof(d), "%s %d %s", kDays[t.tm_wday % 7], t.tm_mday, kMonths[t.tm_mon % 12]);
      cap = d;
    }
    set_text(cap_lbl_, shown_cap_, cap);
    if (world_) set_text(glyph_lbl_, shown_glyph_, (t.tm_hour >= 6 && t.tm_hour < 18) ? kSun : kMoon);
  }
};

}  // namespace tilehaus
