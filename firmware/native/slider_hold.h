#pragma once
#include <cstdint>
#include <functional>
#include "lvgl.h"

namespace tilehaus {

// Makes a tile slider act only once it is held. A tile slider fills its whole
// cell, so without this any brush of the tile, a tap meant for the next tile,
// or the start of a swipe sets a light or a blind. The press must stay down
// kSliderHoldMs before the slider follows the finger; a shorter touch changes
// nothing and sends nothing to Home Assistant.
//
// It works by stopping LVGL's own slider handling in PREPROCESS callbacks, so
// attach() must run straight after lv_slider_create(), before any other
// callback on the slider. A stopped release also skips the card's own release
// callbacks, which is why on_cancel exists: it lets the card undo what its
// PRESSED callbacks started (an HaHold, a touch phase).
inline constexpr uint32_t kSliderHoldMs = 300;

struct SliderHold {
  bool armed = false;
  uint32_t pressed_at = 0;
  std::function<void()> on_arm;      // e.g. show the readout: the slider is live
  std::function<void()> on_cancel;   // a short touch ended without acting

  void attach(lv_obj_t *s) {
    lv_obj_add_event_cb(s, [](lv_event_t *e) {
      auto *self = static_cast<SliderHold *>(lv_event_get_user_data(e));
      self->armed = false;
      self->pressed_at = lv_tick_get();
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(s, [](lv_event_t *e) {
      auto *self = static_cast<SliderHold *>(lv_event_get_user_data(e));
      if (self->armed) return;
      if (lv_tick_elaps(self->pressed_at) < kSliderHoldMs) {
        lv_event_stop_processing(e);
        return;
      }
      self->armed = true;
      if (self->on_arm) self->on_arm();
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSING | LV_EVENT_PREPROCESS), this);
    auto end = [](lv_event_t *e) {
      auto *self = static_cast<SliderHold *>(lv_event_get_user_data(e));
      if (self->armed) return;
      lv_event_stop_processing(e);   // no jump to the touch point, no HA call
      if (self->on_cancel) self->on_cancel();
    };
    lv_obj_add_event_cb(s, end,
        static_cast<lv_event_code_t>(LV_EVENT_RELEASED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(s, end,
        static_cast<lv_event_code_t>(LV_EVENT_PRESS_LOST | LV_EVENT_PREPROCESS), this);
  }
};

}  // namespace tilehaus
