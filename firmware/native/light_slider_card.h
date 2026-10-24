#pragma once
#include "lvgl.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <memory>
#include "card.h"
#include "card_style.h"
#include "ha.h"
#include "ha_hold.h"
#include "slider_hold.h"
#include "slider_glide.h"
#include "slider_map.h"
#include "detail_modal.h"

namespace tilehaus {

// Whole-tile vertical fill slider with a centered readout that flashes on change
// then fades out. Serves two entity kinds, auto-detected from the entity domain:
//   * light.*            -> brightness: 0..100 %, maps to/from 0..255 brightness,
//                           light.turn_on/off, readout "NN%".
//   * number./input_number. -> raw value over the entity's min/max, number.set_value,
//                           readout "NN" (no unit).
// Both share the fill look, the transient readout + fade, and the tap-as-glide
// replay. HA pushes are ignored while dragging; the value is sent on release.
struct LightSliderCard : Card {
  lv_subject_t value_{};   // slider position (0..100 for light, min..max for number)
  lv_subject_t text_{};    // readout label
  char tbuf_[12];
  char tprev_[12];
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *slider_ = nullptr;
  lv_obj_t *pct_lbl_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  lv_timer_t *hide_timer_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_;
  std::unique_ptr<DetailModal> detail_;
  std::string icon_on_;   // filled glyph when > 0
  std::string icon_off_;  // outline glyph at 0 (empty = no swap)
  SliderHold press_;   // acts only once held; a brush of the tile does nothing
  SliderGlide glide_;  // tap glides, drag tracks the finger; owns touch phase
  HaHold hold_;  // no HA pushes onto the slider mid-drag or while it settles
  bool is_number_ = false;   // number entity vs light brightness
  int nmin_ = 0, nmax_ = 100, ncur_ = 0;  // number range + last pushed value
  bool follow_color_ = false;  // fill colour tracks the light's own colour
  int fr_ = -1, fg_ = -1, fb_ = -1;  // last rgb_color (-1 = unknown)

  TileSpan default_size() const override { return {2, 4}; }  // tall

  std::string format_val(int v) const {
    return is_number_ ? std::to_string(v) : format_brightness_pct(v);
  }

  // Parse up to three integers out of an HA colour attribute ("[r, g, b]" or
  // "(r, g, b)"). Returns the count found.
  static int parse_ints(const std::string &s, int *out, int max) {
    int n = 0;
    for (size_t i = 0; i < s.size() && n < max;) {
      if (isdigit(static_cast<unsigned char>(s[i]))) {
        out[n++] = std::atoi(s.c_str() + i);
        while (i < s.size() && isdigit(static_cast<unsigned char>(s[i]))) ++i;
      } else {
        ++i;
      }
    }
    return n;
  }

  // Paint the fill (indicator) with the light's colour when following one and it
  // is known; otherwise the default orange. Fill *height* already shows
  // brightness, so the colour is used raw (not brightness-scaled).
  void apply_fill_color() {
    if (!slider_) return;
    uint32_t c = (follow_color_ && fr_ >= 0)
                     ? ((static_cast<uint32_t>(fr_) << 16) |
                        (static_cast<uint32_t>(fg_) << 8) |
                        static_cast<uint32_t>(fb_))
                     : tile_on_color();
    lv_obj_set_style_bg_color(slider_, lv_color_hex(c), LV_PART_INDICATOR);
    ink_fill();
  }

  // Labels the fill (rising from the bottom) covers take the ink for its
  // colour; the rest keep the track's. Uses the slider's target value, so
  // during a 100ms glide the ink can lead the fill by a frame or two.
  void ink_fill() {
    if (!slider_) return;
    lv_obj_t *cell = lv_obj_get_parent(slider_);
    lv_obj_update_layout(cell);
    lv_area_t a;
    lv_obj_get_coords(slider_, &a);
    const int32_t lo = lv_slider_get_min_value(slider_), hi = lv_slider_get_max_value(slider_);
    const int32_t v = lv_slider_get_value(slider_);
    const int32_t h = lv_area_get_height(&a);
    const int32_t top = hi > lo ? a.y2 - h * (v - lo) / (hi - lo) : a.y2;
    uint32_t fill = 0, track = kTileBg;
    fill = style_rgb(lv_obj_get_style_bg_color(slider_, LV_PART_INDICATOR));
    opaque_bg(slider_, LV_PART_MAIN, &track);
    ink_under_band(cell, v > lo ? top : a.y2 + 1, a.y2, needs_dark_ink(fill), needs_dark_ink(track));
  }

  // Flash the centered readout on every change, then auto-hide it 1.5s after the
  // last change (avoids the bottom-corner labels colliding at the extremes).
  static void relabel_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<LightSliderCard *>(lv_observer_get_user_data(o));
    int v = lv_subject_get_int(s);
    std::string t = self->format_val(v);
    lv_subject_copy_string(&self->text_, t.c_str());
    set_icon_glyph(self->icon_lbl_, v > 0 ? self->icon_on_ : self->icon_off_);
    self->ink_fill();
    self->show_readout();
  }

  // Show the centered readout now, fading out 1.5s after the last call.
  void show_readout() {
    if (!pct_lbl_) return;
    lv_anim_del(pct_lbl_, nullptr);  // stop any in-flight fade-out
    lv_obj_set_style_opa(pct_lbl_, LV_OPA_COVER, 0);  // snap to visible
    lv_timer_reset(hide_timer_);
    lv_timer_resume(hide_timer_);
  }

  static void hide_cb(lv_timer_t *t) {
    auto *self = static_cast<LightSliderCard *>(lv_timer_get_user_data(t));
    lv_obj_fade_out(self->pct_lbl_, 400, 0);  // gentle 400ms opacity fade
    lv_timer_pause(t);
  }

  // Apply the number entity's range and re-place the last value inside it.
  void apply_number_range() {
    if (nmax_ <= nmin_) nmax_ = nmin_ + 1;
    lv_slider_set_range(slider_, nmin_, nmax_);
    int v = ncur_ < nmin_ ? nmin_ : (ncur_ > nmax_ ? nmax_ : ncur_);
    if (glide_.touch_phase == 0) lv_subject_set_int(&value_, v);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    is_number_ = cfg.entity.rfind("number.", 0) == 0 ||
                 cfg.entity.rfind("input_number.", 0) == 0;
    follow_color_ = cfg.follow_color && !is_number_;
    // The tile IS the control: a vertical slider filling the cell, with the
    // icon / name / value as non-clickable overlay labels created afterwards so
    // they render on top and let touches fall through to the slider.
    lv_obj_set_style_pad_all(cell, 0, 0);
    slider_ = lv_slider_create(cell);
    lv_slider_set_orientation(slider_, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(slider_, is_number_ ? nmin_ : 0, is_number_ ? nmax_ : 100);
    lv_obj_set_size(slider_, LV_PCT(100), LV_PCT(100));
    lv_obj_align(slider_, LV_ALIGN_CENTER, 0, 0);
    style_fill_slider(slider_);
    press_.attach(slider_);  // first: it stops LVGL's handling of a short touch
    press_.on_arm = [this]() { show_readout(); };
    glide_.attach(slider_);  // tap glides, drag tracks the finger 1:1

    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon, LV_ALIGN_TOP_LEFT,
                         kTileInset, kTileInset);
    icon_on_ = cfg.icon;
    icon_off_ = cfg.icon_alt;
    add_name(cell, fonts, cfg.title, cfg.hide_label, LV_ALIGN_BOTTOM_LEFT,
             kTileInset, -kTileInset);

    // Centered transient readout: large, transparent until a change flashes it
    // visible; auto-fades out via hide_timer_.
    pct_lbl_ = lv_label_create(cell);
    lv_obj_set_style_text_color(pct_lbl_, lv_color_hex(kTileInk), 0);
    if (fonts.value) lv_obj_set_style_text_font(pct_lbl_, fonts.value, 0);
    lv_obj_center(pct_lbl_);
    lv_obj_set_style_opa(pct_lbl_, LV_OPA_TRANSP, 0);
    hide_timer_ = lv_timer_create(hide_cb, 1500, this);
    lv_timer_pause(hide_timer_);

    tbuf_[0] = tprev_[0] = '\0';
    lv_subject_init_int(&value_, 0);
    lv_subject_init_string(&text_, tbuf_, tprev_, sizeof(tbuf_),
                           is_number_ ? "0" : "0%");
    lv_slider_bind_value(slider_, &value_);
    lv_subject_add_observer(&value_, relabel_cb, this);
    lv_label_bind_text(pct_lbl_, &text_, nullptr);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    hold_.attach(slider_);
    press_.on_cancel = [this]() {
      hold_.release();
      glide_.touch_phase = 0;
    };
    if (is_number_) {
      ha_subscribe(entity_, "min", [this](const std::string &s) {
        if (!s.empty()) { nmin_ = static_cast<int>(std::atof(s.c_str())); apply_number_range(); }
      });
      ha_subscribe(entity_, "max", [this](const std::string &s) {
        if (!s.empty()) { nmax_ = static_cast<int>(std::atof(s.c_str())); apply_number_range(); }
      });
      ha_subscribe(entity_, nullptr, [this](const std::string &s) {
        if (s.empty() || s == "unknown" || s == "unavailable") return;
        ncur_ = static_cast<int>(std::lround(std::atof(s.c_str())));
        hold_.gate([this]() { lv_subject_set_int(&value_, ncur_); });
      });
      lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
        auto *self = static_cast<LightSliderCard *>(lv_event_get_user_data(e));
        ha_call_kv("number.set_value", self->entity_, "value",
                   std::to_string(static_cast<int>(lv_slider_get_value(self->slider_))));
      }, LV_EVENT_RELEASED, this);
      return;
    }
    subscribe_brightness(entity_, &value_, hold_);
    if (follow_color_) {
      ha_subscribe(entity_, "rgb_color", [this](const std::string &s) {
        int c[3];
        if (parse_ints(s, c, 3) == 3) { fr_ = c[0]; fg_ = c[1]; fb_ = c[2]; }
        else fr_ = fg_ = fb_ = -1;  // no colour (e.g. off) -> default fill
        apply_fill_color();
      });
    }
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      auto *self = static_cast<LightSliderCard *>(lv_event_get_user_data(e));
      int pct = static_cast<int>(lv_slider_get_value(self->slider_));
      uint8_t bri = slider_to_brightness(pct);
      if (bri == 0) ha_call("light.turn_off", self->entity_);
      else ha_call_kv("light.turn_on", self->entity_, "brightness",
                      std::to_string(static_cast<int>(bri)));
    }, LV_EVENT_RELEASED, this);

    // Opt-in "more-info" chevron (light entities only for now).
    if (cfg.detail && has_detail_modal(entity_)) {
      detail_ = make_detail_modal(entity_, title_, fonts_);
      add_detail_chevron(cell_, fonts_.icon, kTileInset, [](lv_event_t *e) {
        auto *self = static_cast<LightSliderCard *>(lv_event_get_user_data(e));
        if (self->detail_) self->detail_->open();
      }, this);
    }
  }

  // apply_fill_color() already reads tile_on_color() live and repaints
  // slider_'s indicator directly (it is also the sole path that repaints the
  // fill after the initial style_fill_slider() in build() — see
  // apply_fill_color's own comment), so restyle only needs to call it again.
  // No lv_subject involved, so the "does set_int re-notify" question does not
  // apply here.
  void restyle() override { apply_fill_color(); }
};

}  // namespace tilehaus
