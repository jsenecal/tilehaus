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
| 7 | Weather | Read-only weather tile: a condition-driven icon, the condition name as the label, today's high/low, and current temperature in the corner. | Forecast-high helper (Entity 2), Forecast-low helper (Icon alt) — optional `input_number` entities, see [Weather helpers](#weather-helpers) |
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
| 19 | Weather forecast | Read-only multi-day forecast — one column per day (weekday, condition glyph, high, low), reading a compact string an HA automation writes into an `input_text` from `weather.get_forecasts` — see [Weather helpers](#weather-helpers). | — |
| 20 | Camera | **Reserved — removed.** Not offered in the configurator's type list. A deck that somehow stores this value renders as a Blank tile instead. | — |
| 21 | Page | Navigation tile: tap opens another page. Can optionally track an entity for an on/off tint, like Toggle. | Tracked entity (Entity, optional), Target page (Entity 2, picked from existing pages) |
| 22 | Back | Navigation tile that pops back to the previous page. Every new page starts with one so it's never a dead end. | Title only — no Entity field |

## Weather helpers

Three cards show forecast data the panel cannot fetch itself — it has no HTTP
client for the weather service and no way to call `weather.get_forecasts`. Home
Assistant does the work and parks the result in helper entities the panel
subscribes to like any other state.

Skip this if you only use the Weather card's current condition and temperature;
those come straight from the weather entity and need no helpers.

### Today's high/low — Weather (7) and Header (11)

Both read two `input_number` helpers, and both resolve them the same way: the
ids are **derived from the weather entity**, so a deck that names its helpers by
convention configures nothing.

```
weather.forecast_home  ->  input_number.forecast_home_temp_high
                           input_number.forecast_home_temp_low
```

The weather entity is the tile's own **Entity** on Weather (7), and **Entity 2**
on Header (11).

To use different ids, override them per tile. The field differs only because of
what each card has spare — Weather already uses Icon for its condition glyph,
while Header draws no icon of its own:

| Card | High override | Low override |
|---|---|---|
| Weather (7) | Entity 2 | Icon (alt) |
| Header (11) | Icon | Icon (alt) |

Override the high alone and the low follows it, by swapping the last `high` in
the id for `low` — so `input_number.my_high` implies `input_number.my_low`.

A helper that does not exist simply never pushes a state, so the line stays
blank rather than erroring. That is also what you see if the ids do not match
the convention and you have not set an override.

Create both as Number helpers (Settings → Devices & services → Helpers), min
`-60`, max `60`, step `0.1`.

### The multi-day forecast string — Weather forecast (19)

This card does **not** take a weather entity. It takes an `input_text` whose
state is a compact, pre-rendered string, because parsing a forecast service
response on-device is not something the tile can do:

```
Sat|partlycloudy|22|20;Sun|rainy|24|18;Mon|partlycloudy|20|14
```

Days are separated by `;`, fields within a day by `|`, in the order
**weekday, condition, high, low**. A day with fewer than four fields is skipped.
The weekday is shown verbatim, so it is whatever the automation writes.

The condition must be one of Home Assistant's standard values, which map to the
bundled glyphs: `clear`, `clear-night`, `cloudy`, `exceptional`, `fog`, `hail`,
`lightning`, `lightning-rainy`, `partlycloudy`, `pouring`, `rainy`, `snowy`,
`snowy-rainy`, `sunny`, `windy`, `windy-variant`. Anything else falls back to a
default glyph.

> **Set the helper's max length to 255.** `input_text` defaults to 100
> characters and five days runs just over that, which truncates the last day
> mid-field.

### One automation for all of it

Fills the forecast string and both high/low helpers from a single
`weather.get_forecasts` call:

```yaml
alias: Panel forecast helpers
triggers:
  - trigger: time_pattern
    minutes: "/30"
  - trigger: homeassistant
    event: start
actions:
  - action: weather.get_forecasts
    target:
      entity_id: weather.forecast_home
    data:
      type: daily
    response_variable: fc
  - variables:
      days: "{{ fc['weather.forecast_home'].forecast }}"
  - action: input_text.set_value
    target:
      entity_id: input_text.forecast_home_daily
    data:
      value: >-
        {%- set out = namespace(parts=[]) -%}
        {%- for d in days[:5] -%}
          {%- set out.parts = out.parts + [
               (d.datetime | as_datetime | as_local).strftime('%a')
               ~ '|' ~ d.condition
               ~ '|' ~ (d.temperature | round(0) | int)
               ~ '|' ~ (d.templow | round(0) | int)
             ] -%}
        {%- endfor -%}
        {{ out.parts | join(';') }}
  - action: input_number.set_value
    target:
      entity_id: input_number.forecast_home_temp_high
    data:
      value: "{{ days[0].temperature | round(1) }}"
  - action: input_number.set_value
    target:
      entity_id: input_number.forecast_home_temp_low
    data:
      value: "{{ days[0].templow | round(1) }}"
mode: single
```

Swap `weather.forecast_home` for your own weather entity, and rename the helpers
to match its object id — the Header's derivation depends on it.

Trim `days[:5]` if your forecast tile is narrower; five columns need about four
grid columns of width to stay legible.

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
