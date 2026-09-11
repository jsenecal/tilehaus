# Flashing

Firmware lives at `firmware/tilehaus.yaml`, built with [ESPHome](https://esphome.io/).
The web configurator's compiled assets are embedded into the firmware image,
so a change to `web/` needs a rebuild before it's flashed.

## One-time setup

Tool versions are pinned via [mise](https://mise.jdx.dev/) (Node 24, Python
3.13) in `mise.toml`. ESPHome is not a Node dependency — install it into a
local Python virtualenv:

```bash
python3 -m venv .venv-esphome
.venv-esphome/bin/pip install esphome
```

Install the Node dependencies too:

```bash
npm ci
```

## Build the web configurator asset

Whenever `web/` changes, rebuild the embedded configurator asset before
compiling firmware:

```bash
npm run build
```

This bundles `web/` with esbuild into a gzipped asset embedded as
`firmware/native/deck_ui_asset.h`, which the firmware serves at `/`.

## Compile

```bash
.venv-esphome/bin/esphome compile firmware/tilehaus.yaml
```

## First flash (USB)

A blank panel needs its first flash over USB-C, targeting the device's
stable by-id serial path (not a bare `/dev/ttyACMn`, which can shift if
other USB serial devices are attached):

```bash
.venv-esphome/bin/esphome upload firmware/tilehaus.yaml \
  --device /dev/serial/by-id/usb-...
```

## OTA (subsequent flashes)

Once the panel has joined Wi-Fi, flash over the air by IP address. OTA uses
port 3232 with no password:

```bash
.venv-esphome/bin/esphome upload firmware/tilehaus.yaml --device <panel-ip>
```

## Logs

```bash
.venv-esphome/bin/esphome logs firmware/tilehaus.yaml --device <panel-ip>
```

## Safety: wait after an OTA flash

After an OTA flash completes, **do not push a config change (`PUT` or
`DELETE` to `/api/v1/config`) for at least 60 seconds.** The P4 tracks boot
stability across reboots; if it reboots again too soon after an OTA update,
it can trip into safe mode instead of coming back up on the new firmware.
Let the panel settle for a minute after any OTA flash before triggering a
config-apply reboot from the configurator or `scripts/deck_cli.js`.

## Tests

```bash
npm run typecheck
npm run test:web
npm run test:firmware
```
