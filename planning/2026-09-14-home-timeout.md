# Inactivity Timeout to Home — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a per-deck inactivity timeout (seconds, `0` = disabled by default) that closes any open modal and returns the panel to Home, bundled with the second card flags byte that per-tile label scrolling needs.

**Architecture:** DECK bumps to v7 — a `u16` in the header for the timeout, one `flags2` byte per card for the scroll flag. Both decoders already branch on version for header size and gain a matching branch for card stride, so v5 and v6 documents keep decoding with the new fields defaulted. The timeout decision lives in a pure LVGL-free header so it is host-testable; the firmware polls `lv_display_get_inactive_time()` from the 250ms interval that already exists in `hardware.yaml`.

**Tech Stack:** TypeScript codec (`web/model/deck.ts`), C++17 headers under `firmware/native/` (namespace `tilehaus`), LVGL 9 via ESPHome, CTest host tests, node:test for web.

**Design:** `planning/2026-09-14-home-timeout-design.md`

---

## File Structure

| File | Change | Responsibility |
|---|---|---|
| `web/model/deck.ts` | modify | v7 codec: `homeTimeoutS` in the header, `scrollLabel` in `flags2`. |
| `tests/web/deck.test.ts` | modify | v7 round-trip; v5/v6 fixtures still decode with defaults. |
| `scripts/gen_deck_fixture.js` | modify | Freeze `deck_v6_grid.bin`; regenerate `deck_basic.bin` as v7; add a v7 timeout fixture. |
| `firmware/native/deck_document.h` | modify | C++ side of the same format change. |
| `tests/firmware/deck_document_test.cpp` | modify | Mirror of the TS tests, against the same fixtures. |
| `firmware/native/home_timeout.h` | **create** | `should_return_home()` — the whole policy, no LVGL. |
| `tests/firmware/home_timeout_test.cpp` | **create** | Policy tests including boundaries. |
| `tests/firmware/CMakeLists.txt` | modify | Register `home_timeout_test`. |
| `firmware/native/overlay.h` | modify | Module-scope active-overlay pointer so "is a modal open" is answerable. |
| `firmware/native/card_host.h` | modify | `deck_home_timeout()` accessor + `tick_home_timeout()` entry point. |
| `firmware/native/card_style.h` | modify | `add_name` gains a scroll long-mode. |
| `firmware/hardware.yaml` | modify | One call into `tick_home_timeout()` from the existing 250ms interval. |
| `firmware/bindings.yaml` | modify | Store the decoded timeout when the deck loads. |
| `web/editor_state.ts`, `web/editor_view.ts`, `web/card_fields.ts`, `web/deck_client.ts` | modify | Timeout field + per-tile Scroll label checkbox. |

---

## Task 1: DECK v7 in the TypeScript codec

**Files:**
- Modify: `web/model/deck.ts`
- Test: `tests/web/deck.test.ts`

- [ ] **Step 1: Write the failing test**

In `tests/web/deck.test.ts`, add `scrollLabel: false` to **both** entries of the `BASIC` array (after `tightMargins: false`), then append inside `runDeckCodecTests()` just before its closing brace:

```ts
  // --- v7 home timeout + scrollLabel ---
  {
    const doc = decodeDeckDocument(encodeDeck(BASIC, -1, ["Home"], 12, 7, 45));
    assert(doc.homeTimeoutS === 45, "home timeout round-trips");
    assert(decodeDeckDocument(encodeDeck(BASIC)).homeTimeoutS === 0,
           "home timeout defaults to 0");
  }
  {
    const scrolled: DeckCard = { ...BASIC[0]!, scrollLabel: true };
    const round = decodeDeck(encodeDeck([scrolled]));
    assert(round[0]!.scrollLabel === true, "scrollLabel round-trips");
    assert(decodeDeck(encodeDeck([BASIC[0]!]))[0]!.scrollLabel === false,
           "scrollLabel defaults false");
  }
  // A v6 document still decodes, with both new fields defaulted. This fixture is
  // frozen precisely so this assertion keeps meaning something.
  {
    const v6 = new Uint8Array(readFileSync("tests/firmware/fixtures/deck_v6_grid.bin"));
    const doc = decodeDeckDocument(v6);
    assert(doc.gridCols === 8 && doc.gridRows === 4, "v6 grid still decodes");
    assert(doc.homeTimeoutS === 0, "v6 yields timeout 0");
    assert(doc.cards[0]!.scrollLabel === false, "v6 yields scrollLabel false");
  }
  throws(() => encodeDeck(BASIC, -1, ["Home"], 12, 7, -1), "negative timeout rejected");
  throws(() => encodeDeck(BASIC, -1, ["Home"], 12, 7, 70000), "oversize timeout rejected");
```

- [ ] **Step 2: Run it and watch it fail**

Run: `npm run test:web`
Expected: FAIL — `homeTimeoutS` is not a property of the decoded document, and `scrollLabel` is not on `DeckCard`.

- [ ] **Step 3: Implement the codec change**

In `web/model/deck.ts`:

Replace the version/size constants block:
```ts
export const DECK_DOCUMENT_VERSION = 6;
```
with:
```ts
export const DECK_DOCUMENT_VERSION = 7;
```

After `export const DECK_HEADER_SIZE_V6 = 22;` add:
```ts
export const DECK_HEADER_SIZE_V7 = 24;      // v7 appends a u16 home timeout
export const DECK_HOME_TIMEOUT_DEFAULT = 0; // seconds; 0 = never return home
export const DECK_MAX_HOME_TIMEOUT = 65535; // u16 ceiling
```

In `interface DeckCard`, after `tightMargins: boolean;` add:
```ts
  scrollLabel: boolean;   // v7 flags2 bit0: marquee the name instead of wrapping
```

In `interface DeckDocument`, after `gridRows: number;` add:
```ts
  homeTimeoutS: number;   // seconds of inactivity before returning to Home; 0 = off
```

In `encodeCard`, replace:
```ts
  const body = new Uint8Array(16 + strings.reduce((n, s) => n + 1 + s.length, 0));
```
with:
```ts
  // v7: one more fixed byte for flags2. The original flags byte has all 8 bits
  // assigned and the 16 fixed bytes are fully used, so there was nothing to reclaim.
  const flags2 = (card.scrollLabel ? 1 : 0);
  const body = new Uint8Array(17 + strings.reduce((n, s) => n + 1 + s.length, 0));
```
and replace:
```ts
  body[15] = checkByte(card.page, "page");
  let offset = 16;
```
with:
```ts
  body[15] = checkByte(card.page, "page");
  body[16] = flags2;
  let offset = 17;
```

Replace the whole `encodeDeck` signature and header-writing section:
```ts
export function encodeDeck(
  cards: DeckCard[], accent: number = DECK_ACCENT_DEFAULT, pages: string[] = ["Home"],
  gridCols: number = DECK_GRID_COLS_DEFAULT, gridRows: number = DECK_GRID_ROWS_DEFAULT,
): Uint8Array {
```
with:
```ts
export function encodeDeck(
  cards: DeckCard[], accent: number = DECK_ACCENT_DEFAULT, pages: string[] = ["Home"],
  gridCols: number = DECK_GRID_COLS_DEFAULT, gridRows: number = DECK_GRID_ROWS_DEFAULT,
  homeTimeoutS: number = DECK_HOME_TIMEOUT_DEFAULT,
): Uint8Array {
  if (!Number.isInteger(homeTimeoutS) || homeTimeoutS < 0 || homeTimeoutS > DECK_MAX_HOME_TIMEOUT)
    fail(`home timeout must be 0..${DECK_MAX_HOME_TIMEOUT} seconds`);
```

Then in the same function replace every `DECK_HEADER_SIZE_V6` with `DECK_HEADER_SIZE_V7` (three occurrences: the `new Uint8Array(...)`, the `writeU16(out, 6, ...)`, and the `let offset = ...`), and after:
```ts
  out[21] = checkByte(gridRows, "gridRows");
```
add:
```ts
  writeU16(out, 22, homeTimeoutS);
```

In `decodeDeckDocument`, replace:
```ts
  const version = readU16(input, 4);
  if (version !== 5 && version !== 6) fail("invalid deck document header");
  const headerSize = version === 6 ? DECK_HEADER_SIZE_V6 : DECK_HEADER_SIZE_V5;
```
with:
```ts
  const version = readU16(input, 4);
  if (version !== 5 && version !== 6 && version !== 7) fail("invalid deck document header");
  const headerSize = version === 7 ? DECK_HEADER_SIZE_V7
                   : version === 6 ? DECK_HEADER_SIZE_V6
                   : DECK_HEADER_SIZE_V5;
```

Replace:
```ts
  const gridCols = version === 6 ? input[20]! : DECK_GRID_COLS_DEFAULT;
  const gridRows = version === 6 ? input[21]! : DECK_GRID_ROWS_DEFAULT;
```
with:
```ts
  const gridCols = version >= 6 ? input[20]! : DECK_GRID_COLS_DEFAULT;
  const gridRows = version >= 6 ? input[21]! : DECK_GRID_ROWS_DEFAULT;
  const homeTimeoutS = version >= 7 ? readU16(input, 22) : DECK_HOME_TIMEOUT_DEFAULT;
  // v7 widened the card's fixed part by one byte (flags2).
  const cardFixed = version >= 7 ? 17 : 16;
```

In the card loop, replace:
```ts
    if (offset + 16 > input.length) fail("truncated card body");
```
with:
```ts
    if (offset + cardFixed > input.length) fail("truncated card body");
```
replace:
```ts
    offset += 16;
```
with:
```ts
    const flags2 = version >= 7 ? input[offset + 16]! : 0;
    offset += cardFixed;
```
and in the pushed object, after `tightMargins: (flags & 128) !== 0,` add:
```ts
      scrollLabel: (flags2 & 1) !== 0,
```

Finally replace the return:
```ts
  return { accent, pages, gridCols, gridRows, cards };
```
with:
```ts
  return { accent, pages, gridCols, gridRows, homeTimeoutS, cards };
```

- [ ] **Step 4: Run the tests**

Run: `npm run test:web && npm run typecheck`
Expected: `pass 8 / fail 0`, and typecheck silent.

Note: the golden-fixture assertion (`encode matches golden fixture`) WILL fail at this point — `deck_basic.bin` is still v6. Task 2 regenerates it. If that is the only failure, proceed.

- [ ] **Step 5: Commit**

```bash
git add web/model/deck.ts tests/web/deck.test.ts
git commit -m "feat: DECK v7 — home timeout in the header, scrollLabel per card

The timeout is a u16 of seconds in the header (0 = off). scrollLabel
needed a new flags2 byte: the original flags byte has all 8 bits
assigned and the card's 16 fixed bytes were fully used.

v5 and v6 documents still decode, with both new fields defaulted."
```

---

## Task 2: Fixtures — freeze v6, regenerate v7

The point of a golden fixture is that it pins bytes. `gen_deck_fixture.js` currently regenerates `deck_basic.bin` AND `deck_v6_grid.bin` through `encodeDeck`, so bumping the version would silently rewrite both as v7 — destroying the only evidence that a v6 document still decodes. `deck_v6_grid.bin` must be frozen the way `deck_v5_pages.bin` already is.

**Files:**
- Modify: `scripts/gen_deck_fixture.js`
- Regenerate: `tests/firmware/fixtures/deck_basic.bin`
- Create: `tests/firmware/fixtures/deck_v7_timeout.bin`
- Leave untouched: `tests/firmware/fixtures/deck_v6_grid.bin`, `deck_v5_pages.bin`

- [ ] **Step 1: Add `scrollLabel` to the generator's card literals**

In `scripts/gen_deck_fixture.js`, add `scrollLabel: false,` after `tightMargins: false,` in **both** `BASIC` entries and in the single `GRID` entry.

- [ ] **Step 2: Freeze the v6 fixture and add a v7 one**

Replace this block:
```js
const gridOut = path.resolve(__dirname, "../tests/firmware/fixtures/deck_v6_grid.bin");
fs.writeFileSync(gridOut, Buffer.from(encodeDeck(GRID, -1, ["Home"], 8, 4)));
console.log(`wrote ${gridOut} (${fs.statSync(gridOut).size} bytes)`);
```
with:
```js
// NOTE: deck_v6_grid.bin is NOT regenerated either — as of DECK v7 it is the
// frozen v6 fixture proving a v6 document still decodes (grid bytes present,
// no timeout, no flags2). encodeDeck now emits v7, so regenerating it would
// rewrite the very thing it exists to test. GRID is kept above because the v7
// timeout fixture below uses the same card.

// A v7 document with a non-zero home timeout, pinning the 24-byte header and
// the widened card stride.
const timeoutOut = path.resolve(__dirname, "../tests/firmware/fixtures/deck_v7_timeout.bin");
fs.writeFileSync(timeoutOut, Buffer.from(encodeDeck(GRID, -1, ["Home"], 8, 4, 90)));
console.log(`wrote ${timeoutOut} (${fs.statSync(timeoutOut).size} bytes)`);
```

- [ ] **Step 3: Regenerate and confirm what changed**

```bash
npm run gen:fixtures
git status --short tests/firmware/fixtures/
```
Expected: `deck_basic.bin` **modified** (now v7), `deck_v7_timeout.bin` **new**, and `deck_v6_grid.bin` / `deck_v5_pages.bin` **unchanged**. If either of the latter two shows as modified, Step 2 was not applied — stop and fix it.

- [ ] **Step 4: Verify the version bytes directly**

```bash
for f in deck_basic deck_v5_pages deck_v6_grid deck_v7_timeout; do
  printf "%-18s version=%d\n" "$f" \
    "$(od -An -tu1 -j4 -N1 tests/firmware/fixtures/$f.bin | tr -d ' ')"
done
```
Expected exactly:
```
deck_basic         version=7
deck_v5_pages      version=5
deck_v6_grid       version=6
deck_v7_timeout    version=7
```

- [ ] **Step 5: Run the web tests**

Run: `npm run test:web`
Expected: `pass 8 / fail 0` — the golden-fixture assertion now matches again.

- [ ] **Step 6: Commit**

```bash
git add scripts/gen_deck_fixture.js tests/firmware/fixtures/
git commit -m "test: freeze deck_v6_grid.bin, add a v7 fixture

deck_basic.bin tracks the current format and is regenerated; the v5 and
v6 fixtures are frozen so they keep proving that older documents decode.
Without freezing v6, the version bump would have rewritten the evidence."
```

---

## Task 3: DECK v7 in the C++ codec

**Files:**
- Modify: `firmware/native/deck_document.h`
- Modify: `firmware/native/card_config.h`
- Test: `tests/firmware/deck_document_test.cpp`

- [ ] **Step 1: Write the failing test**

In `tests/firmware/deck_document_test.cpp`, add near the top of `main()` after the existing `using` lines:

```cpp
  // --- v7: home timeout + scrollLabel, and older documents still decode ---
  {
    std::ifstream f(DECK_V7_FIXTURE, std::ios::binary);
    assert(f && "v7 fixture opens");
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::vector<CardConfig> c;
    int gc = 0, gr = 0, timeout = -1;
    assert(tilehaus::decode_deck(b.data(), b.size(), c, nullptr, nullptr, &gc, &gr, &timeout));
    assert(gc == 8 && gr == 4);
    assert(timeout == 90);
    assert(c.size() == 1);
    assert(c[0].scroll_label == false);
  }
  {  // the frozen v6 fixture must still decode, with the new fields defaulted
    std::ifstream f(DECK_V6_FIXTURE, std::ios::binary);
    assert(f && "v6 fixture opens");
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::vector<CardConfig> c;
    int timeout = -1;
    assert(tilehaus::decode_deck(b.data(), b.size(), c, nullptr, nullptr, nullptr, nullptr, &timeout));
    assert(timeout == 0);
    assert(c[0].scroll_label == false);
  }
```

- [ ] **Step 2: Add the fixture define**

In `tests/firmware/CMakeLists.txt`, replace:
```cmake
  DECK_V6_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_v6_grid.bin")
```
with:
```cmake
  DECK_V6_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_v6_grid.bin"
  DECK_V7_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deck_v7_timeout.bin")
```

- [ ] **Step 3: Run it and watch it fail**

Run: `npm run test:firmware`
Expected: FAIL to compile — `decode_deck` takes no timeout parameter and `CardConfig` has no `scroll_label`.

- [ ] **Step 4: Implement**

In `firmware/native/card_config.h`, add to `struct CardConfig` beside the other booleans:
```cpp
  // v7 flags2 bit0: marquee the name on one line instead of wrapping it.
  bool scroll_label = false;
```

In `firmware/native/deck_document.h`:

Replace:
```cpp
inline constexpr uint16_t kDeckDocumentVersion = 6;
```
with:
```cpp
inline constexpr uint16_t kDeckDocumentVersion = 7;
```

After the `kDeckHeaderSizeV6` line add:
```cpp
inline constexpr size_t kDeckHeaderSizeV7 = 24;   // v7 appends a u16 home timeout
inline constexpr int kDeckHomeTimeoutDefault = 0; // seconds; 0 = never return home
```

Replace the `decode_deck` signature:
```cpp
inline bool decode_deck(const uint8_t *data, size_t len, std::vector<CardConfig> &out,
                        int32_t *out_accent = nullptr,
                        std::vector<std::string> *out_pages = nullptr,
                        int *out_grid_cols = nullptr, int *out_grid_rows = nullptr) {
```
with:
```cpp
inline bool decode_deck(const uint8_t *data, size_t len, std::vector<CardConfig> &out,
                        int32_t *out_accent = nullptr,
                        std::vector<std::string> *out_pages = nullptr,
                        int *out_grid_cols = nullptr, int *out_grid_rows = nullptr,
                        int *out_home_timeout_s = nullptr) {
```

Replace:
```cpp
  if (version != 5 && version != 6) return false;
  const size_t header_size = version == 6 ? kDeckHeaderSizeV6 : kDeckHeaderSizeV3;  // v5 == 20
```
with:
```cpp
  if (version != 5 && version != 6 && version != 7) return false;
  const size_t header_size = version == 7 ? kDeckHeaderSizeV7
                           : version == 6 ? kDeckHeaderSizeV6
                                          : kDeckHeaderSizeV3;  // v5 == 20
```

Replace:
```cpp
  const int grid_cols = version == 6 ? data[20] : kDeckGridColsDefault;
  const int grid_rows = version == 6 ? data[21] : kDeckGridRowsDefault;
```
with:
```cpp
  const int grid_cols = version >= 6 ? data[20] : kDeckGridColsDefault;
  const int grid_rows = version >= 6 ? data[21] : kDeckGridRowsDefault;
  const int home_timeout_s = version >= 7 ? static_cast<int>(read_u16(data + 22))
                                          : kDeckHomeTimeoutDefault;
  // v7 widened the card's fixed part by one byte (flags2).
  const size_t card_fixed = version >= 7 ? 17u : 16u;
```

In the card loop, replace:
```cpp
    if (offset + 16 > len) return false;
```
with:
```cpp
    if (offset + card_fixed > len) return false;
```
and replace:
```cpp
    card.page = data[offset + 15];
    offset += 16;
```
with:
```cpp
    card.page = data[offset + 15];
    const uint8_t flags2 = version >= 7 ? data[offset + 16] : 0;
    card.scroll_label = (flags2 & 0x01) != 0;
    offset += card_fixed;
```

Before the final `return true;` add:
```cpp
  if (out_home_timeout_s) *out_home_timeout_s = home_timeout_s;
```

- [ ] **Step 5: Run the tests**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 7`.

- [ ] **Step 6: Commit**

```bash
git add firmware/native/deck_document.h firmware/native/card_config.h tests/firmware/deck_document_test.cpp tests/firmware/CMakeLists.txt
git commit -m "feat: DECK v7 in the C++ codec

Mirrors the TS change byte for byte. decode_deck gains an optional
out_home_timeout_s; the card stride widens to 17 for v7 only, so the
frozen v5 and v6 fixtures still decode with the new fields defaulted."
```

---

## Task 4: The timeout policy, in a pure header

**Files:**
- Create: `firmware/native/home_timeout.h`
- Create: `tests/firmware/home_timeout_test.cpp`
- Modify: `tests/firmware/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/firmware/home_timeout_test.cpp`:

```cpp
#include <cassert>

#include "home_timeout.h"

int main() {
  using tilehaus::should_return_home;

  // Disabled is the default and must never fire, however idle the panel gets.
  assert(!should_return_home(0, 9999999, 2, false));
  assert(!should_return_home(0, 9999999, 2, true));
  assert(!should_return_home(-5, 9999999, 2, false));

  // On a subpage, past the threshold: fire.
  assert(should_return_home(30, 30000, 2, false));
  assert(should_return_home(30, 45000, 1, false));

  // On a subpage, below the threshold: do not.
  assert(!should_return_home(30, 29999, 2, false));

  // Already Home with nothing open: nothing to do, however long it has been.
  assert(!should_return_home(30, 999999, 0, false));

  // Home but a modal is open: still fire, so the modal gets closed.
  assert(should_return_home(30, 30000, 0, true));
  assert(!should_return_home(30, 29999, 0, true));

  // Exact boundary is inclusive — 30s means "at 30s".
  assert(should_return_home(1, 1000, 3, false));
  assert(!should_return_home(1, 999, 3, false));
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
foreach(t grid_layout sensor_format slider_map toggle_state tile_scale forecast_helpers home_timeout)
```

Run: `npm run test:firmware`
Expected: FAIL — `home_timeout.h: No such file or directory`.

- [ ] **Step 3: Implement**

Create `firmware/native/home_timeout.h`:

```cpp
#pragma once
#include <cstdint>

namespace tilehaus {

// Should the panel drop whatever it is showing and go back to Home?
//
// Pure on purpose — no LVGL, no globals — so the policy is host-tested and the
// caller is left with nothing but plumbing. The caller supplies LVGL's own
// inactivity clock (lv_display_get_inactive_time), which already counts drags
// on a slider as activity, not just taps.
//
//   timeout_s     deck setting in seconds; 0 (the default) disables the feature
//   inactive_ms   milliseconds since any input device activity
//   current_page  page_nav().current; 0 is Home
//   modal_open    whether an overlay is currently showing
//
// Fires on Home too when a modal is open, so a light modal left standing gets
// closed rather than sitting there all night.
inline bool should_return_home(int timeout_s, uint32_t inactive_ms,
                               int current_page, bool modal_open) {
  if (timeout_s <= 0) return false;
  if (current_page == 0 && !modal_open) return false;
  return inactive_ms >= static_cast<uint32_t>(timeout_s) * 1000u;
}

}  // namespace tilehaus
```

- [ ] **Step 4: Run the tests**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 8`.

- [ ] **Step 5: Commit**

```bash
git add firmware/native/home_timeout.h tests/firmware/home_timeout_test.cpp tests/firmware/CMakeLists.txt
git commit -m "feat: home-timeout policy in a pure, host-tested header

Keeps the decision out of the LVGL poll so its boundaries are testable
without hardware, the same shape as tile_scale.h."
```

---

## Task 5: Know whether a modal is open

Each card owns its own `Overlay` and calls `hide()` on it; nothing tracks which, if any, is showing. The timeout needs that answer both to close the modal and to avoid firing while someone is part-way through one on Home.

**Files:**
- Modify: `firmware/native/overlay.h`

- [ ] **Step 1: Add the registry**

In `firmware/native/overlay.h`, immediately before the `show()` method's doc comment block, add inside the namespace but outside the struct — place it directly after `namespace tilehaus {`:

```cpp
struct Overlay;

// The overlay currently showing, or nullptr. Modals are owned by individual
// cards with no registry between them, so this is the only way to ask "is a
// modal open" — which the inactivity timeout needs in order to close one and to
// avoid firing while the user is part-way through it.
inline Overlay *&active_overlay() {
  static Overlay *o = nullptr;
  return o;
}
```

- [ ] **Step 2: Record and clear it**

In `show()`, replace:
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

In `hide()`, replace:
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

- [ ] **Step 3: Confirm host tests are unaffected**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 8`. `overlay.h` includes LVGL and is not host-compiled; this only proves nothing else broke. Task 8 compiles it for real.

- [ ] **Step 4: Commit**

```bash
git add firmware/native/overlay.h
git commit -m "feat: track which overlay is showing

Modals are per-card with no registry, so there was no way to ask whether
one is open. The inactivity timeout needs it both to close the modal and
to avoid firing mid-interaction."
```

---

## Task 6: Wire the timeout into the firmware

**Files:**
- Modify: `firmware/native/card_host.h`
- Modify: `firmware/bindings.yaml`
- Modify: `firmware/hardware.yaml`

- [ ] **Step 1: Add the accessor and the tick**

In `firmware/native/card_host.h`, add these includes beside the existing ones:
```cpp
#include "home_timeout.h"
#include "overlay.h"
```

Then add, immediately after the `live_cards()` function's closing brace:

```cpp
// Deck-wide inactivity timeout in seconds, from the DECK header. 0 disables it,
// which is the default and what every pre-v7 deck decodes to.
inline int &deck_home_timeout_s() {
  static int seconds = 0;
  return seconds;
}

// Called from the 250ms interval in hardware.yaml. Deliberately does nothing
// until a deck sets a non-zero timeout, so the default costs one integer
// comparison per tick.
inline void tick_home_timeout() {
  const bool modal_open = active_overlay() != nullptr;
  if (!should_return_home(deck_home_timeout_s(),
                          lv_display_get_inactive_time(nullptr),
                          page_nav().current, modal_open)) {
    return;
  }
  if (modal_open) active_overlay()->hide();
  // Clear the stack as well as showing Home: PageNav::show() reveals the auto
  // back button whenever the stack is non-empty, so returning without clearing
  // leaves a back button on Home pointing at a page nobody navigated from.
  page_nav().stack.clear();
  page_nav().show(0);
}
```

- [ ] **Step 2: Store the timeout when the deck loads**

In `firmware/bindings.yaml`, find the `on_boot` lambda's deck-loading section and locate the `tilehaus::decode_deck(` call. Add `&timeout` as its final argument and assign it. If the existing call is:
```cpp
decode_deck(tilehaus::deck_store().data(), tilehaus::deck_store().length(), cards, &accent, &pages, &gc, &gr)
```
make it:
```cpp
decode_deck(tilehaus::deck_store().data(), tilehaus::deck_store().length(), cards, &accent, &pages, &gc, &gr, &timeout)
```
declaring `int timeout = 0;` beside the other locals, and after a successful decode add:
```cpp
tilehaus::deck_home_timeout_s() = timeout;
```

Read the file first and match its actual variable names — do not assume the ones above. Report what you found.

- [ ] **Step 3: Call the tick from the existing interval**

In `firmware/hardware.yaml`, find the `interval: - interval: 250ms` block whose lambda enables/disables indevs. Append this single line to the **end** of that same lambda, after the `for` loop's closing brace:

```cpp
          tilehaus::tick_home_timeout();
```

Keep it to this one call: the lambda also drives touch enable/disable, so anything that can fail here takes touch handling with it.

- [ ] **Step 4: Validate the config**

Run: `.venv-esphome/bin/esphome config firmware/tilehaus.yaml > /dev/null && echo OK`
Expected: `OK`.

- [ ] **Step 5: Commit**

```bash
git add firmware/native/card_host.h firmware/bindings.yaml firmware/hardware.yaml
git commit -m "feat: return to Home after the configured inactivity timeout

Polled from the 250ms interval that already exists rather than a new
timer, and dormant until a deck sets a non-zero value."
```

---

## Task 7: Per-tile label scrolling (closes #2)

**Files:**
- Modify: `firmware/native/card_style.h`

- [ ] **Step 1: Implement the scroll long-mode**

In `firmware/native/card_style.h`, replace the whole `add_name` function with:

```cpp
inline lv_obj_t *add_name(lv_obj_t *cell, const CardFonts &fonts,
                          const std::string &title, bool hidden = false,
                          lv_align_t align = LV_ALIGN_BOTTOM_LEFT,
                          int dx = 0, int dy = 0, bool scroll = false) {
  if (hidden || tile_compact()) return nullptr;  // no room for a label on a 1x1
  const lv_font_t *font = tile_body_font(fonts);
  // The shadow twin is skipped in scroll mode: it would have to marquee in
  // lockstep with the label to stay behind it, and any drift smears the text.
  // A moving label is legible enough without the depth cue.
  if (!scroll) add_text_shadow(cell, font, title, true, align, dx, dy);
  lv_obj_t *lbl = lv_label_create(cell);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
  if (font) lv_obj_set_style_text_font(lbl, font, 0);
  if (scroll) {
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL);
  } else {
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  }
  lv_obj_set_width(lbl, LV_PCT(100));
  lv_label_set_text(lbl, title.c_str());
  lv_obj_align(lbl, align, dx, dy);
  return lbl;
}
```

- [ ] **Step 2: Pass the flag from every card**

Every card calls `add_name(cell, fonts, cfg.title, cfg.hide_label)` or a variant with alignment arguments. The new `scroll` parameter is last and defaults to `false`, so only cards that should honour the flag need changing — which is all of them. Run:

```bash
grep -rn "add_name(cell, fonts" firmware/native/*.h
```

For each call site, append `cfg.scroll_label` as the final argument. Where a call currently ends `cfg.hide_label)`, make it `cfg.hide_label, LV_ALIGN_BOTTOM_LEFT, 0, 0, cfg.scroll_label)`. Where it already passes alignment and offsets, append `, cfg.scroll_label`.

`back_card.h` and `page_card.h` also call `add_name` and have a `cfg` in scope — include them.

- [ ] **Step 3: Confirm host tests still pass**

Run: `npm run test:firmware`
Expected: `100% tests passed out of 8`.

- [ ] **Step 4: Commit**

```bash
git add firmware/native/
git commit -m "feat: per-tile label scrolling (closes #2)

add_name gains a scroll long-mode driven by the v7 scrollLabel flag. The
shadow twin is skipped when scrolling — keeping it in lockstep is what
would smear the text, and a moving label does not need the depth cue."
```

---

## Task 8: Configurator support

**Files:**
- Modify: `web/editor_state.ts`
- Modify: `web/deck_client.ts`
- Modify: `web/editor_view.ts`
- Modify: `web/card_fields.ts`

- [ ] **Step 1: Add the state**

Read `web/editor_state.ts` first. Add `homeTimeoutS: number;` to the state interface beside `accent`, default it to `DECK_HOME_TIMEOUT_DEFAULT` in the initial state, and add `scrollLabel: false,` to the new-card factory beside `tightMargins`.

- [ ] **Step 2: Thread it through the client**

In `web/deck_client.ts`, add `homeTimeoutS: number;` to the document-shaped interfaces that already carry `accent`, add it to the `applied` result variant, and add a trailing `homeTimeoutS: number = 0` parameter to `save(...)` which is passed through to `encodeDeck(cards, accent, pages, gridCols, gridRows, homeTimeoutS)`.

- [ ] **Step 3: Add the editor field**

In `web/editor_view.ts`, beside the existing deck accent control (search for "Deck accent colour"), add a number input labelled **Home timeout (s)** with `min="0"` and `max="65535"`, bound to `state.homeTimeoutS`, with helper text `0 = never return to Home`. Pass `state.homeTimeoutS` in the `client.save(...)` call and read it back from the load result the way `state.accent` already is.

- [ ] **Step 4: Add the per-tile checkbox**

In `web/card_fields.ts`, beside the existing `hideLabel` flag definition, add:
```ts
    defs.push({ key: "scrollLabel", label: "Scroll label", hint: "Marquee a long name on one line instead of wrapping it." });
```
and extend the field read/write helpers at the bottom of the file to handle `scrollLabel` exactly as `hideLabel` is handled.

- [ ] **Step 5: Verify**

Run: `npm run typecheck && npm run test:web && npm run build`
Expected: typecheck silent, `pass 8 / fail 0`, bundle written.

- [ ] **Step 6: Commit**

```bash
git add web/
git commit -m "feat: configurator fields for the home timeout and label scrolling"
```

---

## Task 9: Build, flash, verify on the panel

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
Expected: `INFO Successfully compiled program.` This is the first real check of Tasks 5, 6 and 7 — none of those files are host-compiled.

- [ ] **Step 3: Flash**

```bash
.venv-esphome/bin/esphome upload firmware/tilehaus.yaml --device 192.168.252.221 2>&1 | tail -4
```
Expected: `INFO OTA successful`.

- [ ] **Step 4: Confirm the panel returns with its deck**

```bash
until curl -s -m 3 -o /dev/null http://192.168.252.221/api/v1/config; do sleep 3; done
node scripts/deck_cli.js status --device 192.168.252.221 | head -2
```
Expected: the stored deck decodes and reports its pages and tile count. The deck is still v6 on the device and must decode unchanged — that is the compatibility guarantee working in the field.

- [ ] **Step 5: Verify on the glass**

Wait 60s after the OTA before pushing any config (safe-mode rollback risk). Then push a deck with `homeTimeoutS: 30` and check:

| check | expected |
|---|---|
| navigate to a subpage, wait 30s | returns to Home |
| after returning | no back button on Home |
| open a light modal, wait 30s | modal closes, lands on Home |
| set `homeTimeoutS: 0`, wait 2 min on a subpage | never moves |
| a tile with **Scroll label** on and a long title | marquees on one line |

- [ ] **Step 6: Commit any tuning**

```bash
git add -A firmware web
git commit -m "fix: tune the home timeout against the panel"
```

---

## Self-Review Notes

**Spec coverage.** Design "DECK v7" → Tasks 1, 2, 3. "Idle detection" and "modal registry" → Tasks 5, 6. "Policy in a pure header" → Task 4. "Configurator" → Tasks 7, 8. "Testing" → Tasks 1, 3, 4 (host) and 9 (device).

**Type consistency.** `homeTimeoutS` (TS) / `home_timeout_s` (C++ parameter) / `deck_home_timeout_s()` (accessor); `scrollLabel` (TS) / `scroll_label` (C++); `should_return_home(int, uint32_t, int, bool)`; `active_overlay()`; `tick_home_timeout()`. Each is defined once and spelled consistently at every use.

**Known gaps.** `overlay.h`, `card_host.h`, `card_style.h`, `bindings.yaml` and `hardware.yaml` are not host-compiled, so Tasks 5, 6 and 7 are proven only by the compile in Task 9. Task 6 Step 2 deliberately instructs the implementer to read `bindings.yaml` and match its real variable names rather than trusting the illustrative snippet.
