#pragma once
#include "lvgl.h"
#include <cstdio>
#include "card.h"    // CardFonts
#include "clock.h"   // app_clock
#include "esphome/core/time.h"

namespace tilehaus {

// Full-screen clock shown once the panel reaches its sleep stage. Structured
// exactly like splash.h — same layer, same build/show/hide shape — because it
// has the same job: cover everything, including any open modal.
//
// An overlay rather than a page, so it composes over whatever is underneath and
// needs no entry in the page table, which means it can never be navigated to by
// accident.
struct Screensaver {
  lv_obj_t *overlay = nullptr;
  lv_obj_t *time_lbl = nullptr;
  lv_obj_t *date_lbl = nullptr;
};

inline Screensaver &screensaver() {
  static Screensaver s;
  return s;
}

inline void screensaver_update() {
  Screensaver &s = screensaver();
  if (!s.time_lbl || !app_clock()) return;
  esphome::ESPTime now = app_clock()->now();
  if (!now.is_valid()) return;
  char t[8];
  std::snprintf(t, sizeof(t), "%02d:%02d", now.hour, now.minute);
  lv_label_set_text(s.time_lbl, t);
  char d[32];
  now.strftime(d, sizeof(d), "%a %b %e");
  lv_label_set_text(s.date_lbl, d);
}

inline void screensaver_tick_cb(lv_timer_t *) { screensaver_update(); }

inline void screensaver_build(const CardFonts &fonts) {
  Screensaver &s = screensaver();
  if (s.overlay) return;

  s.overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(s.overlay);
  lv_obj_set_size(s.overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(s.overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s.overlay, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s.overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(s.overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s.overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  s.time_lbl = lv_label_create(s.overlay);
  // clock_xl, not value: fonts.value is 55px, sized to fill a tile's value slot,
  // which reads tiny across a 1024x600 screen.
  if (fonts.clock_xl) lv_obj_set_style_text_font(s.time_lbl, fonts.clock_xl, 0);
  lv_obj_set_style_text_color(s.time_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(s.time_lbl, "--:--");

  s.date_lbl = lv_label_create(s.overlay);
  if (fonts.medium) lv_obj_set_style_text_font(s.date_lbl, fonts.medium, 0);
  lv_obj_set_style_text_color(s.date_lbl, lv_color_hex(0xB0B0B0), 0);
  lv_label_set_text(s.date_lbl, "");

  lv_obj_add_flag(s.overlay, LV_OBJ_FLAG_HIDDEN);
  // One timer for the life of the panel; the overlay being hidden is what stops
  // it being seen, not the timer being stopped.
  lv_timer_create(screensaver_tick_cb, 1000, nullptr);
}

inline bool screensaver_showing() {
  Screensaver &s = screensaver();
  return s.overlay && !lv_obj_has_flag(s.overlay, LV_OBJ_FLAG_HIDDEN);
}

inline void screensaver_show() {
  Screensaver &s = screensaver();
  if (!s.overlay) return;
  screensaver_update();          // paint the right time before it is revealed
  lv_obj_clear_flag(s.overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s.overlay);
}

inline void screensaver_hide() {
  Screensaver &s = screensaver();
  if (s.overlay) lv_obj_add_flag(s.overlay, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace tilehaus
