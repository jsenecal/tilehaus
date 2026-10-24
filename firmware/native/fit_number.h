#pragma once
#include "lvgl.h"
#include <string>
#include "card.h"
#include "fit_text.h"
#include "palette.h"

namespace tilehaus {

// A number drawn as large as its space allows, with its unit smaller beside it
// on the same baseline ("23.4" big, "°C" small). The space is a fixed box the
// card sizes and places; set() measures the text and picks the face:
//
//  - the big 180px face (digits and . , - + : only), scaled down to fill the
//    box, while that stays at least kMinScaledH tall;
//  - otherwise the largest regular face (55 / 34 / 22px) that fits.
//
// The unit takes a face about 42% of the number's height. Text that the big
// face can't draw (a text state, the em dash) uses the regular faces only.
// fit_text.h holds the choice, host-tested.
struct FitNumber {
  static constexpr int kMinScaledH = 60;  // below this a downscaled face turns ragged
  static constexpr int kUnitGap = 4;

  lv_obj_t *box = nullptr;
  lv_obj_t *num = nullptr;
  lv_obj_t *unit = nullptr;
  CardFonts fonts{};
  bool centered = false;
  int box_w = 0, box_h = 0;  // as given: LVGL sizes objects lazily
  std::string shown_value, shown_unit;
  bool shown_numeric = false;

  // A box of w x h inside `parent`, placed by the caller. `centre` puts the
  // number and unit in the middle; otherwise they sit at the left.
  void create(lv_obj_t *parent, int w, int h, const CardFonts &f, bool centre) {
    fonts = f;
    centered = centre;
    box_w = w;
    box_h = h;
    box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, w, h);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    num = lv_label_create(box);
    unit = lv_label_create(box);
    lv_obj_set_style_text_color(num, lv_color_hex(kTileInk), 0);
    lv_obj_set_style_text_color(unit, lv_color_hex(kTileInk), 0);
    lv_obj_set_style_text_letter_space(num, -1, 0);
    lv_label_set_text(num, "");
    lv_label_set_text(unit, "");
  }

  void set(const std::string &value, const std::string &unit_text, bool numeric) {
    if (box == nullptr) return;
    if (value == shown_value && unit_text == shown_unit && numeric == shown_numeric) return;
    shown_value = value;
    shown_unit = unit_text;
    shown_numeric = numeric;
    layout();
  }

  void layout() {
    const int avail_w = box_w;
    const int avail_h = box_h;
    const char *v = shown_value.c_str();

    // Candidates, largest first: the big face only for what it can draw.
    const lv_font_t *faces[4];
    FitCandidate cand[4];
    int n = 0;
    auto add = [&](const lv_font_t *f, bool scalable) {
      if (f == nullptr) return;
      lv_point_t sz;
      lv_text_get_size(&sz, v, f, -1, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      faces[n] = f;
      cand[n] = FitCandidate{static_cast<int>(sz.x), static_cast<int>(lv_font_get_line_height(f)), scalable};
      ++n;
    };
    if (shown_numeric) add(fonts.clock_xl, true);
    add(fonts.value, false);
    add(fonts.medium, false);
    add(fonts.body, false);
    if (n == 0) return;

    // The unit's width depends on its face, which depends on the number's
    // height: guess from the box, fit, then settle the unit face and refit once.
    const lv_font_t *unit_faces[3] = {fonts.medium, fonts.body, fonts.body_tight};
    int unit_h[3];
    for (int i = 0; i < 3; ++i) unit_h[i] = unit_faces[i] ? lv_font_get_line_height(unit_faces[i]) : 0;
    const bool has_unit = !shown_unit.empty();
    auto unit_width = [&](int ui) {
      if (!has_unit || unit_faces[ui] == nullptr) return 0;
      lv_point_t sz;
      lv_text_get_size(&sz, shown_unit.c_str(), unit_faces[ui], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
      return static_cast<int>(sz.x) + kUnitGap;
    };
    int ui = choose_unit_face(unit_h, 3, avail_h);
    FitChoice fit = choose_fit(cand, n, avail_w - unit_width(ui), avail_h, kMinScaledH);
    const int ui2 = choose_unit_face(unit_h, 3, fit.height);
    if (ui2 != ui) {
      ui = ui2;
      fit = choose_fit(cand, n, avail_w - unit_width(ui), avail_h, kMinScaledH);
    }
    const lv_font_t *nf = faces[fit.index];

    lv_obj_set_style_text_font(num, nf, 0);
    lv_label_set_text(num, v);
    lv_obj_set_style_transform_pivot_x(num, 0, 0);
    lv_obj_set_style_transform_pivot_y(num, 0, 0);
    lv_obj_set_style_transform_scale(num, fit.scale, 0);
    if (has_unit && unit_faces[ui]) lv_obj_set_style_text_font(unit, unit_faces[ui], 0);
    lv_label_set_text(unit, has_unit ? shown_unit.c_str() : "");

    // Place them: the number's drawn box, then the unit on its baseline.
    const int vw = cand[fit.index].w * fit.scale / 256;
    const int vh = fit.height;
    const int uw = unit_width(ui);
    const int total = vw + uw;
    const int x0 = centered ? (avail_w - total) / 2 : 0;
    const int y0 = (avail_h - vh) / 2;
    lv_obj_set_pos(num, x0, y0);
    if (has_unit && unit_faces[ui]) {
      const lv_font_t *uf = unit_faces[ui];
      const int num_baseline = (lv_font_get_line_height(nf) - nf->base_line) * fit.scale / 256;
      const int unit_baseline = lv_font_get_line_height(uf) - uf->base_line;
      lv_obj_set_pos(unit, x0 + vw + kUnitGap, y0 + num_baseline - unit_baseline);
    }
  }
};

}  // namespace tilehaus
