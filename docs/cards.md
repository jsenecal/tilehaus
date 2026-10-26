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
| 1 | Sensor | Read-only value display. A number is drawn as large as the tile allows with its unit smaller beside it (`23.4` big, `°C` small); a text state uses the regular faces. The name sits below. | — |
| 2 | Toggle | Tap to turn an entity on/off; tints to the on/off colour. A detail chevron can open a popup with more controls. | — |
| 3 | Light (slider) | Whole-tile vertical fill slider. For `light.*` entities it maps to brightness (0–100%, `light.turn_on`/`off`); for `number.*`/`input_number.*` entities it maps to the entity's raw value range (`number.set_value`). Press and hold for a moment (the readout appears), then drag; a quick touch changes nothing, so brushing the tile or starting a swipe on it is safe. The readout flashes on every change. | — |
| 4 | Light (control) | Tile look (icon + name, tints on/off) that opens a full light-controls modal (brightness, color, etc.) on tap. | — |
| 5 | Cover | Whole-tile fill slider anchored to the top, mirroring a shade's position: dragging down lowers/closes it. Like Light (slider), it acts only once pressed and held for a moment. Backed by the cover's open-percent state. | — |
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
| 23 | Action | Momentary tile, like Scene. The entity's domain picks what a tap does: runs a `script` (and tints while it runs), triggers an `automation`, activates a `scene`, presses a `button`/`input_button`, or turns anything else on. With no entity it fires an `esphome.tilehaus_action` event — see [Action events](#action-events). | Script, automation, scene or button (Entity, optional), Event name (Entity 2) |
| 24 | Timer | A `timer` helper as a countdown ring: the ring shows the share of the duration left, and the tile tints while it runs. Tap starts, pauses or resumes; a long press cancels. On a 3×3 or larger tile the state (Running, Paused, Ready) is also shown top-left. The panel counts down between Home Assistant updates, from the timer's end time. | Timer entity (Entity) |
| 25 | Garage door / gate | A `cover` with one big action. Tap asks before opening or closing it; while it moves, a tap stops it. A caption reads `OPEN · 14 MIN`, `CLOSING` and so on, and after 10 minutes open the tile turns amber (the Active colour). Gates get gate icons from their device class. The minutes count from when the panel saw the door open, so a door already open at boot shows just `OPEN`. | Garage door or gate (Entity) |
| 26 | Gauge | A numeric sensor as a 240° dial with its value inside: battery, humidity, CO₂, power. The dial fills with the accent, or red past the warning threshold. On a 3×3 or larger tile a caption shows the range, or LOW / HIGH while warning. | Sensor (Entity), Range (Entity 2): `min..max`, optionally `<n` or `>n`, e.g. `0..100 <20`. Blank = a range for the unit (`%` 0–100, `°C` 0–40, `ppm` 400–2000 warning above 1000, `W` 0–3000, `kW` 0–10). |
| 27 | Clock | The panel's own time, or a city's for a world clock. A caption shows the date (`FRI 24 OCT`), or for a world clock the day there and its offset from here (`FRI · +13 H`), with a sun or moon for day or night there. Keeps time on the panel, so it runs without Home Assistant once the clock has synced. | Time zone (Entity 2): picked from a city list in the configurator, stored as a POSIX TZ rule (e.g. `JST-9`); blank = the panel's own time. No Entity field |
| 28 | Wi-Fi share | The guest network as a QR code: a phone camera pointed at it offers to join. Tap for a full-screen page with a large code and the network name and password in text. The network is set on the panel's **Guest Wi-Fi name** and **Guest Wi-Fi password** entities in Home Assistant, never in the deck (which anyone on the network can read); changing them updates the code at once. An empty password makes an open network. | Title only — no Entity field |
| 29 | Media | A `media_player` as now playing: the track, the artist and the state (`PLAYING`, `PAUSED`), tinted while playing. Any player works, Music Assistant's included. Tap plays or pauses. With nothing loaded (idle or off) the tile shows only its state and name, never the last track. A 4×2 tile adds the time (`1:23 / 4:56`), a progress line and previous / play / next buttons; a 3×4 or 4×4 tile adds a larger title and a volume slider that, like the tile sliders, needs a press and hold before it moves. The position counts on the panel between Home Assistant updates. Once the cover loads it fills the tile under a dark wash, in place of the playing tint; see [Media covers](#media-covers). The arrows in the top-right corner open the full view: the cover, title, artist and album, the progress, shuffle / previous / play / next / repeat, and the volume. It stays open until you close it: the panel doesn't return Home from it when idle, though it still dims and sleeps. | Media player (Entity) |

## Action events

An Action tile with no entity fires a Home Assistant event instead of calling
a service, so a tap can drive any automation without a helper entity. The
event is `esphome.tilehaus_action`, with:

| Field | Value |
|---|---|
| `action` | The tile's **Event name**, or its Title when that is blank |
| `panel` | The panel's ESPHome name, e.g. `tilehaus-office` |

```yaml
triggers:
  - trigger: event
    event_type: esphome.tilehaus_action
    event_data:
      action: doorbell_chime
```

Add `panel: tilehaus-office` to `event_data` to react to one panel only.

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

## Media covers

The Media card downloads the cover from the player's `entity_picture` and
decodes it on the panel. Music Assistant's covers are full URLs and need no
setup; the panel asks Music Assistant for the nearest size it serves (80,
160, 256, 512 or 1024 px) and scales it the rest of the way.

Some covers are served by Home Assistant itself, as a path such as
`/api/media_player_proxy/...`. For those the panel needs Home Assistant's
address: set the `ha_url` substitution in the panel's YAML, for example
`ha_url: "http://homeassistant.local:8123"`. Without it, those players show
the placeholder.

The panel decodes baseline JPEG only. A PNG or progressive JPEG cover, a
failed download, or a cover over 2 MB shows the placeholder too, and the
panel's log says why (`tilehaus.art`).

## Flags

Every card gets **Transparent background**, dropping the tile's fill so it
blends with the page (useful for section headers or spacers). Beyond that,
flags are type-specific:

| Flag | Applies to | Effect |
|------|-----------|--------|
| **Hide label** | Sensor, Toggle, Light (slider), Light (control), Cover, Presence, Scene, Button, Lock, Climate, WLED, Number slider, Option select, Fan, Alarm, Back, Action, Timer, Garage door / gate, Gauge, Clock, Wi-Fi share, Media (1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 13, 15, 16, 17, 18, 22, 23, 24, 25, 26, 27, 28, 29) | Shows only the icon, no name text. |
| **Detail chevron** | Toggle, Light (slider), Cover, Number slider, Fan (2, 3, 5, 15, 17) | Adds a corner button opening the entity's detail popup. |
| **Follow colour** | Light (slider), Light (control) (3, 4) | Tints the tile (and its modal/sliders) to the light's actual color instead of the fixed active colour. |
| **Show weather** | Header (11) | Shows the weather glyph + temperature; off = text-only header. |
| **Show clock** | Header (11) | Shows the clock + date; off = text-only header. |
| **Show hi/lo** | Header (11) | Shows today's forecast high/low beside the header's weather temperature. |
| **Reduced margins** | Blank (0) | Shrinks the tile's inset so icon + text sit close to the edges. |

Verified against `web/card_types.ts` (the wire enum + labels) and
`web/card_fields.ts` (per-type fields and flags) as of this writing.
