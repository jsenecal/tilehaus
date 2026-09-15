# Idle Engine & Panel Entities — Design

**Status:** Design (approved)
**Date:** 2026-09-14
**Supersedes:** `2026-09-14-home-timeout-design.md` and its plan

## Problem

The panel has no idea whether anyone is in the room. It sits at full brightness
all night, stays on whatever subpage it was left on, and Home Assistant can
neither wake it nor find out what it is showing.

It also exposes almost nothing: two entities, a backlight and a Wi-Fi RSSI. The
NSPanel in the same office exposes about fifteen. There is no restart button.

## The principle this design turns on

> **Layout belongs in DECK. Behaviour belongs in HA entities.**

A tile's position, colour binding and entity describe *what the deck is* — per
deck, travelling with the layout, changed deliberately in the configurator. How
long before the panel dims, how bright it gets, what page it is showing describe
*how this panel behaves right now* — per panel, tunable, automatable.

An earlier draft put the timeouts in the DECK header. That was the wrong side of
the line, and it cost a format bump, a reboot per adjustment, and compatibility
risk against the v6 deck already stored on the device. Moving them to HA entities
removes all three.

**Consequence: this design needs no DECK format change at all.** `scrollLabel`
([#2](https://github.com/jsenecal/tilehaus/issues/2)) still wants a v7 bump, but
it is genuinely per-tile and so genuinely layout. It decouples into its own
project.

## Entities

All `restore_value: true`, so they survive reboots and — because OTA preserves
NVS, as `hardware.yaml:63` already notes — reflashes too.

| entity | range | default | purpose |
|---|---|---|---|
| `number.page_timeout` | 0–3600 s | 0 | idle → close modal, return to Home |
| `number.dim_timeout` | 0–3600 s | 0 | idle → `dim_brightness` |
| `number.sleep_timeout` | 0–3600 s | 0 | idle → `sleep_brightness` + screensaver |
| `number.brightness` | 0–100 % | 100 | the awake level; touch always returns here |
| `number.dim_brightness` | 0–100 % | 25 | |
| `number.sleep_brightness` | 0–100 % | 0 | |
| `select.screensaver_mode` | None / Clock | None | `Image` reserved for a later project |
| `light.accent` | RGB, `entity_category: config` | off | off = DECK accent, on = live override |
| `text_sensor.current_page` | — | Home | read-only: what the panel is showing |
| `esphome.tilehaus_show_page` | service, `page: <name>` | — | write: HA navigates the panel |
| `button.wake` | — | — | brightness, screensaver off, modal closed, Home |
| `button.restart` | — | — | ESPHome built-in; the panel currently has none |
| `binary_sensor.panel_in_use` | — | off | on while `inactive_ms` is below `dim_timeout` |
| uptime / free-heap sensors | — | — | ESPHome built-ins, diagnostic category |

Every timeout is independently disabled with `0`, and `0` is the default for all
three — a panel that is flashed and never configured behaves exactly as it does
today.

Page navigation is the pair that changes what automations can do: with
`button.wake`, a doorbell can wake the panel *and* put the camera page on it.
Today HA cannot even tell what is on screen.

It is split into a read-only sensor plus a service rather than a single `select`
for a concrete reason: a select's options are declared at compile time, but the
page names come from the deck at runtime (`card_host.h:124` sets
`nav.names = pages`). A select could therefore never track a deck it had not yet
loaded. An ESPHome API service takes a free-form argument and works for any deck,
and the sensor reports the truth regardless of how navigation happened — touch,
service, or timeout.

## The engine is a pure function

The three timeouts are not a ladder with transitions — they are independent
thresholds whose effects compose. At 20s the panel is home at full brightness; at
40s home and dimmed; at 700s home, dark, showing a clock. Modelling that as a
state machine invites bugs at every edge; computing a *target* from elapsed time
does not.

```cpp
// firmware/native/idle_state.h — no LVGL, no ESPHome, host-tested
struct IdleConfig {
  int page_s = 0, dim_s = 0, sleep_s = 0;
  int brightness = 100, dim_brightness = 25, sleep_brightness = 0;
  bool screensaver_enabled = false;
};
struct IdleTargets {
  bool go_home = false;      // close modal, clear stack, show page 0
  int brightness_pct = 100;  // what the backlight should be right now
  bool screensaver = false;  // whether the clock overlay should show
};

IdleTargets idle_targets(const IdleConfig &cfg, uint32_t inactive_ms,
                         int current_page, bool modal_open);
```

Idempotent, so the existing 250ms poll calls it every tick and acts only on what
differs. Every boundary — which threshold wins when `dim_s > sleep_s`, what `0`
means per field, the exact millisecond of each crossing — is testable without
hardware. `go_home` is suppressed when already on Home with no modal open, so the
common case costs nothing.

## Waking

Most of it falls out for free. A touch resets LVGL's inactivity clock, so
`inactive_ms` drops to ~0, `idle_targets()` returns the awake brightness and no
screensaver, and the next tick applies it. **No dedicated wake path is needed for
the dim case.**

What needs care is whether that first touch also presses a tile:

| stage | screen | first touch |
|---|---|---|
| dimmed | readable | wakes **and** acts — you can see what you are tapping |
| asleep | dark | wakes only, swallowed |
| screensaver | clock showing | dismisses it only, swallowed |

`hardware.yaml` already implements exactly this for the dark case: it disables
LVGL's input devices while the backlight is off. That condition extends to cover
"screensaver showing".

The catch: while indevs are disabled LVGL never sees the touch, so its inactivity
clock never resets and the panel would never wake. The escape is that ESPHome's
`on_touch` fires at the touchscreen level, below LVGL, and already runs today:

```
on_touch:
  lv_display_trigger_activity(NULL)   # reset the idle clock
  brightness  = number.brightness
  screensaver hidden
```

Touch never returns to Home — only `button.wake` does. Yanking the page out from
under someone walking up mid-task would be worse than leaving it.

## Live accent without a rebuild

Cards resolve their colours in `build()`: `tile_on_color()` is read once when a
tile is constructed, so changing `deck_accent()` afterwards does nothing. The
obvious fix — rebuild the pages — is closed off, because `CLAUDE.md` records that
config applies by reboot precisely because "HA state subscriptions can't be torn
down at runtime". ESPHome's API has no unsubscribe; rebuilding would duplicate
every subscription.

But re-theming is not rebuilding. The tiles already exist and only their colours
are stale:

```cpp
struct Card {
  virtual void restyle() {}   // re-resolve colours against the current accent
};

// on accent change:
deck_accent() = rgb;
for (auto &c : live_cards()) c->restyle();
```

No teardown, no subscriptions touched, no reboot. Bounded to the cards that call
`tile_on_color()` — which a recent change unified into one function, so they are
trivially findable.

`light.accent` uses the entity's own semantics honestly: **off** means fall back
to the DECK accent (the configurator's value stays the baseline), **on** means
override live with the chosen colour.

An RGB `light` was initially rejected here on the theory that it would be swept
into area light groups and turned off with the room. That was checked and is
false: `light.office_lights` contains six lamps and does **not** include the
panel's existing `light.…_display_backlight`, despite it being an ESPHome light
on a device in the Office area. `entity_category: config` is set anyway, to mark
intent and keep it out of auto-generated dashboard sections.

## Screensaver

A full-screen LVGL overlay on `lv_layer_top()` showing a large clock and date,
modelled directly on `splash.h` — the same singleton accessor plus
`build`/`show`/`hide` shape, and the same layer, so it sits above both the grid
and any modal. It reuses `app_clock()` from `clock.h`, which the header card
already drives on a 1s `lv_timer`.

An overlay rather than a page: it composes over whatever is underneath and needs
no entry in the page table, so it cannot be navigated to by accident.

## Testing

Host tests, no device required:

- `idle_state_test` (new) — every threshold alone and combined: all zeros, each
  stage in isolation, all three together, `dim_s > sleep_s`, exact boundary
  milliseconds, and the already-home suppression.
- Existing suites must stay green; this design touches no codec, so the deck
  fixtures and their compatibility guarantees are untouched.

On-device, which is the only place most of this is observable: set
`page 15 / dim 30 / sleep 60` from HA, navigate to a subpage, watch each stage
arrive; confirm touch restores brightness without pressing a tile; confirm
`button.wake` returns to Home; drive `select.current_page` from HA and watch the
panel follow; change `light.accent` and watch tiles recolour without a reboot;
set all three timeouts to `0` and confirm nothing ever happens.

## Risks

**The 250ms poll is shared.** The lambda in `hardware.yaml` also drives indev
enable/disable. A fault in the new code takes touch handling with it — keep the
addition to a single call into native code that cannot throw.

**Brightness fights the user.** Setting the backlight from HA directly will be
overwritten by the next tick. That is correct for an idle engine and surprising
in practice; worth judging on the glass before deciding whether the engine should
yield to a manual override.

**Navigation from HA must not fight the engine.** Calling the show-page service
navigates the panel but resets nothing, so a page selected from HA while the
panel is idle would be returned Home by the very next `page_timeout` crossing.
The service must trigger activity, exactly as `button.wake` does.

**Entity count.** This adds a dozen entities to an HA instance that already has
several thousand. Diagnostics get `entity_category: diagnostic` and the settings
get `config`, so neither clutters the default dashboard.
