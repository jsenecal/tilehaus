# Getting started

## Supported hardware

Tilehaus targets ESP32-P4 touchscreen panels, such as the Guition
JC1060P470. The firmware is built with [ESPHome](https://esphome.io/).

## Flash the firmware

Compile and flash `firmware/tilehaus.yaml` with the ESPHome CLI:

```bash
esphome run firmware/tilehaus.yaml
```

(Use `esphome compile firmware/tilehaus.yaml` if you only want to build.)
This installs the firmware and connects the panel to your Wi-Fi network as
configured in `firmware/secrets.yaml`.

## Open the configurator

Once the panel boots and joins your network, find its IP address (check your
router, or the ESPHome logs) and open it in a browser. The device serves the
tile configurator at its web root.

## Add your first tile

In the configurator:

1. Add a tile to a page.
2. Pick a tile type (light, cover, climate, sensor, scene, and so on).
3. Assign it a Home Assistant entity.
4. Arrange it on the page grid alongside your other tiles.

## Save and apply

Click **Save**. The device writes the new configuration to storage and
reboots to apply it — the panel comes back up showing your updated deck.

A full reference of card types and device layouts is planned as a follow-up
to this page.
