# Tile-Size-Aware Text Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Pick the body and value fonts from each tile's measured pixel width, and flow the header's clock cluster horizontally when it sits in a one-row slot, so text stops clipping on 2-column tiles.

**Architecture:** Grid pixel math moves out of `poc_build_grid` into pure functions in `grid_layout.h`. `build_page` calls them once per page and hands each card a *per-tile copy* of `CardFonts` with `body`/`value` already swapped — so no card is modified, and modals (which get the original `CardFonts`) structurally cannot scale. The tight/roomy decision lives in a new LVGL-free `tile_scale.h` so it is host-testable.

**Tech Stack:** C++17 headers under `firmware/native/` (namespace `tilehaus`), LVGL 9 via ESPHome, ESPHome `font:` definitions in `firmware/fonts.yaml`, CTest host tests under `tests/firmware/`.

**Design:** `planning/2026-09-12-tile-size-aware-text-design.md`

---

## File Structure

| File | Change | Responsibility |
|---|---|---|
| `firmware/native/grid_layout.h` | modify | Pure grid math. Gains `GridMetrics`, `grid_final_columns/rows`, `compute_grid_metrics`, `tile_pixel_width/height`. |
| `firmware/native/grid.h` | modify | `poc_build_grid` delegates its geometry to the above instead of duplicating it. |
| `firmware/native/tile_scale.h` | **create** | The tight/roomy policy and its thresholds. No LVGL. |
| `firmware/native/card.h` | modify | `CardFonts` gains `body_small` (appended last — the struct is aggregate-initialised positionally). |
| `firmware/fonts.yaml` | modify | New `font_text_body_small` @17. |
| `firmware/bindings.yaml` | modify | Sixth initialiser for `CardFonts`. |
| `firmware/native/card_host.h` | modify | Per-tile `CardFonts` copy + `tile_metrics()`. The only place mapping a scale class to a font. |
| `firmware/native/card_style.h` | modify | `TileMetrics` + `tile_metrics()` accessor, beside the existing `tile_compact()`. |
| `firmware/native/header_card.h` | modify | Horizontal clock cluster in a short slot. |
| `tests/firmware/grid_layout_test.cpp` | modify | Cover the new metrics against real panel geometry. |
| `tests/firmware/tile_scale_test.cpp` | **create** | Cover the policy, including the exact boundary. |
| `tests/firmware/CMakeLists.txt` | modify | Register `tile_scale_test`. |

---

## Task 1: Grid pixel metrics as pure functions

**Files:**
- Modify: `firmware/native/grid_layout.h`
- Test: `tests/firmware/grid_layout_test.cpp`

- [ ] **Step 1: Write the failing test**

Append inside `main()` in `tests/firmware/grid_layout_test.cpp`, just before `return 0;`:

```cpp
  // --- pixel metrics: the office panel's real geometry ---
  // 1024x600, deck grid 12x7 (passed as overrides), pad 16, gap 10.
  {
    using tilehaus::compute_grid_metrics;
    using tilehaus::tile_pixel_width;
    using tilehaus::tile_pixel_height;
    using tilehaus::GridMetrics;

    const GridMetrics m =
        compute_grid_metrics(1024, 600, 200, 16, 10, 2, 0, 0, 12, 7);
    assert(m.cell_w == 73);   // (1024 - 2*16 - 11*10) / 12
    assert(m.cell_h == 72);   // (600  - 2*16 -  6*10) / 7
    assert(m.gap == 10);

    assert(tile_pixel_width(m, 2) == 156);    // 2x2 tile — the tight case
    assert(tile_pixel_width(m, 4) == 322);    // 4x2 All Office Lights
    assert(tile_pixel_width(m, 12) == 986);   // full-width header
    assert(tile_pixel_width(m, 1) == 73);
    assert(tile_pixel_width(m, 0) == 73);     // clamps to >= 1 span

    assert(tile_pixel_height(m, 1) == 72);    // 12x1 header slot
    assert(tile_pixel_height(m, 2) == 154);
    assert(tile_pixel_height(m, 4) == 318);   // 2x4 Downlights
  }

  // Auto-derived grid (no overrides) must match what poc_build_grid would use.
  {
    using tilehaus::compute_grid_metrics;
    using tilehaus::grid_final_columns;
    using tilehaus::grid_final_rows;

    // unit 200 on 1024 -> 5 normal columns, x2 subdivisions = 10.
    assert(grid_final_columns(1024, 200, 16, 10, 2, 0, 0) == 10);
    // base_cols wins over the unit-derived value, still x2.
    assert(grid_final_columns(1024, 200, 16, 10, 2, 5, 0) == 10);
    // override wins outright and is already final (no subdivision factor).
    assert(grid_final_columns(1024, 200, 16, 10, 2, 5, 12) == 12);
    assert(grid_final_rows(600, 200, 16, 10, 2, 3, 0) == 6);
    assert(grid_final_rows(600, 200, 16, 10, 2, 3, 7) == 7);

    const auto m = compute_grid_metrics(1024, 600, 200, 16, 10, 2, 5, 3, 0, 0);
    assert(m.cell_w == (1024 - 32 - 9 * 10) / 10);
    assert(m.cell_h == (600 - 32 - 5 * 10) / 6);
  }

  // Degenerate inputs clamp instead of dividing by zero or going negative.
  {
    using tilehaus::compute_grid_metrics;
    const auto m = compute_grid_metrics(40, 40, 200, 16, 10, 1, 0, 0, 8, 8);
    assert(m.cell_w >= 1);
    assert(m.cell_h >= 1);
  }
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
npm run test:firmware
```

Expected: FAIL — the build errors with `'compute_grid_metrics' is not a member of 'tilehaus'`.

- [ ] **Step 3: Write the implementation**

In `firmware/native/grid_layout.h`, insert directly after the `compute_columns` function (before `place_tiles`):

```cpp
// Pixel geometry of the final tile grid. Mirrors what poc_build_grid hands
// LVGL, so a caller can size a tile *before* any layout pass: columns are
// LV_GRID_FR(1) tracks that divide the remaining width evenly, and row tracks
// are set to an explicit height.
struct GridMetrics { int cell_w; int cell_h; int gap; };

// The final sub-unit column count. `base_cols` (0 = derive from unit_px) sets
// the pre-subdivision grid; `override_cols` (0 = derive) sets the final grid
// outright, already including any subdivision factor.
inline int grid_final_columns(int width_px, int unit_px, int pad_px, int gap_px,
                              int subdivisions, int base_cols, int override_cols) {
  if (subdivisions < 1) subdivisions = 1;
  const int normal = base_cols > 0
                         ? base_cols
                         : compute_columns(width_px, unit_px, pad_px, gap_px);
  const int cols = override_cols > 0 ? override_cols : normal * subdivisions;
  return cols < 1 ? 1 : cols;
}

// Row twin of grid_final_columns — compute_columns is dimension-agnostic track
// math, so height reuses it exactly as poc_build_grid does.
inline int grid_final_rows(int height_px, int unit_px, int pad_px, int gap_px,
                           int subdivisions, int base_rows, int override_rows) {
  if (subdivisions < 1) subdivisions = 1;
  const int normal = base_rows > 0
                         ? base_rows
                         : compute_columns(height_px, unit_px, pad_px, gap_px);
  const int rows = override_rows > 0 ? override_rows : normal * subdivisions;
  return rows < 1 ? 1 : rows;
}

inline GridMetrics compute_grid_metrics(int width_px, int height_px, int unit_px,
                                        int pad_px, int gap_px, int subdivisions,
                                        int base_cols, int base_rows,
                                        int override_cols, int override_rows) {
  const int cols = grid_final_columns(width_px, unit_px, pad_px, gap_px,
                                      subdivisions, base_cols, override_cols);
  const int rows = grid_final_rows(height_px, unit_px, pad_px, gap_px,
                                   subdivisions, base_rows, override_rows);
  int cell_w = (width_px - 2 * pad_px - (cols - 1) * gap_px) / cols;
  if (cell_w < 1) cell_w = 1;
  int cell_h = (height_px - 2 * pad_px - (rows - 1) * gap_px) / rows;
  if (cell_h < 1) cell_h = 1;
  return GridMetrics{cell_w, cell_h, gap_px};
}

// Pixel size of a tile spanning `span` tracks: the tracks plus the gaps between
// them.
inline int tile_pixel_width(const GridMetrics &m, int span) {
  if (span < 1) span = 1;
  return span * m.cell_w + (span - 1) * m.gap;
}
inline int tile_pixel_height(const GridMetrics &m, int span) {
  if (span < 1) span = 1;
  return span * m.cell_h + (span - 1) * m.gap;
}
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
npm run test:firmware
```

Expected: `100% tests passed out of 5`.

- [ ] **Step 5: Commit**

```bash
git add firmware/native/grid_layout.h tests/firmware/grid_layout_test.cpp
git commit -m "feat: pure pixel metrics for the tile grid

Tile pixel size is derivable from the display size and the grid knobs
without waiting for an LVGL layout pass. Pulling it into grid_layout.h
keeps it host-testable and lets build_page size a tile before building it."
```

---

## Task 2: Make `poc_build_grid` use the extracted math

The point is one source of truth — two copies of this arithmetic would drift silently.

**Files:**
- Modify: `firmware/native/grid.h:25-45`

- [ ] **Step 1: Replace the inline geometry**

In `firmware/native/grid.h`, replace this block:

```cpp
  if (subdivisions < 1) subdivisions = 1;
  lv_display_t *disp = lv_obj_get_display(root);
  int width = lv_display_get_horizontal_resolution(disp);
  int normal_cols = base_cols > 0 ? base_cols
                                  : compute_columns(width, unit_px, pad_px, gap_px);
  int columns = override_cols > 0 ? override_cols : normal_cols * subdivisions;

  // Row tracks are sized so the rows that fit the display fill its height
  // exactly (symmetric with columns filling the width) — accounting for the
  // outer padding and inter-row gaps — so a full screen of rows doesn't spill
  // into a scroll. Rows beyond what fits keep the same height and overflow,
  // giving vertical scroll only when there is genuinely more than one screen.
  // compute_columns is dimension-agnostic track math, reused here for height.
  int height = lv_display_get_vertical_resolution(disp);
  int normal_rows = base_rows > 0 ? base_rows
                                  : compute_columns(height, unit_px, pad_px, gap_px);
  int visible_rows = override_rows > 0 ? override_rows : normal_rows * subdivisions;
  if (visible_rows < 1) visible_rows = 1;
  int avail_h = height - 2 * pad_px - (visible_rows - 1) * gap_px;
  int row_h = avail_h / visible_rows;
  if (row_h < 1) row_h = 1;
```

with:

```cpp
  if (subdivisions < 1) subdivisions = 1;
  lv_display_t *disp = lv_obj_get_display(root);
  const int width = lv_display_get_horizontal_resolution(disp);
  const int height = lv_display_get_vertical_resolution(disp);

  // Geometry comes from grid_layout.h so build_page can compute the identical
  // numbers ahead of this call. Row tracks are sized so the rows that fit the
  // display fill its height exactly (symmetric with columns filling the width);
  // rows beyond that keep the same height and overflow into a vertical scroll.
  const int columns = grid_final_columns(width, unit_px, pad_px, gap_px,
                                         subdivisions, base_cols, override_cols);
  const GridMetrics gm = compute_grid_metrics(width, height, unit_px, pad_px,
                                              gap_px, subdivisions, base_cols,
                                              base_rows, override_cols, override_rows);
  const int row_h = gm.cell_h;
```

- [ ] **Step 2: Verify nothing else referenced the removed locals**

```bash
grep -n "normal_cols\|normal_rows\|visible_rows\|avail_h" firmware/native/grid.h
```

Expected: no output. (`rows` — the max tile bottom — is computed separately below and must stay.)

- [ ] **Step 3: Confirm the host tests still pass**

```bash
npm run test:firmware
```

Expected: `100% tests passed out of 5`. `grid.h` is not host-compiled, so this only proves Task 1 is intact; Task 7 compiles it for real.

- [ ] **Step 4: Commit**

```bash
git add firmware/native/grid.h
git commit -m "refactor: poc_build_grid takes its geometry from grid_layout.h

Same arithmetic, one copy. build_page needs these numbers before the
grid is built, and two implementations would drift."
```

---

## Task 3: The tight/roomy policy

**Files:**
- Create: `firmware/native/tile_scale.h`
- Create: `tests/firmware/tile_scale_test.cpp`
- Modify: `tests/firmware/CMakeLists.txt:16`

- [ ] **Step 1: Write the failing test**

Create `tests/firmware/tile_scale_test.cpp`:

```cpp
#include <cassert>

#include "tile_scale.h"

int main() {
  using tilehaus::TextScale;
  using tilehaus::text_scale_for;
  using tilehaus::kTileTightWidth;

  // A 2-column tile (156px on the office panel) is tight.
  assert(text_scale_for(156) == TextScale::Tight);
  // A 4-column tile (322px) and the full-width header (986px) are not.
  assert(text_scale_for(322) == TextScale::Default);
  assert(text_scale_for(986) == TextScale::Default);

  // The boundary is inclusive on the roomy side.
  assert(text_scale_for(kTileTightWidth - 1) == TextScale::Tight);
  assert(text_scale_for(kTileTightWidth) == TextScale::Default);

  // Degenerate widths are tight rather than crashing or reading as roomy.
  assert(text_scale_for(0) == TextScale::Tight);
  assert(text_scale_for(-10) == TextScale::Tight);

  // The header needs two rows' worth of content height to stack its clock.
  using tilehaus::header_clock_stacks;
  assert(!header_clock_stacks(72 - 32));    // 12x1 slot -> 40px content
  assert(header_clock_stacks(154 - 32));    // 12x2 slot -> 122px content
  return 0;
}
```

- [ ] **Step 2: Register the test and run it to verify it fails**

In `tests/firmware/CMakeLists.txt`, change:

```cmake
foreach(t grid_layout sensor_format slider_map toggle_state)
```

to:

```cmake
foreach(t grid_layout sensor_format slider_map toggle_state tile_scale)
```

Then:

```bash
npm run test:firmware
```

Expected: FAIL — `tile_scale.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

Create `firmware/native/tile_scale.h`:

```cpp
#pragma once

namespace tilehaus {

// How much horizontal room a tile has before its default text stops fitting.
// On the office panel a 2-column tile is 156px and a 4-column one is 322px, so
// 200 separates them with room to spare either side. Below this, a tile takes
// the smaller body font and drops its sensor value a rung.
//
// Derived from glyph advance-width arithmetic against a 124px content area
// (156px less two 16px insets), not from measuring rendered text — expect to
// tune it against a real panel.
inline constexpr int kTileTightWidth = 200;

// Minimum content height (tile height less both insets) for the header to stack
// its clock over its date. The stacked pair needs ~90px; a one-row slot offers
// 40px, which clipped the date.
inline constexpr int kHeaderStackMinContentHeight = 96;

enum class TextScale { Default, Tight };

// Deliberately returns a class, not an lv_font_t* — keeping LVGL out of this
// header is what makes the policy host-testable. card_host.h maps it to fonts.
inline TextScale text_scale_for(int tile_px_w) {
  return tile_px_w < kTileTightWidth ? TextScale::Tight : TextScale::Default;
}

inline bool header_clock_stacks(int content_px_h) {
  return content_px_h >= kHeaderStackMinContentHeight;
}

}  // namespace tilehaus
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
npm run test:firmware
```

Expected: `100% tests passed out of 6`.

- [ ] **Step 5: Commit**

```bash
git add firmware/native/tile_scale.h tests/firmware/tile_scale_test.cpp tests/firmware/CMakeLists.txt
git commit -m "feat: tile text-scale policy

One threshold, kept free of LVGL so the decision is host-testable and
the tuning numbers live in one place."
```

---

## Task 4: The 17px body font

**Files:**
- Modify: `firmware/fonts.yaml` (append)
- Modify: `firmware/native/card.h:11-17`
- Modify: `firmware/bindings.yaml:7-13`

- [ ] **Step 1: Add the font**

Append to `firmware/fonts.yaml`:

```yaml
  # Tile names on tight (narrow) tiles — a 2-column tile has ~124px of content,
  # where the 22px body wraps an 11-character title onto two lines.
  - file: "gfonts://Roboto"
    id: font_text_body_small
    size: 17
    bpp: 4
    glyphs: !include assets/text_glyphs.yaml
```

- [ ] **Step 2: Add the CardFonts field**

In `firmware/native/card.h`, replace:

```cpp
struct CardFonts {
  const lv_font_t *icon = nullptr;    // font_icon_main
  const lv_font_t *value = nullptr;   // font_number_value
  const lv_font_t *body = nullptr;    // font_text_body
  const lv_font_t *medium = nullptr;  // font_number_medium (compact values)
  const lv_font_t *small = nullptr;   // font_text_small (secondary lines)
};
```

with:

```cpp
struct CardFonts {
  const lv_font_t *icon = nullptr;    // font_icon_main
  const lv_font_t *value = nullptr;   // font_number_value
  const lv_font_t *body = nullptr;    // font_text_body
  const lv_font_t *medium = nullptr;  // font_number_medium (compact values)
  const lv_font_t *small = nullptr;   // font_text_small (secondary lines)
  // Appended last: bindings.yaml aggregate-initialises this struct positionally,
  // so inserting above would silently reassign every field after it.
  const lv_font_t *body_small = nullptr;  // font_text_body_small (tight tiles)
};
```

- [ ] **Step 3: Populate it**

In `firmware/bindings.yaml`, replace:

```yaml
                id(font_text_small)->get_lv_font(),
            };
```

with:

```yaml
                id(font_text_small)->get_lv_font(),
                id(font_text_body_small)->get_lv_font(),
            };
```

- [ ] **Step 4: Validate the config**

```bash
.venv-esphome/bin/esphome config firmware/tilehaus.yaml > /dev/null && echo OK
```

Expected: `OK`. A typo in the font id fails here with `Couldn't find ID 'font_text_body_small'`.

- [ ] **Step 5: Commit**

```bash
git add firmware/fonts.yaml firmware/native/card.h firmware/bindings.yaml
git commit -m "feat: add a 17px body font for tight tiles

~34KB (22KB bitmap + ~12KB glyph index). Appended last in CardFonts
because bindings.yaml initialises it positionally."
```

---

## Task 5: Per-tile fonts in `build_page`

**Files:**
- Modify: `firmware/native/card_style.h` (after `tile_compact`, around line 22)
- Modify: `firmware/native/card_host.h:32-62`

- [ ] **Step 1: Add the tile metrics accessor**

In `firmware/native/card_style.h`, insert immediately after the closing brace of `tile_compact()`:

```cpp
// Pixel size of the tile currently being built, set by build_page around each
// card->build(). Same idiom as tile_compact() above: cards that need to adapt
// to their own size read it during build, and it is meaningless outside that
// window. Only HeaderCard uses it today.
struct TileMetrics { int px_w; int px_h; };
inline TileMetrics &tile_metrics() {
  static TileMetrics m{0, 0};
  return m;
}
```

- [ ] **Step 2: Compute the metrics and swap the fonts**

In `firmware/native/card_host.h`, add the include beside the existing ones:

```cpp
#include "tile_scale.h"
```

Then, inside `build_page`, replace:

```cpp
  auto &cards = live_cards();
  const size_t start = cards.size();
  std::vector<GridTile> tiles;
```

with:

```cpp
  auto &cards = live_cards();
  const size_t start = cards.size();
  // Same numbers poc_build_grid will use below, computed up front so each card
  // can be built at the right text size — no layout pass, no reflow.
  lv_display_t *disp = lv_obj_get_display(container);
  const GridMetrics gm = compute_grid_metrics(
      lv_display_get_horizontal_resolution(disp),
      lv_display_get_vertical_resolution(disp), unit_px, pad_px, gap_px,
      subdivisions, base_cols, base_rows, override_cols, override_rows);
  std::vector<GridTile> tiles;
```

Then replace:

```cpp
    tile_compact() = (sz.w <= 1 && sz.h <= 1);  // 1x1 → centred icon, no label
    card->build(cell, cfg, fonts);
    tile_compact() = false;
```

with:

```cpp
    tile_compact() = (sz.w <= 1 && sz.h <= 1);  // 1x1 → centred icon, no label
    tile_metrics() = {tile_pixel_width(gm, sz.w), tile_pixel_height(gm, sz.h)};
    // Hand the card its own CardFonts rather than threading a size through
    // every card's build(). Cards keep writing fonts.body / fonts.value
    // unchanged, and modals — which receive the original CardFonts built in
    // bindings.yaml — cannot pick up tile sizing by accident.
    CardFonts tile_fonts = fonts;
    if (text_scale_for(tile_metrics().px_w) == TextScale::Tight) {
      if (fonts.body_small) tile_fonts.body = fonts.body_small;
      if (fonts.medium) tile_fonts.value = fonts.medium;
    }
    card->build(cell, cfg, tile_fonts);
    tile_compact() = false;
    tile_metrics() = {0, 0};
```

- [ ] **Step 3: Commit**

```bash
git add firmware/native/card_style.h firmware/native/card_host.h
git commit -m "feat: size tile text from the tile's measured width

build_page hands each card a per-tile CardFonts copy, so no card changes
and modals keep the unscaled fonts by construction."
```

---

## Task 6: Header clock in a one-row slot

**Files:**
- Modify: `firmware/native/header_card.h:119-131`

- [ ] **Step 1: Add the include**

In `firmware/native/header_card.h`, beside the existing includes:

```cpp
#include "tile_scale.h"
```

- [ ] **Step 2: Flow the cluster horizontally when short**

Replace:

```cpp
    if (cfg.show_clock) {
      lv_obj_t *tx = flex_box(right, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(tx, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END,
                            LV_FLEX_ALIGN_END);
      clock_ = text(tx, fonts.value, "--:--");
      date_ = text(tx, fonts.body, "");
```

with:

```cpp
    if (cfg.show_clock) {
      // This card's default size is 10x2. Dropped into a one-row slot it gets
      // ~40px of content, but the 55px clock alone has a ~64px line height, so
      // the stacked date was sliced in half. Shrinking to fit would need ~17px,
      // which is unreadable across a room — so the pair goes side by side
      // instead, which the header's ~986px of width easily absorbs.
      const bool stacked =
          header_clock_stacks(tile_metrics().px_h - 2 * kTileInset);
      lv_obj_t *tx =
          flex_box(right, stacked ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(tx, LV_FLEX_ALIGN_END,
                            stacked ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
      if (!stacked) lv_obj_set_style_pad_column(tx, 12, 0);
      clock_ = text(tx, stacked ? fonts.value : fonts.medium, "--:--");
      date_ = text(tx, fonts.body, "");
```

- [ ] **Step 3: Commit**

```bash
git add firmware/native/header_card.h
git commit -m "fix: header clock and date flow side by side in a 1-row slot

A 10x2 card in a 12x1 slot has 40px of content; the 55px clock's line
height alone exceeds that, which clipped the date."
```

---

## Task 7: Build, flash, verify on the panel

**Files:** none modified.

- [ ] **Step 1: Full host verification**

```bash
npm run typecheck && npm run test:web && npm run test:firmware
```

Expected: typecheck silent, web `pass 8 / fail 0`, firmware `100% tests passed out of 6`.

- [ ] **Step 2: Compile**

```bash
.venv-esphome/bin/esphome compile firmware/tilehaus.yaml 2>&1 | tail -5
```

Expected: `INFO Successfully compiled program.` Flash should read ~22.5% (up from 22.1%) — the 17px font. If it is unchanged, the font was not actually linked; check Task 4 Step 3.

- [ ] **Step 3: Flash**

```bash
.venv-esphome/bin/esphome upload firmware/tilehaus.yaml --device 192.168.252.221 2>&1 | tail -4
```

Expected: `INFO OTA successful`.

The deck survives this flash — `kMaxDocumentBytes` is unchanged, so the stored
blob still loads. (See issue #3: changing that constant silently wipes the deck.)

- [ ] **Step 4: Confirm the panel is back with its deck**

```bash
until curl -s -m 3 -o /dev/null http://192.168.252.221/api/v1/config; do sleep 3; done
node scripts/deck_cli.js status --device 192.168.252.221 | head -2
```

Expected: `ETag "1"` and `tiles 63`. An `ETag "0"` / `awaiting config` means the stored deck was dropped — re-push with `node scripts/deck_cli.js push <spec> --device 192.168.252.221`.

- [ ] **Step 5: Verify on the glass**

Check the Home page against the defects the design lists:

| tile | before | expect |
|---|---|---|
| Temperature (2x2) | `23.4°(` | `23.4°C` complete |
| PM2.5 (2x2) | `1 µg/r` | `1 µg/m³` complete |
| Window DL 1 (2x2) | wrapped to two lines | one line at 17px |
| All Office Lights (4x2, page 1) | — | unchanged: 22px body, 55px value |
| Header date | `Sat Sep 12` sliced | fully visible, beside the clock |

- [ ] **Step 6: Commit any threshold tuning**

If the glass disagrees with the arithmetic, adjust `kTileTightWidth` or the font
size in `firmware/fonts.yaml`, then rerun Steps 2–5. Commit as:

```bash
git add -A firmware
git commit -m "fix: tune tile text thresholds against the panel"
```

---

## Self-Review Notes

**Spec coverage.** Design Part 1 → Tasks 1, 2 (metrics), 3 (policy), 4 (font), 5 (wiring). Design Part 2 → Task 6. Design "Testing" → Tasks 1, 3 (host) and 7 (on-device). Design "Non-goals" → no task touches `add_icon` or `transform_scale`, and no task adds a deck field.

**Type consistency.** `GridMetrics{cell_w, cell_h, gap}`, `tile_pixel_width/height(const GridMetrics&, int)`, `TextScale::{Default,Tight}`, `text_scale_for(int)`, `header_clock_stacks(int)`, `TileMetrics{px_w, px_h}`, `tile_metrics()`, `CardFonts::body_small` — each defined once and used with the same spelling throughout.

**Known gap.** `grid.h`, `card_host.h` and `header_card.h` are not host-compiled (they need LVGL), so Tasks 2, 5 and 6 are only proven by Task 7 Step 2. Run it before assuming those tasks are done.
