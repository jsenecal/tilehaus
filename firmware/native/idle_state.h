#pragma once
#include <cstdint>
#include <string>

namespace tilehaus {

// What the panel should look like after a given stretch of inactivity.
//
// The three timeouts are independent thresholds, not a state machine: each one
// crossed contributes its own effect, and they compose. Computing a target from
// elapsed time — rather than tracking transitions — means there is no edge to
// get wrong when a threshold is crossed twice, skipped, or reconfigured while
// the panel is already idle.
//
// Pure on purpose: no LVGL, no ESPHome, no globals. Every boundary here is
// host-tested, and the caller is left with nothing but plumbing.
struct IdleConfig {
  int page_s = 0;             // 0 disables this stage; 0 is the shipped default
  int dim_s = 0;
  int sleep_s = 0;
  int brightness = 100;       // percent, the awake level
  int dim_brightness = 25;
  int sleep_brightness = 0;
  bool screensaver_enabled = false;
  // A touch on a dark screen brings it up at dim_brightness rather than full;
  // the next touch goes to full. Keeps a night-time wake from lighting the room.
  bool wake_dimmed = false;
};

struct IdleTargets {
  bool go_home = false;       // close any modal, clear the stack, show page 0
  int brightness_pct = 100;
  bool screensaver = false;
};

// A presence sensor counts as present only when HA reports it "on":
// "unavailable" and "unknown" must not pin the screen awake all night.
inline bool presence_state_on(const std::string &state) { return state == "on"; }

// Whether a touch is waking the panel, judged from the screen as it was before
// the touch: dark (brightness 0) or showing the screensaver. Only a wake arms
// the input guard. A touch on a screen already lit, dimmed included, is a tap,
// and guarding it would switch input off between press and release and drop
// the click. A negative brightness means none applied yet: lit at boot.
inline bool touch_is_wake(int applied_brightness_pct, bool screensaver_up) {
  return screensaver_up || applied_brightness_pct == 0;
}

inline bool idle_stage_reached(int timeout_s, uint32_t inactive_ms) {
  if (timeout_s <= 0) return false;
  return inactive_ms >= static_cast<uint32_t>(timeout_s) * 1000u;
}

// `woke_dimmed` is set by a touch that woke a dark screen with wake_dimmed on,
// and cleared by the next touch: until then the awake level is the dim one.
// `present` is the optional presence sensor (see presence_wake in
// panel_runtime.h): while someone is in the room the screen stays awake, so the
// dim and sleep stages hold. Return-home still applies; being nearby is not the
// same as being mid-task.
inline IdleTargets idle_targets(const IdleConfig &cfg, uint32_t inactive_ms,
                                int current_page, bool modal_open,
                                bool present = false,
                                bool woke_dimmed = false,
                                bool stays_open = false) {
  IdleTargets t;
  t.brightness_pct = cfg.brightness;
  // Never dim to 0 here: an awake screen at 0 takes no touch, so it could
  // never be brought back up.
  if (woke_dimmed && cfg.wake_dimmed && cfg.dim_brightness > 0 &&
      cfg.dim_brightness < cfg.brightness) {
    t.brightness_pct = cfg.dim_brightness;
  }

  // Nothing to return to when already Home with nothing open, however idle.
  // A view meant to be left up (the Media card's full view) holds the panel
  // where it is: no return Home, though it still dims and sleeps.
  const bool somewhere_to_return_from = current_page != 0 || modal_open;
  t.go_home = somewhere_to_return_from && !stays_open && idle_stage_reached(cfg.page_s, inactive_ms);

  // Sleep is applied after dim, so it wins when both are crossed — including the
  // misconfigured case where sleep_s < dim_s. The later stage should win on
  // effect, not on whichever number happens to be larger.
  if (present) return t;
  if (idle_stage_reached(cfg.dim_s, inactive_ms)) t.brightness_pct = cfg.dim_brightness;
  if (idle_stage_reached(cfg.sleep_s, inactive_ms)) {
    t.brightness_pct = cfg.sleep_brightness;
    // The screensaver hangs off the sleep stage, so it cannot show when there is
    // no sleep timeout to reach.
    t.screensaver = cfg.screensaver_enabled;
  }
  return t;
}

}  // namespace tilehaus
