#include <cassert>

#include "idle_state.h"

int main() {
  using tilehaus::IdleConfig;
  using tilehaus::IdleTargets;
  using tilehaus::idle_targets;

  // All zeros is the shipped default: the engine never does anything, however
  // long the panel sits, whatever page it is on.
  {
    IdleConfig cfg;  // page_s=0, dim_s=0, sleep_s=0
    IdleTargets t = idle_targets(cfg, 9999999, 3, false);
    assert(!t.go_home);
    assert(t.brightness_pct == cfg.brightness);
    assert(!t.screensaver);
  }

  // page_timeout alone: returns home, brightness untouched.
  {
    IdleConfig cfg; cfg.page_s = 15;
    assert(!idle_targets(cfg, 14999, 2, false).go_home);
    assert(idle_targets(cfg, 15000, 2, false).go_home);
    assert(idle_targets(cfg, 15000, 2, false).brightness_pct == 100);
  }

  // Already Home with nothing open: no work to do, however idle.
  {
    IdleConfig cfg; cfg.page_s = 15;
    assert(!idle_targets(cfg, 999999, 0, false).go_home);
  }
  // Home but a modal is open: still fires, so the modal gets closed.
  {
    IdleConfig cfg; cfg.page_s = 15;
    assert(idle_targets(cfg, 15000, 0, true).go_home);
    assert(!idle_targets(cfg, 14999, 0, true).go_home);
  }

  // dim_timeout alone.
  {
    IdleConfig cfg; cfg.dim_s = 30; cfg.brightness = 90; cfg.dim_brightness = 20;
    assert(idle_targets(cfg, 29999, 0, false).brightness_pct == 90);
    assert(idle_targets(cfg, 30000, 0, false).brightness_pct == 20);
  }

  // sleep_timeout alone, and it wins over dim once crossed.
  {
    IdleConfig cfg; cfg.dim_s = 30; cfg.sleep_s = 60;
    cfg.brightness = 100; cfg.dim_brightness = 25; cfg.sleep_brightness = 0;
    assert(idle_targets(cfg, 10000, 0, false).brightness_pct == 100);
    assert(idle_targets(cfg, 40000, 0, false).brightness_pct == 25);
    assert(idle_targets(cfg, 70000, 0, false).brightness_pct == 0);
    // The sleep boundary through brightness specifically, not only through the
    // screensaver below: both gate on the same call today, but asserting only
    // one of them would let a future split go unnoticed.
    assert(idle_targets(cfg, 59999, 0, false).brightness_pct == 25);
    assert(idle_targets(cfg, 60000, 0, false).brightness_pct == 0);
  }

  // Misconfiguration: sleep sooner than dim. Sleep still wins once crossed —
  // the later-stage effect takes precedence rather than the larger number.
  {
    IdleConfig cfg; cfg.dim_s = 60; cfg.sleep_s = 30;
    cfg.brightness = 100; cfg.dim_brightness = 25; cfg.sleep_brightness = 5;
    assert(idle_targets(cfg, 40000, 0, false).brightness_pct == 5);
    assert(idle_targets(cfg, 70000, 0, false).brightness_pct == 5);
  }

  // Screensaver follows the sleep stage, and only when enabled.
  {
    IdleConfig cfg; cfg.sleep_s = 60; cfg.screensaver_enabled = true;
    assert(!idle_targets(cfg, 59999, 0, false).screensaver);
    assert(idle_targets(cfg, 60000, 0, false).screensaver);
    cfg.screensaver_enabled = false;
    assert(!idle_targets(cfg, 60000, 0, false).screensaver);
  }
  // A screensaver with no sleep timeout never shows — there is no stage to
  // attach it to.
  {
    IdleConfig cfg; cfg.screensaver_enabled = true;  // sleep_s stays 0
    assert(!idle_targets(cfg, 999999, 0, false).screensaver);
  }

  // All three together, walking the stages.
  {
    IdleConfig cfg; cfg.page_s = 15; cfg.dim_s = 30; cfg.sleep_s = 60;
    cfg.screensaver_enabled = true;
    IdleTargets a = idle_targets(cfg, 20000, 2, false);
    assert(a.go_home && a.brightness_pct == 100 && !a.screensaver);
    IdleTargets b = idle_targets(cfg, 40000, 2, false);
    assert(b.go_home && b.brightness_pct == 25 && !b.screensaver);
    IdleTargets c = idle_targets(cfg, 70000, 2, false);
    assert(c.go_home && c.brightness_pct == 0 && c.screensaver);
  }
  // Presence holds dim and sleep (and so the screensaver), but not return-home.
  {
    IdleConfig cfg; cfg.page_s = 15; cfg.dim_s = 30; cfg.sleep_s = 60;
    cfg.brightness = 80; cfg.screensaver_enabled = true;
    IdleTargets p = idle_targets(cfg, 999999, 2, false, true);
    assert(p.go_home);
    assert(p.brightness_pct == 80);
    assert(!p.screensaver);
    IdleTargets q = idle_targets(cfg, 999999, 2, false, false);
    assert(q.brightness_pct == 0 && q.screensaver);
  }
  // Only "on" is presence; a dropped sensor must not keep the screen lit.
  {
    using tilehaus::presence_state_on;
    assert(presence_state_on("on"));
    assert(!presence_state_on("off"));
    assert(!presence_state_on("unavailable"));
    assert(!presence_state_on("unknown"));
    assert(!presence_state_on(""));
  }
  // Only a touch on a dark screen or the screensaver is a wake. A tap on a lit
  // or dimmed screen must not arm the input guard, or fast taps get dropped.
  {
    using tilehaus::touch_is_wake;
    assert(touch_is_wake(0, false));
    assert(touch_is_wake(100, true));
    assert(!touch_is_wake(100, false));
    assert(!touch_is_wake(25, false));
    assert(!touch_is_wake(-1, false));
  }
  // Wake dimmed: a dark-screen wake comes up at the dim level until the next
  // touch, and only when it is on and the dim level is a usable one.
  {
    IdleConfig cfg; cfg.dim_s = 30; cfg.sleep_s = 60;
    cfg.brightness = 90; cfg.dim_brightness = 20; cfg.wake_dimmed = true;
    assert(idle_targets(cfg, 0, 0, false, false, true).brightness_pct == 20);
    assert(idle_targets(cfg, 0, 0, false, false, false).brightness_pct == 90);
    // Still sleeps on schedule.
    assert(idle_targets(cfg, 60000, 0, false, false, true).brightness_pct == 0);
    cfg.wake_dimmed = false;
    assert(idle_targets(cfg, 0, 0, false, false, true).brightness_pct == 90);
    // A dim level of 0 would leave an awake screen dark and untouchable.
    cfg.wake_dimmed = true; cfg.dim_brightness = 0;
    assert(idle_targets(cfg, 0, 0, false, false, true).brightness_pct == 90);
    // A dim level at or above the awake one is no dimmer.
    cfg.dim_brightness = 95;
    assert(idle_targets(cfg, 0, 0, false, false, true).brightness_pct == 90);
  }
  return 0;
}
