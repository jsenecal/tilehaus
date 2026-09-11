# Card reference

The deck's wire format uses a fixed numeric type per card — these values are
frozen and never renumbered, so a saved deck stays valid across firmware
updates. The configurator lists card types alphabetically by label; the
numbers below are the underlying (unordered) wire values.

Every card has an **Entity** and **Title** field unless noted otherwise, plus
**Width**/**Height**, **Icon**/**Icon (alt)**, **Active colour**/**Inactive
colour**, and a **Transparent background** flag. See
[Flags](#flags) below for which additional flags each type supports, and
[Configurator](configurator.md#per-tile-fields) for how fields behave in the
editor.

## Types

| # | Name | What it does | Fields (beyond Entity/Title) |
|---|------|---------------|-------------------------------|
| 0 | Blank | A plain tile: an optional icon + title, positioned by horizontal/vertical alignment. With **Transparent** on, it works as a section header or spacer. | Horizontal align, Vertical align, Reduced margins |
| 1 | Sensor | Read-only numeric/text value display — the value in the icon slot, the name below it. | — |
| 2 | Toggle | Tap to turn an entity on/off; tints to the on/off colour. A detail chevron can open a popup with more controls. | — |
| 3 | Light (slider) | Whole-tile vertical fill slider. For `light.*` entities it maps to brightness (0–100%, `light.turn_on`/`off`); for `number.*`/`input_number.*` entities it maps to the entity's raw value range (`number.set_value`). Drag or tap-to-glide; a centered readout flashes on change. | — |
| 4 | Light (control) | Tile look (icon + name, tints on/off) that opens a full light-controls modal (brightness, color, etc.) on tap. | — |
| 5 | Cover | Whole-tile fill slider anchored to the top, mirroring a shade's position: dragging down lowers/closes it. Backed by the cover's open-percent state. | — |
| 6 | Presence | Read-only occupancy tile — icon + name, tinted occupied vs. clear. Not tappable. | — |
| 7 | Weather | Read-only weather tile: a condition-driven icon, the condition name as the label, today's high/low, and current temperature in the corner. | Forecast-high helper (Entity 2), Forecast-low helper (Icon alt) — optional `input_number` entities |
| 8 | Scene | Momentary action tile — press-dip and a one-shot flash on tap, then calls `scene.turn_on`. Stateless. | — |
| 9 | Button | Same as Scene, but calls `button.press`. | — |
| 10 | Lock | Icon + name, tinted secure/unwarn. Locking is immediate; unlocking asks for confirmation first. | — |
| 11 | Header | A transparent, full-width header: a greeting line + subtitle on the left (both HA template sensors, shown verbatim), and an on-device clock/date plus live weather on the right. | Greeting entity (Entity), Subtitle entity (Title), Weather entity (Entity 2), Show weather, Show clock, Show hi/lo |
| 12 | Climate | Icon + name, tinted warm while actively heating (`hvac_action`). Tap opens the climate control modal. | — |
| 13 | WLED | Icon + name, tinted while the strip is on. Tap opens the WLED control modal. | — |
| 14 | Door/Window | Read-only binary contact-status tile. Uses the same renderer as Presence — an icon/colour swap between open and closed, using whatever icons/colours you configure. | — |
| 15 | Number slider | The same whole-tile fill-slider renderer as Light (slider), for `number.*`/`input_number.*` entities (domain auto-detected). | — |
| 16 | Option select | Icon + name + the current option, centered. Tap opens a full-screen scrollable pick-list for a `select` entity. | — |
| 17 | Fan | Icon + name, tinted while running. Tap toggles on/off; a detail chevron opens speed/preset-mode controls. | — |
| 18 | Alarm | Shield icon + name + current state (e.g. "Armed Away"), tinted red while armed/arming/triggered. Tap opens an arm/disarm modal. | — |
| 19 | Weather forecast | Read-only multi-day forecast — one column per day (weekday, condition glyph, high, low), reading a compact string an HA automation writes into an `input_text` from `weather.get_forecasts`. | — |
| 20 | Camera | **Reserved — removed.** Not offered in the configurator's type list. A deck that somehow stores this value renders as a Blank tile instead. | — |
| 21 | Page | Navigation tile: tap opens another page. Can optionally track an entity for an on/off tint, like Toggle. | Tracked entity (Entity, optional), Target page (Entity 2, picked from existing pages) |
| 22 | Back | Navigation tile that pops back to the previous page. Every new page starts with one so it's never a dead end. | Title only — no Entity field |

## Flags

Every card gets **Transparent background**, dropping the tile's fill so it
blends with the page (useful for section headers or spacers). Beyond that,
flags are type-specific:

| Flag | Applies to | Effect |
|------|-----------|--------|
| **Hide label** | Sensor, Toggle, Light (slider), Light (control), Cover, Presence, Scene, Button, Lock, Climate, WLED, Number slider, Option select, Fan, Alarm, Back (1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 13, 15, 16, 17, 18, 22) | Shows only the icon, no name text. |
| **Detail chevron** | Toggle, Light (slider), Cover, Number slider, Fan (2, 3, 5, 15, 17) | Adds a corner button opening the entity's detail popup. |
| **Follow colour** | Light (slider), Light (control) (3, 4) | Tints the tile (and its modal/sliders) to the light's actual color instead of the fixed active colour. |
| **Show weather** | Header (11) | Shows the weather glyph + temperature; off = text-only header. |
| **Show clock** | Header (11) | Shows the clock + date; off = text-only header. |
| **Show hi/lo** | Header (11) | Shows today's forecast high/low beside the header's weather temperature. |
| **Reduced margins** | Blank (0) | Shrinks the tile's inset so icon + text sit close to the edges. |

Verified against `web/card_types.ts` (the wire enum + labels) and
`web/card_fields.ts` (per-type fields and flags) as of this writing.
