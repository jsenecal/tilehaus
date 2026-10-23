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
`tilehaus-office.local` (office) or `tilehaus-lobby.local` (lobby).

> **Unlock first.** Writes need the panel's **Configurator** switch on. Turn it
> on with ha-mcp (`mcp__ha-mcp__ha_call_service`, `switch.turn_on`, e.g.
> `switch.tilehaus_office_configurator`; confirm the id with `ha_search`). The CLI
> claims a session on the first write and caches the key in
> `~/.config/tilehaus/sessions.json`; the switch turns itself off after 30 min
> without use. "panel is locked" means the switch is off; "another client holds"
> means someone else claimed it — cycle the switch only if the user agrees.
> `node scripts/deck_cli.js session --device <host>` shows the state.

> **Not within 60s of an OTA flash** (safe-mode rollback risk). Confirm the panel is
> up (`panel-status`) first.
