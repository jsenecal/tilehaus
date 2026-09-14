# Inactivity Timeout to Home — Design

**Status:** Design (approved)
**Date:** 2026-09-14
**Closes:** [#2](https://github.com/jsenecal/tilehaus/issues/2) (label scrolling, bundled)

## Problem

A panel left on a subpage stays there. Walk away mid-way through the Lights page
and that is what the next person sees, with a back button pointing at a journey
they did not take. There is no way to say "go back to Home when nobody is using
it", and no inactivity detection in the firmware at all.

## What this adds

A per-deck **home timeout** in seconds. After that much time with no touch, the
panel closes any open modal and returns to Home. `0` disables it, and `0` is the
default — an existing deck behaves exactly as it does today.

Bundled with it: the **second card flags byte** that [#2](https://github.com/jsenecal/tilehaus/issues/2)
needs for per-tile label scrolling. Both require a DECK version bump, and the
bump is the expensive part — spending a whole format revision on one `u16` is
poor value when a specced feature is already waiting on the same change.

## DECK v7

### Header: 22 → 24 bytes

```
 0..3   magic "DECK"
 4..5   version            = 7
 6..7   header size        = 24
 8..11  payload length
12..13  card count
14      page count
15      reserved, must be 0
16..19  accent (int32, -1 = built-in)
20      gridCols
21      gridRows
22..23  home_timeout_s (u16, little-endian)   ← NEW. 0 = disabled.
```

`u16` seconds caps at 65535 (~18 hours), far past anything useful. Seconds
rather than ms because the configurator field is human-facing and 250ms polling
makes sub-second precision meaningless.

### Card body: 16 → 17 fixed bytes

```
 0      type
 1      w
 2      h
 3      flags       (full: hideLabel, detail, followColor, showHilo,
                     transparent, showWeather, showClock, tightMargins)
 4..7   activeColor
 8..11  inactiveColor
12      col
13      row
14      align
15      page
16      flags2      ← NEW. bit0 = scrollLabel. bits 1-7 reserved, write 0.
17..    five length-prefixed strings (unchanged)
```

The card body grows because the existing flags byte has all 8 bits assigned and
the 16 fixed bytes are fully used. There is no spare bit to reclaim.

Cost: 1 byte per card, 128 bytes on a maximum deck, against a 16384-byte budget.

### Compatibility

Both decoders already branch on version for header size; they gain a matching
branch for card stride. A v6 document decodes with `homeTimeoutS = 0` and
`scrollLabel = false`, so **every existing deck keeps working and keeps its
current behaviour**. The encoder always writes v7.

Golden fixtures: the existing `deck_basic.bin`, `deck_v5_pages.bin` and
`deck_v6_grid.bin` stay as they are and must still decode — that is the
compatibility test. A new `deck_v7_timeout.bin` pins the new layout.

## Firmware

### Idle detection

`lv_display_get_inactive_time(NULL)` returns milliseconds since any input device
activity. It covers drags on a slider, not just taps, and needs no touch
tracking of our own.

Polled from the **existing 250ms interval in `hardware.yaml`** — the one that
already enables/disables indevs by backlight state — rather than a new timer.

```
every 250ms:
  if home_timeout_s == 0                      -> nothing (dormant by default)
  if page_nav().current == 0 and no modal     -> nothing
  if lv_display_get_inactive_time() < timeout -> nothing
  else:
      close the active overlay (if any)
      page_nav().stack.clear()
      page_nav().show(0)
```

Clearing the back stack matters: `PageNav::show()` reveals the auto back button
whenever the stack is non-empty, so returning to Home without clearing leaves a
back button on Home pointing at a page the user never navigated from.

### The modal registry

Modals have no central registry today — each card owns its own `Overlay` and
calls `hide()` on it. `overlay.h` gains a module-scope active-overlay pointer:
`show()` records itself, `hide()` clears it if it is the current one.

That is the only way to answer "is a modal open" without inventing a registry
class, and it serves two purposes: closing the modal on timeout, and preventing
the timeout firing while the user is part-way through a light or climate modal
on the Home page.

### Policy in a pure header

The decision lives in a new LVGL-free header so it is host-testable, the same
shape as `tile_scale.h`:

```cpp
// firmware/native/home_timeout.h
inline bool should_return_home(int timeout_s, uint32_t inactive_ms,
                               int current_page, bool modal_open);
```

Returns false when `timeout_s <= 0`, false when already on Home with no modal
open, false when `inactive_ms` is below the threshold, true otherwise.

## Configurator

- A **Home timeout** number field beside the existing accent / grid controls, in
  seconds, with `0` labelled as "never".
- A per-tile **Scroll label** checkbox, which is [#2](https://github.com/jsenecal/tilehaus/issues/2)'s
  deliverable. `add_name` takes a long-mode argument; scroll mode drops the
  `LV_PCT(100)` wrap width and sets `LV_LABEL_LONG_SCROLL`.

The shadow twin in `add_text_shadow` must scroll in lockstep with the label or
it smears. Simplest correct answer: skip the twin entirely in scroll mode, since
a scrolling label is already moving and the depth cue is doing less work.

## Testing

Host tests (`npm run test:firmware`, `npm run test:web`), no device needed:

- `home_timeout_test` (new) — `should_return_home` across: timeout 0, on Home
  with and without a modal, below and above threshold, and the exact boundary.
- `deck_document_test` / `deck.test.ts` — v7 round-trips `homeTimeoutS` and
  `scrollLabel`; the three existing golden fixtures still decode, yielding
  `homeTimeoutS == 0` and `scrollLabel == false`.
- A new `deck_v7_timeout.bin` fixture pins the v7 byte layout on both sides.

On-device (what host tests cannot reach): set a 30s timeout, navigate to Lights,
wait, confirm the panel returns to Home with no back button; open a light modal
and confirm it closes; set 0 and confirm nothing ever happens.

## Risks

**The 250ms poll is shared.** The existing lambda in `hardware.yaml` also drives
indev enable/disable. Adding work there is cheap, but a fault in the new code
takes touch handling with it. Keep the added lambda to a single call into native
code that cannot throw.

**Inactive time and the backlight interact.** Indevs are disabled while the
screen is dark, so LVGL's inactivity clock may not advance the way one expects
during sleep. The feature returning home while dark is harmless — arguably
desirable — but it is the behaviour to check first if something looks wrong.

**`flags2` bits 1-7 are reserved.** They must be written as 0 so a later version
can claim them without another stride change.
