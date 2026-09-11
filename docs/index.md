# Tilehaus

Tilehaus turns an affordable ESP32-P4 touchscreen into a wall-mounted Home
Assistant control panel. The UI is native LVGL running directly on the
device — no cloud dependency, no companion app, no browser tab left open
somewhere.

> **Screenshot placeholder** — a photo of a JC1060P470 panel running a tile
> deck belongs here.

## The tile deck

You arrange a deck of tiles — lights, covers, climate, sensors, scenes, and
more — across one or more pages. Each tile binds to a Home Assistant entity.
Layout and bindings are edited from a browser-based configurator that the
device itself serves at its own IP address. When you save, the device writes
the new configuration to on-device storage as a compact binary document and
reboots into it.

## Features

- **Native LVGL UI** — renders directly on the panel, no companion app or
  cloud round-trip for the display itself.
- **Browser-based configurator** — served by the device; no separate install,
  just open its IP address.
- **22 card types** — toggles, sliders, covers, climate, locks, alarms,
  weather, scenes, navigation tiles, and more. See the
  [card reference](cards.md).
- **Multi-page decks** — organize tiles across pages, with dedicated
  navigation tiles to move between them.
- **Home Assistant native** — tiles bind straight to entity IDs; the
  configurator's entity fields look them up live from your HA instance.
- **Reboot-to-apply persistence** — layout changes are written to on-device
  storage and applied by a clean reboot, so there's no partial or live-patched
  state to reason about.
- **Simple config API** — the same binary document the configurator writes is
  readable and writable over a small HTTP API, so layouts can be scripted.
  See [Config API](config-api.md).

## Where to go next

- New to Tilehaus? Start at [Getting started](getting-started.md).
- Picking or wiring up a panel? See [Hardware](hardware.md).
- Building and flashing firmware? See [Flashing](flashing.md).
- Using the browser configurator? See [Configurator](configurator.md).
- Looking up what a card type does? See [Card reference](cards.md).
- Scripting layouts or integrating with the device API? See
  [Config API](config-api.md).
- Coming from EspControl, or weighing the two? See
  [vs EspControl](comparison.md) for a feature-parity comparison.

## Credits

Tilehaus is an independent project inspired by
[EspControl](https://github.com/jtenniswood/espcontrol); it reuses its
product concepts and visual language but shares no firmware code.
