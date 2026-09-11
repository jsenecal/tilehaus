---
name: flash
description: Build the Tilehaus web UI bundle, compile the ESPHome firmware, and OTA-flash it to a panel. Use when deploying firmware/ or web/ changes to a device.
---

# Flash Tilehaus firmware to a panel

Default device: `192.168.252.221` — override with the target panel's IP.

1. Rebuild the served UI bundle (whenever `web/` changed):
   ```bash
   npm run build
   ```
2. Compile the firmware:
   ```bash
   .venv-esphome/bin/esphome compile firmware/tilehaus.yaml
   ```
3. OTA-flash it (port 3232, no password):
   ```bash
   .venv-esphome/bin/esphome upload firmware/tilehaus.yaml --device <ip>
   ```
4. The panel reboots into the new firmware.

> **After flashing, wait >60s before pushing any config** (`push-config` / `reset`).
> The P4 can roll back to safe mode if it reboots again too soon after an OTA.
> Confirm the panel is serving again with the `panel-status` skill first.

Notes:
- If ESPHome isn't installed:
  `python3 -m venv .venv-esphome && .venv-esphome/bin/pip install esphome`.
- A green compile is **not** device-confirmed testing — verify on the physical panel.
- USB flashing (first bring-up / a bricked unit) uses the same config with a serial
  `--device /dev/serial/by-id/...` path instead of the network IP.
