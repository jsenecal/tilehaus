#pragma once
#include <cstdint>

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
};

struct IdleTargets {
  bool go_home = false;       // close any modal, clear the stack, show page 0
  int brightness_pct = 100;
  bool screensaver = false;
};

inline bool idle_stage_reached(int timeout_s, uint32_t inactive_ms) {
  if (timeout_s <= 0) return false;
  return inactive_ms >= static_cast<uint32_t>(timeout_s) * 1000u;
}

inline IdleTargets idle_targets(const IdleConfig &cfg, uint32_t inactive_ms,
                                int current_page, bool modal_open) {
  IdleTargets t;
  t.brightness_pct = cfg.brightness;

  // Nothing to return to when already Home with nothing open, however idle.
  const bool somewhere_to_return_from = current_page != 0 || modal_open;
  t.go_home = somewhere_to_return_from && idle_stage_reached(cfg.page_s, inactive_ms);

  // Sleep is applied after dim, so it wins when both are crossed — including the
  // misconfigured case where sleep_s < dim_s. The later stage should win on
  // effect, not on whichever number happens to be larger.
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
