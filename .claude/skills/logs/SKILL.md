---
name: logs
description: Tail a Tilehaus panel's device logs over the network via ESPHome. Use to debug runtime behaviour on the device.
---

# Panel logs

```bash
.venv-esphome/bin/esphome logs firmware/tilehaus.yaml --device <ip>
```

Streams the device log over the ESPHome API (Ctrl-C to stop). Default device
`192.168.252.221`. Use this to watch boot, Wi-Fi/HA connection, and card runtime
behaviour — e.g. after a `flash` or a `push-config`.
