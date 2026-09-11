# Tilehaus

Tilehaus turns an affordable ESP32-P4 touchscreen into a wall-mounted Home
Assistant control panel. The UI is native LVGL running directly on the
device — no cloud dependency, no companion app.

## The tile deck

You arrange a deck of tiles — lights, covers, climate, sensors, scenes, and
more — across one or more pages. Each tile binds to a Home Assistant entity.
Layout and bindings are edited from a browser-based configurator that the
device itself serves. When you save, the device writes the new configuration
and reboots into it.

Ready to set one up? Head to [Getting started](getting-started.md).

## Credits

Tilehaus is an independent project inspired by
[EspControl](https://github.com/jtenniswood/espcontrol); it reuses its
product concepts and visual language but shares no firmware code.
