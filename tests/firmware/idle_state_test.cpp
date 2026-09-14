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
  return 0;
}
