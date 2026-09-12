#pragma once
#include "lvgl.h"
#include <cstdio>
#include <string>
#include <vector>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "weather_glyph.h"
#include "tile_scale.h"

namespace tilehaus {

// Multi-day forecast tile. Reads a compact "Day|condition|high|low;..." string
// (populated in HA by a weather.get_forecasts automation into an input_text) and
// renders one column per day: weekday, condition glyph, high°, low°. Read-only.
struct WeatherForecastCard : Card {
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *row_ = nullptr;
  CardFonts fonts_{};
  std::string last_;
  bool stack_hilo_ = true;

  TileSpan default_size() const override { return {4, 2}; }

  static std::vector<std::string> split(const std::string &s, char d) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
      if (c == d) { out.push_back(cur); cur.clear(); }
      else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    // rebuild() runs later, from the HA subscription, by which point build_page
    // has reset tile_metrics() — so decide the layout now, while it is valid.
    stack_hilo_ = forecast_stacks_hilo(tile_metrics().px_h - 2 * kTileInset);
    row_ = lv_obj_create(cell);
    lv_obj_remove_style_all(row_);
    lv_obj_set_style_bg_opa(row_, LV_OPA_TRANSP, 0);
    lv_obj_set_size(row_, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(row_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
  }

  static lv_obj_t *add_label(lv_obj_t *parent, const lv_font_t *font,
                             uint32_t color, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, text);
    return l;
  }

  void rebuild(const std::string &s) {
    lv_obj_clean(row_);
    for (auto &day : split(s, ';')) {
      auto f = split(day, '|');
      if (f.size() < 4) continue;
      lv_obj_t *col = lv_obj_create(row_);
      lv_obj_remove_style_all(col);
      lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
      lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
      lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      lv_obj_set_style_pad_row(col, 4, 0);

      // Compact columns also drop to the small rung: five columns share ~322px,
      // so ~58px each, and "22° 20°" at 22px is ~80px — putting the pair on one
      // line fixes the height but overruns the width without this.
      const lv_font_t *tf = stack_hilo_ ? fonts_.body
                                        : (fonts_.small ? fonts_.small : fonts_.body);
      add_label(col, tf, 0xC8C8C8, f[0].c_str());   // weekday
      add_label(col, fonts_.icon, 0xFFFFFF, weather_glyph(f[1]));  // condition
      char hi[16], lo[16];
      std::snprintf(hi, sizeof(hi), "%s\xC2\xB0", f[2].c_str());
      // On one line the low carries the separator so the pair reads "22°/20°"
      // with no gap; stacked, it is just the temperature on its own row.
      std::snprintf(lo, sizeof(lo), "%s%s\xC2\xB0", stack_hilo_ ? "" : "/",
                    f[3].c_str());
      if (stack_hilo_) {
        add_label(col, tf, 0xFFFFFF, hi);  // high
        add_label(col, tf, 0x909090, lo);  // low
      } else {
        // Short tile: the pair shares a line, saving a whole row. Kept as two
        // labels rather than one string so the high stays white and the low
        // stays grey — that contrast is the readable part.
        lv_obj_t *hl = lv_obj_create(col);
        lv_obj_remove_style_all(hl);
        lv_obj_set_style_bg_opa(hl, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(hl, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(hl, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(hl, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(hl, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(hl, 0, 0);
        add_label(hl, tf, 0xFFFFFF, hi);   // "22°"
        add_label(hl, tf, 0x909090, lo);   // "/20°" — slash carried by the low
      }
    }
  }

  void bind(const CardConfig &cfg) override {
    ha_subscribe(cfg.entity, nullptr, [this](const std::string &s) {
      if (s == last_ || s.empty() || s == "unknown" || s == "unavailable")
        return;
      last_ = s;
      rebuild(s);
    });
  }
};

}  // namespace tilehaus
