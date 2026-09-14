# Idle Engine — Design

**Status:** Design (approved)
**Date:** 2026-09-14
**Supersedes:** `2026-09-14-home-timeout-design.md`
**Closes:** [#2](https://github.com/jsenecal/tilehaus/issues/2) (label scrolling, bundled)

## Problem

The panel has no idea whether anyone is in the room. It sits at full brightness
all night, stays on whatever subpage it was left on, and there is no way for Home
Assistant to wake it before someone walks up. There is no inactivity detection in
the firmware at all.

## What this adds

A three-stage idle model, per deck, mirroring the one already proven on the
NSPanel in the same office:

| after | stage | effect |
|---|---|---|
| `page_timeout_s` | page | close any modal, return to Home |
| `dim_timeout_s` | dim | backlight to `dim_brightness` |
| `sleep_timeout_s` | sleep | backlight to `sleep_brightness`, optional screensaver |

Every timeout is independently disableable with `0`, and `0` is the default for
all three — an existing deck behaves exactly as it does today.

Plus a **wake button** exposed to Home Assistant, and the **second card flags
byte** that [#2](https://github.com/jsenecal/tilehaus/issues/2) needs for
per-tile label scrolling. Both ride the same DECK bump, because the bump is the
expensive part.

**Deliberately out of scope:** the fullscreen-image screensaver. It needs an
image source and a decode pipeline, and the repo has history here —
`docs/comparison.md` records the Camera card being removed because *"full-res
JPEG decode froze the UI"*. `screensaver_mode = 2` is reserved for it. Its own
project.

## DECK v7

### Header: 22 → 32 bytes

```
 0..3   magic "DECK"
 4..5   version            = 7
 6..7   header size        = 32
 8..11  payload length
12..13  card count
14      page count
15      reserved, must be 0
16..19  accent (int32, -1 = built-in)
20      gridCols
21      gridRows
22..23  page_timeout_s     u16   0 = never return to Home
24..25  dim_timeout_s      u16   0 = never dim
26..27  sleep_timeout_s    u16   0 = never sleep
28      brightness         u8    percent, awake level, default 100
29      dim_brightness     u8    percent, default 25
30      sleep_brightness   u8    percent, default 0
31      screensaver_mode   u8    0 = none, 1 = clock (2 = image, reserved)
```

Ten bytes, once per document rather than per card. `u16` seconds caps at 65535
(~18 hours), far beyond anything useful, and seconds rather than milliseconds
because the field is human-facing and the poll is 250ms anyway.

`sleep_brightness` is a field rather than a hardcoded zero. The default gives
"sleep means dark" out of the box, but a panel that wants 1% rather than fully
off costs one byte to support.

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

The body grows because the flags byte has all 8 bits assigned and the 16 fixed
bytes are fully used — there is nothing to reclaim. Cost: 128 bytes on a maximum
deck, against a 16384-byte budget.

### Compatibility

Both decoders already branch on version for header size and gain a matching
branch for card stride. A v6 document decodes with every new field at its
default, so **existing decks keep working and keep their current behaviour**.
This matters concretely: the office panel has a v6 deck in NVS right now, and it
must survive the flash.

Fixtures: `deck_v5_pages.bin` is already frozen. `deck_v6_grid.bin` **must now
also be frozen** — `gen_deck_fixture.js` currently regenerates it through
`encodeDeck`, so bumping the version would rewrite the very file that proves v6
still decodes. `deck_basic.bin` continues to track the current format.

## The engine is a pure function

The three timeouts are not a ladder with transitions — they are independent
thresholds whose effects compose. At 20s the panel is home at full brightness; at
40s home and dimmed; at 700s home, dark, showing a clock. Modelling that as a
state machine invites bugs at every edge; computing a *target* from elapsed time
does not.

```cpp
// firmware/native/idle_state.h — no LVGL, host-tested
struct IdleConfig {
  int page_s = 0, dim_s = 0, sleep_s = 0;
  int brightness = 100, dim_brightness = 25, sleep_brightness = 0;
  int screensaver_mode = 0;
};
struct IdleTargets {
  bool go_home = false;      // modal closed, stack cleared, page 0
  int brightness_pct = 100;  // what the backlight should be right now
  bool screensaver = false;  // whether the clock overlay should show
};

IdleTargets idle_targets(const IdleConfig &cfg, uint32_t inactive_ms,
                         int current_page, bool modal_open);
```

Idempotent, so the poll can call it every tick and act only on what differs from
the current state. Every boundary — which threshold wins when `dim_s > sleep_s`,
what `0` means per field, the exact millisecond of each crossing — is testable
without hardware.

`go_home` is suppressed when already on Home with no modal open, so the common
case costs nothing.

## Three thin LVGL-side pieces

**Brightness** cannot be set from native code: `display_backlight` is an ESPHome
light. Same hook pattern as the existing reboot darkening — `bindings.yaml`
supplies a `set_brightness_hook()` that native code calls with a percent.

**Screensaver** is a full-screen LVGL overlay showing a large clock and date,
reusing the tick that `header_card.h` already runs. Hidden by default; shown when
`screensaver` goes true. It is an overlay rather than a page so it composes with
whatever page is underneath and needs no entry in the page table.

**Wake** is an ESPHome `button` template exposed to HA, calling one native
`panel_wake()`: restore `brightness`, hide the screensaver, close any modal,
return to Home, and reset LVGL's inactivity clock. That last part is not
optional — without it the next 250ms tick would compute the same idle targets and
put the panel straight back to sleep.

## Waking by touch

Most of this falls out for free. A touch resets LVGL's inactivity clock, so
`inactive_ms` drops to ~0, `idle_targets()` returns the awake brightness and no
screensaver, and the next tick applies it. **No dedicated wake path is needed for
the dim case.**

What does need care is whether that first touch also presses a tile:

| stage | screen | first touch |
|---|---|---|
| dimmed | readable | wakes **and** acts — you can see what you are tapping |
| asleep | dark | wakes only, swallowed |
| screensaver | clock showing | dismisses it only, swallowed |

`hardware.yaml` already implements exactly this for the dark case: it disables
LVGL's input devices while the backlight is off, so a wake touch cannot reach a
tile. That condition extends to cover "screensaver showing".

The catch: while indevs are disabled LVGL never sees the touch, so its inactivity
clock never resets and the panel would never wake. The escape is that ESPHome's
`on_touch` fires at the touchscreen level, below LVGL, and already runs today. So
the wake path is:

```
on_touch:
  lv_display_trigger_activity(NULL)   # reset the idle clock
  brightness  = cfg.brightness
  screensaver hidden
```

Touch never returns to Home — only the HA button does. Yanking the page out from
under someone walking up mid-task would be worse than leaving it.

## Configurator

Six new deck-level fields beside the existing accent and grid controls: three
timeouts in seconds (`0` labelled "never"), three brightness percents, and a
screensaver mode select (`None` / `Clock`). Plus the per-tile **Scroll label**
checkbox that #2 specced.

For label scrolling, `add_name` gains a long-mode argument. The shadow twin in
`add_text_shadow` must be **skipped** in scroll mode: keeping it marqueeing in
lockstep is what would smear the text, and a moving label does not need the depth
cue.

## Testing

Host tests, no device required:

- `idle_state_test` (new) — every threshold in isolation and in combination: all
  zeros, each stage alone, all three together, `dim_s > sleep_s`, exact boundary
  milliseconds, and the already-home suppression.
- `deck_document_test` / `deck.test.ts` — v7 round-trips all seven new header
  fields and `scrollLabel`; the frozen v5 and v6 fixtures still decode with
  every new field defaulted.
- A new `deck_v7_idle.bin` fixture pins the 32-byte header and the widened card
  stride on both sides.

On-device: set `page 15 / dim 30 / sleep 60`, navigate to a subpage, and watch
each stage arrive in turn; confirm touch restores brightness without pressing a
tile; confirm the HA button returns to Home; set all three to `0` and confirm
nothing ever happens.

## Risks

**The 250ms poll is shared.** The lambda in `hardware.yaml` also drives indev
enable/disable. A fault in the new code takes touch handling with it — keep the
addition to a single call into native code that cannot throw.

**Brightness fights the user.** If someone sets the backlight from HA directly,
the next tick will overwrite it with whatever `idle_targets()` says. That is
correct for an idle engine but surprising; worth confirming on the glass before
deciding whether the engine should yield to a manual override.

**`flags2` bits 1-7 and `screensaver_mode` 2+ are reserved.** They must be
written as 0 so later versions can claim them without another stride change.
