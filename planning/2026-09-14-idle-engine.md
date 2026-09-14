# Idle Engine & Panel Entities — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A three-stage inactivity engine (return Home / dim / sleep) driven by restorable Home Assistant entities, plus a wake button, live accent recolouring, page navigation from HA, and the diagnostic entities the panel currently lacks.

**Architecture:** All settings are ESPHome entities with `restore_value: true`, not DECK fields — per-panel behaviour rather than per-deck layout, so there is **no format change and no compatibility risk**. The decision itself is a pure function in an LVGL-free header, called from the 250ms interval already present in `hardware.yaml`. Native code reaches ESPHome components through hooks, the same pattern the existing reboot-darkening uses.

**Tech Stack:** C++17 headers under `firmware/native/` (namespace `tilehaus`), ESPHome YAML (`number`/`select`/`light`/`button`/`text_sensor`/`binary_sensor` template platforms, API services), LVGL 9, CTest host tests.

**Design:** `planning/2026-09-14-idle-engine-design.md`

---

## File Structure

| File | Change | Responsibility |
|---|---|---|
| `firmware/native/idle_state.h` | **create** | `idle_targets()` — the whole policy, pure, no LVGL. |
| `tests/firmware/idle_state_test.cpp` | **create** | Every threshold, boundary and combination. |
| `tests/firmware/CMakeLists.txt` | modify | Register `idle_state_test`. |
| `firmware/native/screensaver.h` | **create** | Full-screen clock overlay, modelled on `splash.h`. |
| `firmware/native/overlay.h` | modify | Track which overlay is showing. |
| `firmware/native/card.h` | modify | `virtual void restyle()` on `Card`. |
| `firmware/native/panel_runtime.h` | **create** | Hooks + `tick_idle()`, `panel_wake()`, `show_page()`, `set_accent()`. |
| `firmware/native/*_card.h` | modify | `restyle()` on the cards that use the accent. |
| `firmware/panel_entities.yaml` | **create** | All the new ESPHome entities, `!include`d from `tilehaus.yaml`. |
| `firmware/tilehaus.yaml` | modify | Include the new package. |
| `firmware/hardware.yaml` | modify | One call to `tick_idle()`; extend the indev gate. |
| `firmware/bindings.yaml` | modify | Wire the hooks at boot. |

---

## Task 1: The idle policy, pure and host-tested

**Files:**
- Create: `firmware/native/idle_state.h`
- Create: `tests/firmware/idle_state_test.cpp`
- Modify: `tests/firmware/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/firmware/idle_state_test.cpp`:

```cpp
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
```

- [ ] **Step 2: Register it and watch it fail**

In `tests/firmware/CMakeLists.txt` replace:
```cmake
foreach(t grid_layout sensor_format slider_map toggle_state tile_scale forecast_helpers)
```
with:
```cmake
foreach(t grid_layout sensor_format slider_map toggle_state tile_scale forecast_helpers idle_state)
```

Run: `npm run test:firmware`
Expected: FAIL — `idle_state.h: No such file or directory`.

- [ ] **Step 3: Implement**

Create `firmware/native/idle_state.h`:

```cpp
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
```

- [ ] **Step 4: Run the tests**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 8`.

- [ ] **Step 5: Commit**

```bash
git add firmware/native/idle_state.h tests/firmware/idle_state_test.cpp tests/firmware/CMakeLists.txt
git commit -m "feat: idle policy as a pure, host-tested function

Three independent thresholds composing into a target, rather than a
state machine with transitions — there is then no edge to get wrong when
a threshold is crossed twice, skipped, or reconfigured mid-idle."
```

---

## Task 2: Know which overlay is showing

Each card owns its own `Overlay` and calls `hide()` on it; nothing tracks which, if any, is up. The engine needs that to close a modal on timeout and to avoid firing mid-interaction.

**Files:**
- Modify: `firmware/native/overlay.h`

- [ ] **Step 1: Add the registry**

In `firmware/native/overlay.h`, directly after `namespace tilehaus {`, add:

```cpp
struct Overlay;

// The overlay currently showing, or nullptr. Modals are owned by individual
// cards with no registry between them, so this is the only way to ask "is a
// modal open" — which the idle engine needs both to close one and to avoid
// firing while the user is part-way through it.
inline Overlay *&active_overlay() {
  static Overlay *o = nullptr;
  return o;
}
```

- [ ] **Step 2: Record and clear it**

Replace:
```cpp
  void show() {
    if (!root_) return;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root_);
  }
```
with:
```cpp
  void show() {
    if (!root_) return;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root_);
    active_overlay() = this;
  }
```

Replace:
```cpp
  void hide() {
    if (!root_) return;
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
  }
```
with:
```cpp
  void hide() {
    if (!root_) return;
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    // Only clear it if we are the one showing — a modal hiding itself after
    // another has already opened must not blank the pointer.
    if (active_overlay() == this) active_overlay() = nullptr;
  }
```

- [ ] **Step 3: Confirm nothing else broke**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 8`. `overlay.h` includes LVGL and is not host-compiled; the real check is the compile in Task 8.

- [ ] **Step 4: Commit**

```bash
git add firmware/native/overlay.h
git commit -m "feat: track which overlay is showing

Modals are per-card with no registry, so there was no way to ask whether
one is open."
```

---

## Task 3: The screensaver overlay

**Files:**
- Create: `firmware/native/screensaver.h`

Model it on `firmware/native/splash.h` — read that file first. Same shape: a struct, a singleton accessor, `build`/`show`/`hide`, and `lv_obj_create(lv_layer_top())` so it sits above both the grid and any modal.

- [ ] **Step 1: Write it**

Create `firmware/native/screensaver.h`:

```cpp
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
  const auto now = app_clock()->now();
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
  if (fonts.value) lv_obj_set_style_text_font(s.time_lbl, fonts.value, 0);
  lv_obj_set_style_text_color(s.time_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(s.time_lbl, "--:--");

  s.date_lbl = lv_label_create(s.overlay);
  if (fonts.body) lv_obj_set_style_text_font(s.date_lbl, fonts.body, 0);
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
```

- [ ] **Step 2: Confirm host tests still pass**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 8`. Not host-compiled; Task 8 is the real check.

- [ ] **Step 3: Commit**

```bash
git add firmware/native/screensaver.h
git commit -m "feat: full-screen clock screensaver

Same layer and lifecycle as splash.h so it covers the grid and any open
modal, and an overlay rather than a page so it cannot be navigated to."
```

---

## Task 4: `restyle()` for live accent changes

**Files:**
- Modify: `firmware/native/card.h`
- Modify: the cards that resolve an accent-derived colour

- [ ] **Step 1: Add the virtual**

In `firmware/native/card.h`, inside `struct Card`, after `virtual void bind(const CardConfig &cfg) = 0;` add:

```cpp
  // Re-resolve colours against the current deck accent, without rebuilding.
  // Rebuilding is not an option: HA state subscriptions cannot be torn down at
  // runtime, so a rebuild would duplicate every subscription. Re-theming an
  // existing widget has no such problem.
  virtual void restyle() {}
```

- [ ] **Step 2: Find the cards that need it**

```bash
grep -rn "tile_on_color()" firmware/native/*.h
```

Every card whose `build()` assigns a member from `tile_on_color()` needs a `restyle()` that re-runs that assignment and repaints. For a card that stores `on_color_` and recolours through an observer, the body is:

```cpp
  void restyle() override {
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    lv_subject_set_int(&on_, lv_subject_get_int(&on_));  // re-fire the observer
  }
```

That requires the card to have kept `cfg.active_color`. Where a card does not already store it, add a member `int32_t cfg_active_color_ = -1;` set in `build()`.

Implement `restyle()` for each card the grep reports. Report the list you found and what you did for each.

- [ ] **Step 3: Confirm host tests still pass**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 8`.

- [ ] **Step 4: Commit**

```bash
git add firmware/native/
git commit -m "feat: cards can restyle against a changed accent

Re-theming rather than rebuilding, because HA subscriptions cannot be
torn down at runtime."
```

---

## Task 5: The runtime surface native code exposes

**Files:**
- Create: `firmware/native/panel_runtime.h`

- [ ] **Step 1: Write it**

Create `firmware/native/panel_runtime.h`:

```cpp
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
// Home Assistant instead of DECK header fields. bindings.yaml writes them
// whenever an entity changes.
inline IdleConfig &idle_config() {
  static IdleConfig cfg;
  return cfg;
}

// Native code cannot drive an ESPHome light, so the YAML supplies the action.
// Same shape as the existing pre_reboot_hook.
inline std::function<void(int)> &set_brightness_hook() {
  static std::function<void(int)> hook;
  return hook;
}
// Publishes the current page name so a text_sensor can report it.
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
```

- [ ] **Step 2: Confirm host tests still pass**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 8`.

- [ ] **Step 3: Commit**

```bash
git add firmware/native/panel_runtime.h
git commit -m "feat: panel runtime surface for the idle engine and HA entities"
```

---

## Task 6: The ESPHome entities

**Files:**
- Create: `firmware/panel_entities.yaml`
- Modify: `firmware/tilehaus.yaml`

Read `firmware/hardware.yaml` first for the house style, and note that `tilehaus.yaml` assembles packages with `!include`.

- [ ] **Step 1: Write the entity package**

Create `firmware/panel_entities.yaml`:

```yaml
# Panel behaviour, exposed to Home Assistant rather than stored in the deck.
#
# These are per-panel settings, not per-deck layout: how long before the screen
# dims is a property of this panel in this room, and wanting to change it should
# not mean re-encoding a deck and rebooting. restore_value keeps them across
# reboots, and OTA preserves NVS, so they survive reflashes too.
#
# Every timeout defaults to 0 (disabled), so a freshly flashed panel behaves
# exactly as it did before this existed.

number:
  - platform: template
    name: "Page timeout"
    id: cfg_page_timeout
    entity_category: config
    unit_of_measurement: s
    min_value: 0
    max_value: 3600
    step: 5
    initial_value: 0
    restore_value: true
    optimistic: true
    icon: mdi:home-clock
    on_value:
      - lambda: "tilehaus::idle_config().page_s = (int) x;"

  - platform: template
    name: "Dim timeout"
    id: cfg_dim_timeout
    entity_category: config
    unit_of_measurement: s
    min_value: 0
    max_value: 3600
    step: 5
    initial_value: 0
    restore_value: true
    optimistic: true
    icon: mdi:brightness-4
    on_value:
      - lambda: "tilehaus::idle_config().dim_s = (int) x;"

  - platform: template
    name: "Sleep timeout"
    id: cfg_sleep_timeout
    entity_category: config
    unit_of_measurement: s
    min_value: 0
    max_value: 3600
    step: 5
    initial_value: 0
    restore_value: true
    optimistic: true
    icon: mdi:sleep
    on_value:
      - lambda: "tilehaus::idle_config().sleep_s = (int) x;"

  - platform: template
    name: "Brightness"
    id: cfg_brightness
    entity_category: config
    unit_of_measurement: "%"
    min_value: 0
    max_value: 100
    step: 1
    initial_value: 100
    restore_value: true
    optimistic: true
    icon: mdi:brightness-7
    on_value:
      - lambda: "tilehaus::idle_config().brightness = (int) x;"

  - platform: template
    name: "Dim brightness"
    id: cfg_dim_brightness
    entity_category: config
    unit_of_measurement: "%"
    min_value: 0
    max_value: 100
    step: 1
    initial_value: 25
    restore_value: true
    optimistic: true
    icon: mdi:brightness-5
    on_value:
      - lambda: "tilehaus::idle_config().dim_brightness = (int) x;"

  - platform: template
    name: "Sleep brightness"
    id: cfg_sleep_brightness
    entity_category: config
    unit_of_measurement: "%"
    min_value: 0
    max_value: 100
    step: 1
    initial_value: 0
    restore_value: true
    optimistic: true
    icon: mdi:brightness-2
    on_value:
      - lambda: "tilehaus::idle_config().sleep_brightness = (int) x;"

select:
  - platform: template
    name: "Screensaver"
    id: cfg_screensaver
    entity_category: config
    options: ["None", "Clock"]
    initial_option: "None"
    restore_value: true
    optimistic: true
    icon: mdi:clock-digital
    on_value:
      - lambda: 'tilehaus::idle_config().screensaver_enabled = (x == "Clock");'

light:
  # A real RGB light so Home Assistant renders its colour wheel. entity_category
  # config marks it as a setting rather than a lamp; the panel's existing
  # backlight light is already excluded from the room's light group, so this does
  # not risk being swept into "turn everything off".
  #
  # OFF means "use the accent the deck itself specifies"; ON overrides it live.
  - platform: partition
    name: "Accent"
    id: cfg_accent
    entity_category: config
    segments: []
    internal: false

button:
  - platform: template
    name: "Wake"
    id: btn_wake
    icon: mdi:gesture-tap
    on_press:
      - lambda: "tilehaus::panel_wake();"

  - platform: restart
    name: "Restart"
    entity_category: diagnostic

binary_sensor:
  - platform: template
    name: "In use"
    id: sens_in_use
    device_class: occupancy
    lambda: |-
      const int dim_s = tilehaus::idle_config().dim_s;
      if (dim_s <= 0) return true;   // never dims, so always "in use"
      return lv_display_get_inactive_time(nullptr) < (uint32_t) dim_s * 1000u;

text_sensor:
  - platform: template
    name: "Current page"
    id: sens_current_page
    entity_category: diagnostic
    icon: mdi:page-layout-header

sensor:
  - platform: uptime
    name: "Uptime"
    entity_category: diagnostic
```

**Note on the accent light:** `platform: partition` above is a placeholder that will not work. ESPHome has no built-in "virtual RGB light" platform. Use a **`light: - platform: rgb`** backed by three dummy `output: - platform: template` float outputs whose `write_action` does nothing but record the channel, then call `tilehaus::set_accent()` from the light's `on_state`. Work out the correct shape against the installed ESPHome version (`.venv-esphome/bin/esphome version`), verify with `esphome config`, and report what you used. Do not guess — validate.

- [ ] **Step 2: Include the package**

In `firmware/tilehaus.yaml`, in the `packages:` block, add:
```yaml
  entities: !include panel_entities.yaml
```

- [ ] **Step 3: Validate**

Run: `.venv-esphome/bin/esphome config firmware/tilehaus.yaml > /dev/null && echo OK`
Expected: `OK`. Iterate on the accent light until this passes.

- [ ] **Step 4: Commit**

```bash
git add firmware/panel_entities.yaml firmware/tilehaus.yaml
git commit -m "feat: expose panel behaviour as Home Assistant entities"
```

---

## Task 7: Wire it together

**Files:**
- Modify: `firmware/bindings.yaml`
- Modify: `firmware/hardware.yaml`

- [ ] **Step 1: Wire the hooks at boot**

Read `firmware/bindings.yaml` first and match its actual variable names. In the `on_boot` lambda, after the existing hook assignments, add:

```cpp
tilehaus::set_brightness_hook() = [](int pct) {
  auto call = id(display_backlight).turn_on();
  call.set_brightness(pct / 100.0f);
  call.set_transition_length(300);
  call.perform();
};
tilehaus::page_report_hook() = [](const std::string &name) {
  id(sens_current_page).publish_state(name);
};
tilehaus::screensaver_build(poc_fonts);
```

- [ ] **Step 2: Add the show-page service**

In `firmware/bindings.yaml`, under the existing `api:` block, add:

```yaml
  services:
    - service: show_page
      variables:
        page: string
      then:
        - lambda: "tilehaus::show_page(page);"
```

If `api:` lives in another package file, put it beside the existing `api:` definition rather than creating a second one. Report where you put it.

- [ ] **Step 3: Call the tick, and extend the indev gate**

In `firmware/hardware.yaml`, find the 250ms interval lambda that enables/disables indevs. Two changes:

Replace the `bright` computation so the screensaver also gates input:
```cpp
          bool bright = v.is_on() && v.get_brightness() > 0.0f;
```
with:
```cpp
          // The screensaver swallows the first touch for the same reason a dark
          // screen does: that touch is a wake, not a tap on whatever is beneath.
          bool bright = v.is_on() && v.get_brightness() > 0.0f &&
                        !tilehaus::screensaver_showing();
```

And append to the end of the same lambda:
```cpp
          tilehaus::tick_idle();
```

- [ ] **Step 4: Wake on touch**

In the same file, in the existing `on_touch:` block, replace the whole `if/then` with:

```yaml
    on_touch:
      - lambda: "tilehaus::panel_touch_wake();"
```

`panel_touch_wake()` restores the configured brightness rather than a hardcoded 100%, hides the screensaver, and resets LVGL's inactivity clock — which the old handler did not do and which is what makes waking work at all while indevs are disabled.

- [ ] **Step 5: Validate**

Run: `.venv-esphome/bin/esphome config firmware/tilehaus.yaml > /dev/null && echo OK`
Expected: `OK`.

- [ ] **Step 6: Commit**

```bash
git add firmware/bindings.yaml firmware/hardware.yaml
git commit -m "feat: drive the idle engine from the existing 250ms poll"
```

---

## Task 8: Build, flash, verify

**Files:** none modified.

- [ ] **Step 1: Full host verification**

```bash
npm run typecheck && npm run test:web && npm run test:firmware && npm run build
```
Expected: typecheck silent, web `pass 8 / fail 0`, firmware `100% tests passed out of 8`, bundle written.

- [ ] **Step 2: Compile**

```bash
.venv-esphome/bin/esphome compile firmware/tilehaus.yaml 2>&1 | tail -5
```
Expected: `INFO Successfully compiled program.` This is the first real check of Tasks 2-7 — none of those files are host-compiled.

- [ ] **Step 3: Flash**

```bash
.venv-esphome/bin/esphome upload firmware/tilehaus.yaml --device 192.168.252.221 2>&1 | tail -4
```
Expected: `INFO OTA successful`.

- [ ] **Step 4: Confirm the deck survived**

```bash
until curl -s -m 3 -o /dev/null http://192.168.252.221/api/v1/config; do sleep 3; done
node scripts/deck_cli.js status --device 192.168.252.221 | head -2
```
Expected: the stored deck still decodes with its pages and tile count. No format changed, so this should be untouched — if it is not, stop and investigate before going further.

- [ ] **Step 5: Verify on the glass**

Wait 60s after the OTA before anything else (safe-mode rollback risk). Then, from Home Assistant:

| check | expected |
|---|---|
| the new entities appear | timeouts, brightnesses, screensaver, accent, wake, restart, in-use, current page, uptime |
| set page 15 / dim 30 / sleep 60, sit on a subpage | returns Home at 15s, dims at 30s, sleeps + clock at 60s |
| touch while dimmed | full brightness, and the tap works normally |
| touch while asleep or on the screensaver | wakes, and does **not** press a tile |
| press `button.wake` | bright, screensaver gone, back on Home |
| call `esphome.tilehaus_show_page` with `page: Lights` | panel navigates; `current page` reports Lights |
| turn `light.accent` on, pick a colour | tiles recolour with no reboot |
| turn `light.accent` off | reverts to the deck's accent |
| set all three timeouts to 0 | nothing ever happens |

- [ ] **Step 6: Commit any tuning**

```bash
git add -A firmware
git commit -m "fix: tune the idle engine against the panel"
```

---

## Self-Review Notes

**Spec coverage.** Design "Entities" → Task 6. "The engine is a pure function" → Task 1. "Waking" → Tasks 5, 7. "Live accent without a rebuild" → Tasks 4, 5, 6. "Screensaver" → Task 3. "Testing" → Task 1 (host) and Task 8 (device). The modal registry the engine depends on → Task 2.

**Type consistency.** `IdleConfig` / `IdleTargets` / `idle_targets()` / `idle_stage_reached()`; `idle_config()`, `set_brightness_hook()`, `page_report_hook()`, `apply_brightness()`, `report_page()`, `go_home()`, `tick_idle()`, `panel_wake()`, `panel_touch_wake()`, `show_page()`, `set_accent()`; `screensaver_build/show/hide/showing/update()`; `active_overlay()`; `Card::restyle()`. Each defined once and spelled identically at every use.

**Known gaps and deliberate unknowns.**
- Everything except `idle_state.h` is LVGL- or ESPHome-bound and therefore only proven by the compile in Task 8.
- Task 6's accent light is the one piece written as a *problem to solve* rather than code to copy: ESPHome has no virtual-RGB-light platform, and the right shape depends on the installed version. The task says so, forbids guessing, and requires `esphome config` to pass.
- Task 4 cannot list the cards to change without grepping, because the set is whatever currently calls `tile_on_color()`. The task makes the grep the first step and asks for the list back.
