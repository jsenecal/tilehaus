---
name: reset
description: Clear a Tilehaus panel's stored config (DELETE /api/v1/config), returning it to the awaiting-config screen. Use to wipe a device's layout.
---

# Reset the panel config

```bash
node scripts/deck_cli.js reset --device <ip>
```

`GET`s the current `ETag`, `DELETE`s `/api/v1/config` with `If-Match`, and the panel
reboots to the **awaiting-config** screen (no layout). Default device
`192.168.252.221`.

> **Not within 60s of an OTA flash** (safe-mode rollback risk). Confirm the panel is
> up (`panel-status`) first.
