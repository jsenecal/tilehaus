# Tilehaus

A native-LVGL Home Assistant control panel for affordable ESP32-P4
touchscreens.

Inspired by [EspControl](https://github.com/jtenniswood/espcontrol).

## What it is

Tilehaus turns a cheap touchscreen panel into a wall-mounted Home Assistant
control surface. You arrange a deck of tiles (lights, covers, climate,
sensors, scenes, and more) across one or more pages, using a browser-based
configurator that the device itself serves. The UI is native LVGL running
directly on the ESP32-P4 — there's no cloud dependency and no companion app.
Changes made in the configurator are saved to the device and applied by
rebooting into the new layout.

## Hardware

Targets ESP32-P4 touchscreen panels, such as the Guition JC1060P470. The
firmware is built with [ESPHome](https://esphome.io/).

## Quick start

1. Flash `firmware/tilehaus.yaml` to your panel with ESPHome.
2. Open the device's IP address in a browser — it serves the tile
   configurator at `/`.
3. Arrange tiles, assign Home Assistant entities to them, and organize pages.
4. Click **Save**. The device writes the new configuration and reboots to
   apply it.

## Development

### Prerequisites

Tool versions are pinned via [mise](https://mise.jdx.dev/) in `mise.toml`:
Node 24 and Python 3.13. Install mise, then run `mise install` in the repo
root, or otherwise ensure matching versions are on your `PATH`.

ESPHome is not a Node/npm dependency — install it into a local Python
virtualenv, the same pattern EspControl uses with its `.venv-esphome/`:

```bash
python3 -m venv .venv-esphome
. .venv-esphome/bin/activate
pip install esphome
```

Then install the Node dependencies:

```bash
npm ci
```

### npm scripts

| Script                  | What it does                                                                      |
| ------------------------ | ----------------------------------------------------------------------------------- |
| `npm run build`          | esbuild bundles `web/` into a gzipped asset embedded as `firmware/native/deck_ui_asset.h` |
| `npm run typecheck`      | Type-checks the web configurator sources with `tsc --noEmit`                       |
| `npm run test:web`       | Runs the web unit tests (`node --test`)                                            |
| `npm run test:firmware`  | Configures and builds the firmware host tests (CMake) and runs them with `ctest`   |
| `npm run gen:icons`      | Regenerates the icon catalog used by the configurator                              |
| `npm run gen:fixtures`   | Regenerates firmware test fixtures for the deck binary format                      |

### Compiling the firmware

With the ESPHome venv active:

```bash
esphome compile firmware/tilehaus.yaml
```

The device serves the configurator (`/`, `/app.js`) and a config API at
`/api/v1/config` (media type `application/vnd.tilehaus.deck`); posting a new
config there is applied by reboot.

## Repo layout

```
firmware/    ESPHome YAML + native LVGL C++ headers (firmware/native/)
web/         TypeScript browser configurator, bundled into the firmware image
scripts/     Build/codegen scripts (deck UI bundling, icon catalog, fixtures)
tests/       Web unit tests (tests/web/) and firmware host tests (tests/firmware/)
docs/        Zensical documentation site sources
```

## Credits

Tilehaus is an independent project inspired by
[EspControl](https://github.com/jtenniswood/espcontrol); it reuses its
product concepts and visual language but shares no firmware code.

Licensed under [PolyForm Noncommercial 1.0.0](LICENSE).
