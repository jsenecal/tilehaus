#pragma once
#include "lvgl.h"
#include <cstdint>
#include <cstdlib>
#include <string>
#include "card.h"         // CardFonts
#include "card_style.h"   // style_fill_slider, kTileInset
#include "ha.h"
#include "ha_hold.h"
#include "slider_glide.h"
#include "slider_map.h"

namespace tilehaus {

// Brightness tab: a tall, square, outlined vertical fill slider bound to its
// own value subject, with the % readout above it, gliding to tapped/HA values.
// Owns its brightness subscription.
struct BrightnessTab {
  lv_subject_t value_{};
  lv_subject_t text_{};
  char buf_[8] = {0};
  char prev_[8] = {0};
  lv_obj_t *slider_ = nullptr;
  lv_obj_t *lbl_ = nullptr;
  SliderGlide glide_;
  HaHold hold_;  // no HA pushes onto the slider mid-drag or while it settles
  std::string entity_;

  // Override the slider fill colour (follow-colour lights tint it to the light's
  // own colour, mirroring the tile). No-op before the slider is built.
  void set_fill_color(uint32_t rgb) {
    if (slider_) lv_obj_set_style_bg_color(slider_, lv_color_hex(rgb), LV_PART_INDICATOR);
  }

  static void grip_draw_cb(lv_event_t *e) {
    constexpr int kGripH = 4, kGripInset = 12;  // line height; gap below fill top
    lv_obj_t *s = lv_event_get_target_obj(e);
    lv_area_t a;
    lv_obj_get_coords(s, &a);
    const int32_t h = lv_area_get_height(&a);
    const int32_t fill = h * lv_slider_get_value(s) / 100;
    if (fill < 2 * kGripInset + kGripH) return;  // no room inside a short fill
    const int32_t w = lv_area_get_width(&a);
    const int32_t y = a.y2 - fill + kGripInset;
    fill_rect(lv_event_get_layer(e),
              lv_area_t{a.x1 + w / 4, y, a.x2 - w / 4, y + kGripH - 1}, kTileInk);
  }

  static void relabel_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<BrightnessTab *>(lv_observer_get_user_data(o));
    int v = lv_subject_get_int(s);
    std::string t = format_brightness_pct(v);
    lv_subject_copy_string(&self->text_, t.c_str());
  }

  void build(lv_obj_t *panel, const std::string &entity, const CardFonts &fonts,
             int panel_h) {
    entity_ = entity;
    // The % readout sits above the slider, on the page; the slider takes the
    // rest of the height below it.
    lbl_ = lv_label_create(panel);
    lv_obj_set_style_text_color(lbl_, lv_color_hex(kTileInk), 0);  // inked on the page
    if (fonts.value) lv_obj_set_style_text_font(lbl_, fonts.value, 0);
    lv_obj_align(lbl_, LV_ALIGN_TOP_MID, 0, 0);
    const int lbl_h = fonts.value ? lv_font_get_line_height(fonts.value) : 66;

    slider_ = lv_slider_create(panel);
    lv_slider_set_orientation(slider_, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(slider_, 0, 100);
    lv_obj_set_size(slider_, 140, panel_h - lbl_h - 2 * kTileInset);
    lv_obj_align(slider_, LV_ALIGN_BOTTOM_MID, 0, -kTileInset);
    style_fill_slider(slider_);
    // An ink outline a few px clear of the slider, drawn outside it so the
    // fill keeps the full track. Follows night mode via retheme.
    lv_obj_set_style_outline_width(slider_, 2, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(slider_, 4, LV_PART_MAIN);
    lv_obj_set_style_outline_color(slider_, lv_color_hex(kInk), LV_PART_MAIN);
    lv_obj_set_style_outline_opa(slider_, LV_OPA_COVER, LV_PART_MAIN);
    // A grip line inside the fill, just under its top edge (as in Home
    // Assistant's light dialog). Drawn from the live value, so it rides the
    // glide; no knob.
    lv_obj_add_event_cb(slider_, grip_draw_cb, LV_EVENT_DRAW_POST, nullptr);
    glide_.attach(slider_);

    lv_subject_init_int(&value_, 0);
    lv_subject_init_string(&text_, buf_, prev_, sizeof(buf_), "0%");
    lv_slider_bind_value(slider_, &value_);
    lv_subject_add_observer(&value_, relabel_cb, this);
    lv_label_bind_text(lbl_, &text_, nullptr);

    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      auto *self = static_cast<BrightnessTab *>(lv_event_get_user_data(e));
      int pct = static_cast<int>(lv_slider_get_value(self->slider_));
      uint8_t bri = slider_to_brightness(pct);
      if (bri == 0) ha_call("light.turn_off", self->entity_);
      else ha_call_kv("light.turn_on", self->entity_, "brightness",
                      std::to_string(static_cast<int>(bri)));
    }, LV_EVENT_RELEASED, this);

    hold_.attach(slider_);
    subscribe_brightness(entity_, &value_, hold_);
  }
};

}  // namespace tilehaus
