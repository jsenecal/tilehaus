#pragma once
#include "lvgl.h"
#include <cstdint>
#include <functional>
#include <string>
#include "card.h"
#include "card_config.h"
#include "card_host.h"
#include "idle_state.h"
#include "overlay.h"
#include "page_nav.h"
#include "screensaver.h"

namespace tilehaus {

// Live settings, owned by ESPHome entities rather than the deck: these are
// per-panel behaviour, not per-deck layout, so they are restorable numbers in
// Home Assistant instead of DECK header fields. panel_entities.yaml writes them
// whenever an entity changes.
inline IdleConfig &idle_config() {
  static IdleConfig cfg;
  return cfg;
}

// Native code cannot drive an ESPHome light or publish to a text_sensor, so the
// YAML supplies the actions. Same shape as the existing pre_reboot_hook.
inline std::function<void(int)> &set_brightness_hook() {
  static std::function<void(int)> hook;
  return hook;
}
inline std::function<void(const std::string &)> &page_report_hook() {
  static std::function<void(const std::string &)> hook;
  return hook;
}

inline void apply_brightness(int pct) {
  static int last = -1;
  if (pct == last) return;        // the poll runs 4x a second; only act on change
  last = pct;
  if (set_brightness_hook()) set_brightness_hook()(pct);
}

inline void report_page() {
  static int last = -1;
  if (page_nav().current == last) return;
  last = page_nav().current;
  if (page_report_hook() && last >= 0 &&
      last < static_cast<int>(page_nav().names.size())) {
    page_report_hook()(page_nav().names[last]);
  }
}

inline void go_home() {
  if (active_overlay()) active_overlay()->hide();
  // Clear the stack as well as showing Home: PageNav::show() reveals the auto
  // back button whenever the stack is non-empty, so returning without clearing
  // leaves a back button on Home pointing at a page nobody navigated from.
  page_nav().stack.clear();
  page_nav().show(0);
}

// Called from the 250ms interval in hardware.yaml.
inline void tick_idle() {
  const IdleTargets t = idle_targets(idle_config(),
                                     lv_display_get_inactive_time(nullptr),
                                     page_nav().current,
                                     active_overlay() != nullptr);
  if (t.go_home) go_home();
  apply_brightness(t.brightness_pct);
  if (t.screensaver != screensaver_showing()) {
    if (t.screensaver) screensaver_show(); else screensaver_hide();
  }
  report_page();
}

// button.wake: put the panel into a known state for someone about to walk up.
inline void panel_wake() {
  lv_display_trigger_activity(nullptr);   // without this the next tick re-sleeps it
  screensaver_hide();
  go_home();
  apply_brightness(idle_config().brightness);
  report_page();
}

// on_touch: wake without navigating. Touch must never move the page out from
// under someone mid-task — only button.wake does that.
inline void panel_touch_wake() {
  lv_display_trigger_activity(nullptr);
  screensaver_hide();
  apply_brightness(idle_config().brightness);
}

// The show-page API service. Counts as activity, or an idle panel would be
// returned Home by the very next page_timeout crossing.
inline void show_page(const std::string &name) {
  lv_display_trigger_activity(nullptr);
  screensaver_hide();
  apply_brightness(idle_config().brightness);
  const int i = page_nav().index_of(name);
  if (i >= 0) page_nav().show(i);
  report_page();
}

// light.accent: off falls back to the deck's own accent, on overrides it live.
inline void set_accent(bool on, uint32_t rgb) {
  static int32_t deck_default = -2;       // -2 = not yet captured
  if (deck_default == -2) deck_default = deck_accent();
  deck_accent() = on ? static_cast<int32_t>(rgb) : deck_default;
  for (auto &c : live_cards()) c->restyle();
}

}  // namespace tilehaus
