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
  int touch_phase_ = 0;   // 0 idle, 1 pressing, 2 releasing
  int press_value_ = 0;   // slider value captured when the press began
  bool drag_moved_ = false;  // value changed *during* a press => real drag
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
                     : accent_or(0xFF8C00);
    lv_obj_set_style_bg_color(slider_, lv_color_hex(c), LV_PART_INDICATOR);
  }

  // Flash the centered readout on every change, then auto-hide it 1.5s after the
  // last change (avoids the bottom-corner labels colliding at the extremes).
  static void relabel_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<LightSliderCard *>(lv_observer_get_user_data(o));
    int v = lv_subject_get_int(s);
    std::string t = self->format_val(v);
    lv_subject_copy_string(&self->text_, t.c_str());
    set_icon_glyph(self->icon_lbl_, v > 0 ? self->icon_on_ : self->icon_off_);
    if (self->pct_lbl_) {
      lv_anim_del(self->pct_lbl_, nullptr);  // stop any in-flight fade-out
      lv_obj_set_style_opa(self->pct_lbl_, LV_OPA_COVER, 0);  // snap to visible
      lv_timer_reset(self->hide_timer_);
      lv_timer_resume(self->hide_timer_);
    }
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
    if (touch_phase_ == 0) lv_subject_set_int(&value_, v);
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
    style_fill_slider(slider_, 10);
    lv_obj_set_style_anim_duration(slider_, kSliderAnimMs, 0);

    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      auto *self = static_cast<LightSliderCard *>(lv_event_get_user_data(e));
      self->touch_phase_ = 1;
      self->press_value_ = static_cast<int>(lv_slider_get_value(self->slider_));
      self->drag_moved_ = false;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      static_cast<LightSliderCard *>(lv_event_get_user_data(e))->touch_phase_ = 1;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSING | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      static_cast<LightSliderCard *>(lv_event_get_user_data(e))->touch_phase_ = 2;
    }, static_cast<lv_event_code_t>(LV_EVENT_RELEASED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      auto *self = static_cast<LightSliderCard *>(lv_event_get_user_data(e));
      if (self->touch_phase_ == 1) self->drag_moved_ = true;  // moved while pressed
    }, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      auto *self = static_cast<LightSliderCard *>(lv_event_get_user_data(e));
      if (!self->drag_moved_) {  // a tap: replay the jump as an animated glide
        int target = static_cast<int>(lv_slider_get_value(self->slider_));
        if (target != self->press_value_) {
          lv_slider_set_value(self->slider_, self->press_value_, LV_ANIM_OFF);
          lv_slider_set_value(self->slider_, target, LV_ANIM_ON);
        }
      }
      self->touch_phase_ = 0;
    }, LV_EVENT_RELEASED, this);

    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon, LV_ALIGN_TOP_LEFT,
                         kTileInset, kTileInset);
    icon_on_ = cfg.icon;
    icon_off_ = cfg.icon_alt;
    add_name(cell, fonts, cfg.title, cfg.hide_label, LV_ALIGN_BOTTOM_LEFT,
             kTileInset, -kTileInset);

    // Centered transient readout: large, transparent until a change flashes it
    // visible; auto-fades out via hide_timer_.
    pct_lbl_ = lv_label_create(cell);
    lv_obj_set_style_text_color(pct_lbl_, lv_color_hex(0xFFFFFF), 0);
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
        if (touch_phase_ == 0) lv_subject_set_int(&value_, ncur_);
      });
      lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
        auto *self = static_cast<LightSliderCard *>(lv_event_get_user_data(e));
        ha_call_kv("number.set_value", self->entity_, "value",
                   std::to_string(static_cast<int>(lv_slider_get_value(self->slider_))));
      }, LV_EVENT_RELEASED, this);
      return;
    }
    lv_subject_t *subj = &value_;
    ha_subscribe(entity_, "brightness", [subj](const std::string &s) {
      float b = s.empty() ? 0.0f : static_cast<float>(atof(s.c_str()));
      if (std::isnan(b)) b = 0.0f;
      lv_subject_set_int(subj, brightness_to_slider(static_cast<uint8_t>(b)));
    });
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
};

}  // namespace tilehaus
