#pragma once
#include "lvgl.h"
#include <cstdint>
#include <functional>
#include <utility>

namespace tilehaus {

// Keeps Home Assistant's state pushes off a control while someone is using it.
//
// Sliders send their value once, on release. But HA keeps pushing state the
// whole time: echoes of earlier commands, and — for lights with a transition —
// a run of intermediate brightness values as the light fades. Applied straight
// to the slider, those yank it out from under the finger mid-drag, and after
// release make it snap back to the old value and creep forward, which reads as
// the slider applying as you drag.
//
// gate() applies an update at once when the control is idle. While it is
// touched, and for kSettleMs after release, the update is held instead —
// only the latest — and applied when the settle time runs out, so the control
// rests where it was let go and then shows HA's final word.
struct HaHold {
  static constexpr uint32_t kSettleMs = 1500;

  bool touching = false;
  bool settling = false;
  std::function<void()> pending;
  lv_timer_t *timer = nullptr;

  // Track presses on the control. Call once, after the control exists.
  void attach(lv_obj_t *obj) {
    lv_obj_add_event_cb(obj, [](lv_event_t *e) {
      static_cast<HaHold *>(lv_event_get_user_data(e))->touching = true;
    }, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(obj, release_cb, LV_EVENT_RELEASED, this);
    lv_obj_add_event_cb(obj, release_cb, LV_EVENT_PRESS_LOST, this);
    timer = lv_timer_create(settled_cb, kSettleMs, this);
    lv_timer_pause(timer);
  }

  bool blocked() const { return touching || settling; }

  void gate(std::function<void()> apply) {
    if (blocked()) pending = std::move(apply);
    else apply();
  }

  static void release_cb(lv_event_t *e) {
    auto *self = static_cast<HaHold *>(lv_event_get_user_data(e));
    self->touching = false;
    self->settling = true;
    lv_timer_reset(self->timer);
    lv_timer_resume(self->timer);
  }

  static void settled_cb(lv_timer_t *t) {
    auto *self = static_cast<HaHold *>(lv_timer_get_user_data(t));
    lv_timer_pause(t);
    self->settling = false;
    if (self->pending) {
      auto apply = std::move(self->pending);
      self->pending = nullptr;
      apply();
    }
  }
};

}  // namespace tilehaus
