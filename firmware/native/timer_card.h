#pragma once
#include "lvgl.h"
#include <algorithm>
#include <string>
#include "card.h"
#include "card_style.h"
#include "clock.h"
#include "ha.h"
#include "theme.h"
#include "timer_state.h"

namespace tilehaus {

// A Home Assistant timer as a countdown ring (type 24). The ring shows the share
// of the duration left and the tile tints while the timer runs. Tap starts,
// pauses or resumes; a long press cancels.
//
// Two layouts, by tile size. A roomy tile (3x3 and up on the 7" panel) has the
// state as a mono caption top-left, the ring in the middle and the name
// bottom-left. A small tile drops the caption: the ring sits at the top with
// the name under it.
//
// HA pushes `remaining` only on state changes, so while the timer runs a 1s
// LVGL timer recomputes it from `finishes_at` and the panel clock
// (timer_state.h, host-tested).
struct TimerCard : Card {
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *arc_ = nullptr;
  lv_obj_t *time_lbl_ = nullptr;
  lv_obj_t *state_lbl_ = nullptr;  // roomy layout: "RUNNING" caption
  CardFonts fonts_{};
  int ring_ = 0;
  std::string entity_, state_, duration_, remaining_, finishes_at_;
  uint32_t on_color_ = kTileOnBuiltin;
  uint32_t off_color_ = kTileBg;
  int32_t cfg_active_color_ = -1;
  bool active_ = false;
  std::string shown_;  // the countdown text on screen, to skip no-op relabels

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg, const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    cfg_active_color_ = cfg.active_color;
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    off_color_ = resolve_color(cfg.inactive_color, kTileBg);

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
    // Roomy: centred in the band between the caption and the name. Small: at
    // the top, above the name.
    if (roomy) lv_obj_align(arc_, LV_ALIGN_CENTER, 0, (kCaptionH - kNameH) / 2);
    else lv_obj_align(arc_, LV_ALIGN_TOP_MID, 0, 0);
    lv_arc_set_rotation(arc_, 270);  // start at 12 o'clock
    lv_arc_set_bg_angles(arc_, 0, 360);
    lv_arc_set_range(arc_, 0, 1000);
    lv_arc_set_value(arc_, 1000);
    lv_obj_remove_flag(arc_, LV_OBJ_FLAG_CLICKABLE);  // taps belong to the tile
    lv_obj_set_style_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
    const int stroke = std::max(5, ring_ / 11);
    lv_obj_set_style_arc_width(arc_, stroke, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, stroke, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(kBlack), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc_, LV_OPA_30, LV_PART_MAIN);

    time_lbl_ = lv_label_create(arc_);
    lv_obj_center(time_lbl_);
    if (roomy) {
      state_lbl_ = lv_label_create(cell);
      if (fonts.caption) lv_obj_set_style_text_font(state_lbl_, fonts.caption, 0);
      lv_obj_set_style_text_letter_space(state_lbl_, 2, 0);
      lv_obj_align(state_lbl_, LV_ALIGN_TOP_LEFT, 0, 0);
    }
    add_name(cell, fonts, cfg.title, cfg.hide_label);

    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    render();
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    ha_subscribe(entity_, nullptr, [this](const std::string &s) { state_ = s; render(); });
    ha_subscribe(entity_, "duration", [this](const std::string &s) { duration_ = s; render(); });
    ha_subscribe(entity_, "remaining", [this](const std::string &s) { remaining_ = s; render(); });
    ha_subscribe(entity_, "finishes_at", [this](const std::string &s) { finishes_at_ = s; render(); });

    // SHORT_CLICKED, not CLICKED: LVGL still sends CLICKED after a long press.
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<TimerCard *>(lv_event_get_user_data(e));
      ha_call(self->active_ ? "timer.pause" : "timer.start", self->entity_);
    }, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<TimerCard *>(lv_event_get_user_data(e));
      ha_call("timer.cancel", self->entity_);
    }, LV_EVENT_LONG_PRESSED, this);

    lv_timer_create([](lv_timer_t *t) {
      auto *self = static_cast<TimerCard *>(lv_timer_get_user_data(t));
      if (self->active_) self->render();
    }, 1000, this);
  }

  void restyle() override {
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    render();
  }

 private:
  static const char *state_caption(TimerPhase p) {
    switch (p) {
      case TimerPhase::Active: return "RUNNING";
      case TimerPhase::Paused: return "PAUSED";
      case TimerPhase::Idle: return "READY";
      case TimerPhase::Unknown: break;
    }
    return "";
  }

  static int64_t now_s() {
    if (app_clock() == nullptr) return -1;
    const esphome::ESPTime t = app_clock()->now();
    return t.is_valid() ? static_cast<int64_t>(t.timestamp) : -1;
  }

  void render() {
    if (cell_ == nullptr) return;
    const TimerView v = timer_view(state_, duration_, remaining_, finishes_at_, now_s());
    active_ = v.phase == TimerPhase::Active;
    paint_state_cell(cell_, active_, on_color_, off_color_);

    // Ink that reads on the current fill: dark on lime, white on charcoal.
    const uint32_t fill = active_ ? on_color_ : off_color_;
    const uint32_t ink = needs_dark_ink(fill) ? kTileBg : kTileInk;
    lv_obj_set_style_arc_color(arc_, lv_color_hex(ink), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(time_lbl_, lv_color_hex(ink), 0);
    if (state_lbl_) {
      // The caption is the quiet line: ink at reduced strength reads as muted
      // on both the lime and the charcoal fill.
      lv_obj_set_style_text_color(state_lbl_, lv_color_hex(ink), 0);
      lv_obj_set_style_text_opa(state_lbl_, LV_OPA_70, 0);
      lv_label_set_text(state_lbl_, state_caption(v.phase));
    }
    // Paused reads as held: the countdown dims until it runs again.
    lv_obj_set_style_text_opa(time_lbl_, v.phase == TimerPhase::Paused ? LV_OPA_60 : LV_OPA_COVER, 0);
    lv_arc_set_value(arc_, v.ring_permille());

    const std::string text = format_countdown(v.remaining_s);
    if (text != shown_) {
      shown_ = text;
      // The largest face that fits inside the ring, a step down for h:mm:ss.
      const lv_font_t *steps[] = {fonts_.value, fonts_.medium, fonts_.body, fonts_.small};
      int i = ring_ >= 200 ? 0 : ring_ >= 120 ? 1 : 2;
      if (text.size() > 5) ++i;
      const lv_font_t *f = steps[i];
      if (f != nullptr) lv_obj_set_style_text_font(time_lbl_, f, 0);
      lv_label_set_text(time_lbl_, text.c_str());
    }
  }
};

}  // namespace tilehaus
