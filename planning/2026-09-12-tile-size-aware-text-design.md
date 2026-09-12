# Tile-Size-Aware Text — Design

**Status:** Design (for review)
**Date:** 2026-09-12
**Issue:** [#1](https://github.com/jsenecal/tilehaus/issues/1)

## Problem

Fonts are fixed at boot. `CardFonts` resolves five ESPHome `font:` ids and hands
the same `lv_font_t*` to every card regardless of the tile's size, so text sized
for a roomy tile overflows a tight one. On a 1024x600 panel at 12x7, a 2-column
tile is 156px wide — 124px of content after the 16px insets — and that is where
it breaks down.

Observed on the office panel (photo, 2026-09-12):

| symptom | tile | cause |
|---|---|---|
| `23.4°(` instead of `23.4°C` | 2x2 Temperature | `font_number_value` @55 needs ~150px, has 124px |
| `1 µg/r` instead of `1 µg/m³` | 2x2 PM2.5 | same |
| `Window DL` / `1` on two lines | 2x2 Window DL 1/2 | `font_text_body` @22 at ~11 chars hits 124px |
| `Sat Sep 12` sliced horizontally | 12x1 Header | a 2-row card in a 1-row slot |

The clipped sensor values are the most visible defect: the tile silently shows
wrong-looking data rather than merely awkward layout.

## Non-goals

**Icon scaling is explicitly out.** The original issue proposed a font ladder
covering icons, and an earlier draft of this design had them scaling
continuously via `transform_scale`. The panel photo shows 46px icons are
well-proportioned on a 2-column tile and crowd nothing. Building icon scaling
would add a moving part to fix a problem that does not exist. Dropped.

**Scaling up is out.** Large tiles look correct as they are; this design only
ever picks a *smaller* font than the default.

**No new deck fields.** Selection is inferred from measured geometry, so the
DECK wire format is untouched and no v7 is needed.

## Part 1 — Width-driven font selection

### Where the size comes from

`poc_build_grid` derives its pixel geometry inline from the display resolution
and the grid knobs. Extract that into `grid_layout.h` (pure, no LVGL, already
host-tested):

```cpp
struct GridMetrics { int cell_w, cell_h, gap; };

GridMetrics compute_grid_metrics(int width_px, int height_px, int unit_px,
                                 int pad_px, int gap_px, int subdivisions,
                                 int base_cols, int base_rows,
                                 int override_cols, int override_rows);

int tile_pixel_width(const GridMetrics &m, int span_w);   // span*cell + (span-1)*gap
int tile_pixel_height(const GridMetrics &m, int span_h);
```

`poc_build_grid` then *calls* this rather than keeping its own copy, so there is
one source of truth and the two cannot drift.

This is computed arithmetic, not a layout result — it needs no LVGL pass, so
there is no reflow and no `LV_EVENT_SIZE_CHANGED` handler.

### How it reaches the cards

`build_page` already brackets each `card->build()` with `tile_compact()`. The
same idiom carries the new information, but instead of threading a parameter
through fifteen cards, `build_page` hands each card a **per-tile copy** of
`CardFonts`:

```cpp
const GridMetrics gm = compute_grid_metrics(/* … */);
// per tile:
const int px_w = tile_pixel_width(gm, sz.w);
CardFonts tf = fonts;
const bool tight = text_scale_for(px_w) == TextScale::Tight;
tf.body  = tight ? fonts.body_small : fonts.body;   // 22 → 17
tf.value = tight ? fonts.medium     : fonts.value;  // 55 → 34
tile_metrics() = {px_w, tile_pixel_height(gm, sz.h)};
card->build(cell, cfg, tf);
```

Every card keeps writing `fonts.body` and `fonts.value` verbatim. **No card is
modified.**

This also gives the containment guarantee structurally rather than by
convention: `splash`, `overlay`, `pin_keypad`, `confirm_dialog`,
`climate_dropdown` and the modal tabs receive the original `CardFonts` built in
`bindings.yaml`, never this per-tile copy, so they cannot accidentally scale.
Confirmed by inspection — `add_icon`/`add_name` are called only from cards.

### Policy

A new header `tile_scale.h` holds the policy. It must not return
`const lv_font_t *` — that would drag LVGL in and make it untestable on the
host — so it classifies instead, and `card_host.h` maps the class to a font:

```cpp
// tile_scale.h — pure, no LVGL, host-testable
inline constexpr int kTileTightWidth = 200;  // px; below this a tile is "tight"

enum class TextScale { Default, Tight };
inline TextScale text_scale_for(int px_w) {
  return px_w < kTileTightWidth ? TextScale::Tight : TextScale::Default;
}

// card_host.h — the only place that knows about fonts
const bool tight = text_scale_for(px_w) == TextScale::Tight;
tf.body  = tight ? fonts.body_small : fonts.body;
tf.value = tight ? fonts.medium     : fonts.value;
```

200px cleanly separates a 2-column tile (156px) from a 4-column one (322px).

| role | roomy | tight | source |
|---|---|---|---|
| `body` | `font_text_body` @22 | `font_text_body_small` @17 | **new font** |
| `value` | `font_number_value` @55 | `font_number_medium` @34 | already exists |

The value fallback reuses an existing font, so it costs nothing and fixes the
worst defect. `"20.5°C"` at 34px is ~108px against 124px of content.

### Cost

One new font. `(17/22)² × 37,562` ≈ 22 KB of bitmap plus a ~12 KB glyph index ≈
**34 KB**, taking fonts from ~620 KB to ~654 KB and the image from 22.1% to
~22.5% of the 8,126,464-byte app partition.

Changes: `fonts.yaml` (new `font_text_body_small`), `bindings.yaml` (one more
initialiser), `CardFonts` (one field), `grid_layout.h` (extraction),
`grid.h` (call the extraction), `card_host.h` (per-tile copy), `tile_scale.h`
(new).

## Part 2 — Header in a one-row slot

`HeaderCard::default_size()` is `{10, 2}`. Placed at 12x1 it gets 72px of tile,
40px of content after insets. Its clock cluster is a **column** — `clock_` @55
over `date_` @22 — roughly 90px stacked. A 55px font alone has a ~64px line
height, so the clock overflows 40px before the date is even considered.

Shrinking cannot solve this: fitting two stacked lines in 40px needs ~17px and
~15px, which is unreadable on a wall panel.

**Fix: flow the cluster horizontally when the tile is short.** At 986px wide
there is abundant unused width. When content height cannot seat the stack, the
clock box becomes `LV_FLEX_FLOW_ROW` with the clock at `fonts.medium` (34px,
~40px line height) and the date at `fonts.body` — about 215px total, trivially
accommodated.

Gated on measured height via `tile_metrics().px_h`, which is why that accessor
survives despite icon scaling being dropped:

```cpp
inline constexpr int kHeaderStackMinContentHeight = 96;  // px
```

The left greeting/subtitle column is left alone — the photo shows it renders
correctly, and the scrolling subtitle is working as designed.

## Testing

Host tests (`tests/firmware/`), no device required:

- `grid_layout_test` — `compute_grid_metrics` on the real panel geometry
  (1024x600, 12x7, pad 16, gap 10) yields `cell_w == 73`; `tile_pixel_width`
  gives 156 for a 2-span and 322 for a 4-span.
- New `tile_scale_test` — `text_scale_for` returns `Tight` below
  `kTileTightWidth` and `Default` at or above it, including the boundary exactly
  at 200. Pure integer policy, so this is a complete test of the decision.
- Regression: `poc_build_grid` delegating to `compute_grid_metrics` must not
  change existing `grid_layout_test` expectations.

On-device verification (the parts host tests cannot reach): Temperature and
PM2.5 render their full units, `Window DL 1` sits on one line, `All Office
Lights` (4-col) keeps the 22px body and 55px value, and the header date is
fully visible.

## Risks

**Thresholds are a first pass.** 200px and the 17px size come from advance-width
arithmetic, not from measuring rendered glyphs. The photo already corrected one
such estimate — label wrapping proved far less widespread than predicted. Expect
one tuning round on glass. Both live as named constants in `tile_scale.h`.

**The 4-col boundary is untested by the current deck** in the tight direction:
only two tile widths exist today (156 and 322), so the threshold is exercised at
its extremes, not near it.

**Reusing `font_number_medium` for tight values** couples the sensor tile to a
font chosen for the weather card. If that font is ever resized for weather, tight
sensor tiles move with it. Acceptable now; worth a dedicated rung if they
diverge.

---

## Addendum (2026-09-12, post-implementation)

Two things in this document describe a design that did **not** ship. Read them
alongside this note.

**1. "How it reaches the cards" is wrong, and its safety claim was false.**
The design had `build_page` hand each card a per-tile *copy* of `CardFonts` with
`body`/`value` already swapped, and claimed modals were "structurally incapable"
of picking up tile sizing because they receive the original struct. Review found
that eleven cards do `fonts_ = fonts;` and later pass that stored struct to a
modal, confirm dialog or PIN pad (`lock_card.h:39,64`; `climate_card.h:38,56`;
`cover_card.h:88,176`; and eight more). The swapped copy therefore followed them
into full-screen overlays: a modal opened from a 2-column tile rendered at 17px,
the same modal from a wide tile at 22px. The alarm PIN pad and the lock's confirm
dialog — both named in this document as things that must never scale — inherited
it.

The convenience that caused the bug *was* the design's selling point: swap in
`build_page` so no card needs changing. Cards persisting what they are handed is
exactly what makes that unsound.

**Shipped instead:** `build_page` passes the fonts through untouched and only
sets `tile_metrics()`. The choice moved to the tile-only render points —
`add_name` takes the whole `CardFonts` and picks via `tile_body_font()`, and the
sensor value picks via `tile_value_font()`. Both are reachable only from tile
bodies, so containment is checkable rather than asserted. Fifteen `add_name` call
sites changed; no card logic did. A follow-up guards `px_w > 0` so a call from
outside the build window falls back to the full-size font rather than the shrunk
one.

**2. The field is `body_tight`, not `body_small`** (`font_text_body_tight` in
`fonts.yaml`), matching the vocabulary the policy already uses —
`kTileTightWidth`, `TextScale::Tight`. "Small" was already taken by the unrelated
15px secondary-text font. `bindings.yaml` also uses designated initialisers now;
the positional aggregate init described here was a silent-misorder hazard that a
comment could only warn about.

**Measured cost:** +34,376 bytes of flash (1,793,592 → 1,827,968; 22.1% → 22.5%),
matching this document's ~34 KB estimate. RAM +104 bytes for the `TileMetrics`
static.
