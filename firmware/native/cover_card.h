#pragma once
#include "lvgl.h"
#include <algorithm>
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

// Cover / blind tile: an "inverted" whole-tile fill anchored to the TOP (the
// shade descends from the top as it closes). LVGL vertical sliders only fill
// bottom-up, so the visual is a separate fill panel sized from the top, driven
// by a transparent full-tile slider that captures input. The value is HA
// position ("open percent", 0 = closed, 100 = open) so dragging down lowers the
// shade (closes); the fill height is the inverse (closed percent).
struct CoverCard : Card {
  lv_subject_t value_{};   // open percent 0..100 (HA position)
  lv_subject_t text_{};    // readout label
  char tbuf_[8];
  char tprev_[8];
  lv_obj_t *slider_ = nullptr;   // transparent input, on top
  lv_obj_t *fill_ = nullptr;     // visual shade, anchored top
  lv_obj_t *pct_lbl_ = nullptr;
  lv_timer_t *hide_timer_ = nullptr;
  std::string entity_;
  lv_obj_t *icon_lbl_ = nullptr;
  std::string icon_closed_;  // glyph when fully closed
  std::string icon_open_;    // glyph when any part open (empty = no swap)
  int touch_phase_ = 0;   // 0 idle, 1 pressing, 2 releasing
  int fill_closed_ = 0;   // current shade height (closed %), anim start value
  lv_obj_t *cell_ = nullptr;
  CardFonts fonts_{};
  std::string title_;
  std::unique_ptr<DetailModal> detail_;

  TileSpan default_size() const override { return {2, 4}; }  // tall

  static void on_change_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<CoverCard *>(lv_observer_get_user_data(o));
    int open = lv_subject_get_int(s);              // HA position 0..100
    int closed = 100 - open;
    if (self->fill_) {
      if (self->touch_phase_ == 1) {  // live drag: track the finger, no anim
        lv_anim_del(self->fill_, fill_h_cb);
        lv_obj_set_height(self->fill_, LV_PCT(closed));
      } else {  // HA update or tap: glide the shade to the new height
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, self->fill_);
        lv_anim_set_exec_cb(&a, fill_h_cb);
        lv_anim_set_values(&a, self->fill_closed_, closed);
        lv_anim_set_duration(&a, kSliderAnimMs);
        lv_anim_start(&a);
      }
      self->fill_closed_ = closed;
    }
    std::string t = format_brightness_pct(open);   // show HA position (open %)
    lv_subject_copy_string(&self->text_, t.c_str());
    set_icon_glyph(self->icon_lbl_, open == 0 ? self->icon_closed_ : self->icon_open_);
    if (self->pct_lbl_) {
      lv_anim_del(self->pct_lbl_, nullptr);
      lv_obj_set_style_opa(self->pct_lbl_, LV_OPA_COVER, 0);
      lv_timer_reset(self->hide_timer_);
      lv_timer_resume(self->hide_timer_);
    }
  }

  static void hide_cb(lv_timer_t *t) {
    auto *self = static_cast<CoverCard *>(lv_timer_get_user_data(t));
    lv_obj_fade_out(self->pct_lbl_, 400, 0);
    lv_timer_pause(t);
  }

  // Animate the shade panel's height (closed percent) between two values.
  static void fill_h_cb(void *var, int32_t v) {
    lv_obj_set_height(static_cast<lv_obj_t *>(var), LV_PCT(v));
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    lv_obj_set_style_pad_all(cell, 0, 0);

    // Visual shade: a top-anchored panel whose height is the closed percent.
    // The cell's clip_corner rounds its top edges to the tile.
    fill_ = lv_obj_create(cell);
    lv_obj_remove_style_all(fill_);
    lv_obj_set_style_bg_color(fill_, lv_color_hex(0xFF8C00), 0);
    lv_obj_set_style_bg_opa(fill_, LV_OPA_COVER, 0);
    lv_obj_set_width(fill_, LV_PCT(100));
    lv_obj_set_height(fill_, LV_PCT(0));
    lv_obj_align(fill_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(fill_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(fill_, LV_OBJ_FLAG_SCROLLABLE);

    // Transparent full-tile slider for input (only fill_ is drawn). Vertical so
    // dragging up increases the closed percent (shade comes down).
    slider_ = lv_slider_create(cell);
    lv_slider_set_orientation(slider_, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(slider_, 0, 100);
    lv_obj_set_size(slider_, LV_PCT(100), LV_PCT(100));
    lv_obj_center(slider_);
    lv_obj_set_style_opa(slider_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_opa(slider_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_opa(slider_, LV_OPA_TRANSP, LV_PART_KNOB);

    // Track the touch phase so on_change_cb knows whether a value change is a
    // live drag (track the finger, no anim) or a tap / HA update (glide). The
    // phase is set in PREPROCESS so it is current before LVGL fires
    // VALUE_CHANGED from its own press handling.
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      static_cast<CoverCard *>(lv_event_get_user_data(e))->touch_phase_ = 1;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      static_cast<CoverCard *>(lv_event_get_user_data(e))->touch_phase_ = 1;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSING | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      static_cast<CoverCard *>(lv_event_get_user_data(e))->touch_phase_ = 2;
    }, static_cast<lv_event_code_t>(LV_EVENT_RELEASED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      static_cast<CoverCard *>(lv_event_get_user_data(e))->touch_phase_ = 0;
    }, LV_EVENT_RELEASED, this);

    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon, LV_ALIGN_TOP_LEFT,
                         kTileInset, kTileInset);
    icon_closed_ = cfg.icon;
    icon_open_ = cfg.icon_alt;
    add_name(cell, fonts.body, cfg.title, cfg.hide_label, LV_ALIGN_BOTTOM_LEFT,
             kTileInset, -kTileInset,
             (cfg.detail && has_detail_modal(cfg.entity)) ? kDetailChevronReserve : 0);

    pct_lbl_ = lv_label_create(cell);
    lv_obj_set_style_text_color(pct_lbl_, lv_color_hex(0xFFFFFF), 0);
    if (fonts.value) lv_obj_set_style_text_font(pct_lbl_, fonts.value, 0);
    lv_obj_center(pct_lbl_);
    lv_obj_set_style_opa(pct_lbl_, LV_OPA_TRANSP, 0);
    hide_timer_ = lv_timer_create(hide_cb, 1500, this);
    lv_timer_pause(hide_timer_);

    tbuf_[0] = tprev_[0] = '\0';
    lv_subject_init_int(&value_, 0);
    lv_subject_init_string(&text_, tbuf_, tprev_, sizeof(tbuf_), "0%");
    lv_slider_bind_value(slider_, &value_);
    lv_subject_add_observer(&value_, on_change_cb, this);
    lv_label_bind_text(pct_lbl_, &text_, nullptr);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    lv_subject_t *subj = &value_;
    // HA reports current_position as open percent (0=closed, 100=open); the
    // tile value is the inverse (closed percent).
    ha_subscribe(cfg.entity, "current_position", [subj](const std::string &s) {
      float p = s.empty() ? 0.0f : static_cast<float>(atof(s.c_str()));
      if (std::isnan(p)) p = 0.0f;
      int open = std::max(0, std::min(100, static_cast<int>(p)));
      lv_subject_set_int(subj, open);
    });
    lv_obj_add_event_cb(slider_, [](lv_event_t *e) {
      auto *self = static_cast<CoverCard *>(lv_event_get_user_data(e));
      int position = static_cast<int>(lv_slider_get_value(self->slider_));
      ha_call_kv("cover.set_cover_position", self->entity_, "position",
                 std::to_string(position));
    }, LV_EVENT_RELEASED, this);

    // Opt-in "more-info" chevron -> cover detail modal (position / open-stop-
    // close / presets).
    if (cfg.detail && has_detail_modal(entity_)) {
      detail_ = make_detail_modal(entity_, title_, fonts_);
      add_detail_chevron(cell_, fonts_.icon, kTileInset, [](lv_event_t *e) {
        auto *self = static_cast<CoverCard *>(lv_event_get_user_data(e));
        if (self->detail_) self->detail_->open();
      }, this);
    }
  }
};

}  // namespace tilehaus
