# Tilehaus vs. EspControl

Tilehaus is an independent sibling to
[EspControl](https://github.com/jtenniswood/espcontrol) — it reuses EspControl's
product concepts and visual language but shares no firmware code. EspControl is the
mature, broad product; Tilehaus is a newer, native-LVGL implementation that covers
the core control / status / navigation cards and adds a freely configurable tile
grid.

This page is an honest feature-parity comparison so you know what's covered today.
Legend: **✓** supported · **◑** partial / different approach · **✗** not yet.

!!! note
    Gaps below reflect Tilehaus's earlier stage, not permanent exclusions — except
    where noted as deliberate (e.g. the Camera card was removed). Tilehaus's own
    advantages are called out at the end.

## Card types

EspControl exposes ~40 card variants; Tilehaus has 28 wire types (one reserved).
Grouped by capability:

| Capability | EspControl | Tilehaus | Notes |
|---|:-:|:-:|---|
| Lights (brightness / switch / temperature / full control) | ✓ | ✓ | Tilehaus: **Light (slider)** + **Light (control)** with a colour/temperature modal and follow-colour tinting |
| Switches / plugs | ✓ | ✓ | Tilehaus: **Toggle** |
| Scenes / scripts / buttons | ✓ | ✓ | Tilehaus: **Action** runs a script (tinting while it runs), triggers an automation, activates a scene or presses a button; **Scene** and **Button** remain |
| Trigger (fire an HA event) | ✓ | ✓ | Tilehaus: an **Action** tile with no entity fires `esphome.tilehaus_action` |
| Webhook / Push / Local action | ✓ | ✗ | — |
| Option select | ✓ | ✓ | |
| Sensor | ✓ | ✓ | Tilehaus also has a **Gauge** dial with a range and a warning threshold |
| Local sensor (on-device) | ✓ | ✗ | |
| Doors & Windows | ✓ | ✓ | Tilehaus: **Door/Window** (read-only contact) |
| Presence | ✓ | ✓ | |
| Slider (number / input_number) | ✓ | ✓ | Tilehaus: **Number slider** |
| Fans | ✓ | ✓ | Tilehaus: **Fan** with a speed/preset modal |
| Cover / blinds | ✓ | ✓ | Tilehaus: **Cover** with a position modal |
| Garage door / Gate | ✓ | ✓ | Tilehaus: **Garage door / gate** — confirm before open/close, tap to stop, open-time caption and an amber overdue tint |
| Vacuum / Lawn mower | ✓ | ✗ | — |
| Lock | ✓ | ✓ | |
| Alarm | ✓ | ✓ | Tilehaus: **Alarm** with an on-screen PIN keypad |
| Climate / HVAC | ✓ | ✓ | Tilehaus: **Climate** with an arc/target modal |
| Weather | ✓ | ✓ | |
| Weather forecast | ✓ | ✓ | |
| Date & time / World clock / Timezone / Calendar | ✓ | ◑ | Tilehaus: **Clock** (local or a world clock from a city list) plus the **Header** clock; no calendar card |
| Camera / image | ✓ | ✗ | Deliberately removed from Tilehaus (full-res JPEG decode froze the UI) — type 20 stays reserved |
| Timer | ✓ | ✓ | Tilehaus: **Timer** countdown ring; tap to start/pause, hold to cancel |
| Media player | ✓ | ✗ | No media card (art, transport, volume, progress) |
| Internal switches / relays | ✓ | ✗ | On-panel relays not exposed |
| Screen lock | ✓ | ✗ | |
| Wi-Fi sharing (guest QR) | ✓ | ✓ | Tilehaus: **Wi-Fi share** — the network is set from Home Assistant, never stored in the deck; tap for a full-screen code with the password in text |
| Subpages / navigation | ✓ | ✓ | Tilehaus: **Page** + **Back** tiles for multi-page navigation |
| Section header / spacer | ◑ | ✓ | Tilehaus: **Header** and **Blank** (transparent/section) tiles |
| WLED | ✗ | ✓ | Tilehaus has a dedicated **WLED** card + modal |

## Platform & configurator

| Feature | EspControl | Tilehaus | Notes |
|---|:-:|:-:|---|
| Supported panels | ✓ (6: four ESP32-P4 + one S3 + the 86-panel) | ◑ (ESP32-P4 JC1060P470) | Tilehaus targets one P4 panel today; others not yet in a device catalog |
| Browser web installer (first flash) | ✓ | ✗ | Tilehaus flashes via the ESPHome CLI (USB then OTA) |
| Browser configurator (drag/drop, pages, tile sizes) | ✓ | ✓ | Tilehaus's configurator is served by the panel and works without internet access |
| Configurable grid size (columns × visible rows) | ✗ | ✓ | Tilehaus lets you set the deck grid; EspControl uses fixed per-device card slots |
| Apply changes | ✓ (without reflashing) | ◑ (reboot-to-apply) | Tilehaus reboots to apply a new config; there is no live in-place rebuild |
| Backup & restore / copy to another panel | ✓ | ✓ | Tilehaus: **Export**/**Import** a JSON backup in the configurator (the same spec `deck_cli.js` pushes), plus undo/redo while editing |
| Saved-config migration & compatibility contract | ✓ | ◑ | Tilehaus versions the DECK format (v5/v6) but has a leaner migration story |
| Display scheduling (idle / night / presence) | ✓ | ✓ | Tilehaus: return-home, dim and sleep timeouts and presence wake built in; night schedules are Home Assistant automations on the **Night mode** switch (see [Panel settings](panel-settings.md)) |
| Screensaver / brightness / sleep | ✓ | ✓ | Tilehaus: awake/dim/sleep brightness levels and a full-screen clock screensaver, set from Home Assistant |
| Protecting the configuration | ◑ (PIN for the touchscreen) | ✓ | Tilehaus: writes need a **Configurator** switch turned on in Home Assistant; the first client to claim it is the only one that can save, and it locks itself after 30 minutes idle. No touchscreen PIN yet |
| Appearance settings (icons, labels, active colour, rotation, temp units, clock) | ✓ | ◑ | Tilehaus: per-tile icons/labels/colours, a deck accent colour (overridable live from a Home Assistant colour light) and hi/lo temps; no rotation/temp-unit UI |
| Automatic firmware updates | ✓ | ✗ | Tilehaus updates are manual OTA |
| Language / i18n | ✓ | ✗ | Tilehaus is English-only |
| Local control, no cloud | ✓ | ✓ | Both talk to Home Assistant on your LAN |
| Home Assistant connection | ESPHome native API (auto-discovered in HA) | ESPHome native API for control; the configurator uses a HA long-lived token for entity typeahead | |

## What Tilehaus does differently

- **Native LVGL UI** — the panel renders with LVGL directly, with rich interaction
  modals: a light colour/temperature tab set, a climate target arc, an alarm PIN
  keypad, and fan / cover / WLED controls.
- **Configurable tile grid** — set the deck's columns × visible rows; the editor
  clamps to a display-derived maximum, and the panel re-clamps authoritatively.
- **Per-tile colour + follow-colour** — active/inactive tint per tile, a deck-wide
  accent, and lights that tint the tile to their real colour.
- **Composite Header tile** — greeting text, a weather cluster, and a clock/date in
  one tile, with optional hi/lo temperatures.
- **Self-contained config** — the whole layout is one small binary DECK document
  served and stored by the device over a simple HTTP API.

## Summary

If you want the widest device and card coverage today — media, vacuum, garage,
camera, world clocks, built-in display schedules, a web installer, and
translations — **EspControl** is the mature choice. If you want a native-LVGL panel
with a freely configurable tile grid and rich on-device control modals on an
ESP32-P4, **Tilehaus** covers the core and is growing.
