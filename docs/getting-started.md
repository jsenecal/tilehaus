# Getting started

This is the happy path from unboxed panel to a working tile deck.

## 1. Get the hardware

Tilehaus targets ESP32-P4 touchscreen panels, primarily the Guition
JC1060P470 (1024×600). See [Hardware](hardware.md) for details on the board,
its Wi-Fi setup, and power/USB-C requirements.

## 2. Flash the firmware

Build and flash `firmware/tilehaus.yaml` with the ESPHome CLI over USB for
the first flash, then over-the-air (OTA) afterward. Full steps, including the
one-time toolchain setup, are in [Flashing](flashing.md).

```bash
.venv-esphome/bin/esphome compile firmware/tilehaus.yaml
.venv-esphome/bin/esphome upload firmware/tilehaus.yaml --device /dev/serial/by-id/...
```

## 3. Open the configurator

Once the panel boots and joins Wi-Fi, it shows an "Awaiting Configuration"
screen with its own IP address. Open that address in a browser — the device
serves the tile configurator at its web root (`/`).

## 4. Connect Home Assistant

In the configurator, open the Home Assistant bar and enter your HA base URL
(e.g. `http://homeassistant.local:8123`) and a long-lived access token. Once
connected, entity fields on tiles offer live autocomplete against your HA
entities. See [Configurator](configurator.md#home-assistant-connection) for
details, including the CORS setup HA needs.

## 5. Add tiles

Pick a card type, add it to the deck, and assign it a Home Assistant entity.
Arrange tiles on the page grid, add more pages if you want, and set titles,
icons, and colors as needed. See [Configurator](configurator.md) for the full
editing workflow and [Card reference](cards.md) for what each card type does.

## 6. Save

Click **Save**. The device validates and writes the new configuration, then
reboots to apply it — the panel comes back up showing your updated deck.

That's the core loop: edit in the browser, Save, the panel reboots into the
new layout. Repeat as your dashboard evolves.
