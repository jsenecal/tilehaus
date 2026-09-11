---
name: panel-status
description: Read a Tilehaus panel's current config — ETag, reported display size, and the decoded deck (pages, grid, tiles). Use to inspect what a device is running.
---

# Panel status

```bash
node scripts/deck_cli.js status --device <ip>
```

`GET`s `/api/v1/config` and prints:
- the `ETag` (config generation) and `X-Display-Width` / `X-Display-Height`;
- if configured, the DECK document decoded via `web/model/deck.ts` — pages, grid
  size, accent, and each tile (type, size, position, page, entity, title);
- if `204`, reports **awaiting config** (no layout stored).

Default device `192.168.252.221`. Read-only — safe to run anytime, including right
after an OTA to confirm the panel is back up before pushing a config.
