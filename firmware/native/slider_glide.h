#pragma once
#include "lvgl.h"
#include "card_style.h"  // kSliderAnimMs

namespace tilehaus {

// Makes a slider glide to a tapped position (and to programmatic / HA-pushed
// value changes) instead of snapping, while a live drag still tracks the finger
// 1:1. LVGL applies press/drag/tap positions to the value directly (no
// animation), so we detect a tap — a value change that lands on RELEASE rather
// than during PRESSING — and replay it as an animated set from the pre-press
// value. The touch phase is tracked in PREPROCESS callbacks so it is current
// before LVGL fires VALUE_CHANGED from its own press handling. This mirrors the
// tile slider behaviour; attach() also sets the glide duration on the slider.
struct SliderGlide {
  lv_obj_t *slider = nullptr;
  int touch_phase = 0;   // 0 idle, 1 pressing, 2 releasing
  int press_value = 0;   // slider value captured when the press began
  bool drag_moved = false;  // value changed *during* a press => real drag

  void attach(lv_obj_t *s) {
    slider = s;
    lv_obj_set_style_anim_duration(s, kSliderAnimMs, 0);
    lv_obj_add_event_cb(s, [](lv_event_t *e) {
      auto *self = static_cast<SliderGlide *>(lv_event_get_user_data(e));
      self->touch_phase = 1;
      self->press_value = static_cast<int>(lv_slider_get_value(self->slider));
      self->drag_moved = false;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(s, [](lv_event_t *e) {
      static_cast<SliderGlide *>(lv_event_get_user_data(e))->touch_phase = 1;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSING | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(s, [](lv_event_t *e) {
      static_cast<SliderGlide *>(lv_event_get_user_data(e))->touch_phase = 2;
    }, static_cast<lv_event_code_t>(LV_EVENT_RELEASED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(s, [](lv_event_t *e) {
      auto *self = static_cast<SliderGlide *>(lv_event_get_user_data(e));
      if (self->touch_phase == 1) self->drag_moved = true;  // moved while pressed
    }, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(s, [](lv_event_t *e) {
      auto *self = static_cast<SliderGlide *>(lv_event_get_user_data(e));
      if (!self->drag_moved) {  // a tap: replay the jump as an animated glide
        int target = static_cast<int>(lv_slider_get_value(self->slider));
        if (target != self->press_value) {
          lv_slider_set_value(self->slider, self->press_value, LV_ANIM_OFF);
          lv_slider_set_value(self->slider, target, LV_ANIM_ON);
        }
      }
      self->touch_phase = 0;
    }, LV_EVENT_RELEASED, this);
  }
};

}  // namespace tilehaus
