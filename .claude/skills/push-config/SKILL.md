---
name: push-config
description: Encode a Tilehaus DECK document from a JSON spec and push it to a panel's /api/v1/config (reboot-to-apply). Use when setting or changing a panel's tiles, pages, or grid.
---

# Push a deck config to the panel

The panel stores its layout as a binary DECK document. `scripts/deck_cli.js` encodes
a JSON spec via `web/model/deck.ts` and performs the ETag / `If-Match` / reboot-wait
handshake, mirroring the in-browser configurator's save path.

**First, resolve entity IDs with ha-mcp** (`mcp__ha-mcp__ha_search`,
`mcp__ha-mcp__ha_get_state`) so each tile's `entity` / `entity2` binds to a real
Home Assistant entity — don't guess IDs.

1. Write a deck JSON (shape below), e.g. `deck.json`.
2. Validate it offline first (encodes + round-trip decodes, no device needed):
   ```bash
   node scripts/deck_cli.js build deck.json /tmp/deck.bin
   ```
3. Push it to the panel:
   ```bash
   node scripts/deck_cli.js push deck.json --device <ip>
   ```
   This GETs the current `ETag`, PUTs with `If-Match` +
   `Content-Type: application/vnd.tilehaus.deck`, then waits for the panel to reboot
   back up. On `409 Conflict` it reports the device's current ETag — re-run to retry.

> **Do not push within 60s of an OTA flash** (safe-mode rollback risk). Default
> device `192.168.252.221`.

## Deck JSON shape

```json
{
  "pages": ["Home", "Lights"],
  "gridCols": 10,
  "gridRows": 6,
  "accent": -1,
  "cards": [
    { "type": 2, "entity": "light.desk", "title": "Lamp",
      "w": 2, "h": 2, "col": 0, "row": 0, "page": 0 }
  ]
}
```

- A bare array is also accepted as the `cards` list (defaults: pages `["Home"]`,
  grid `10x6`, accent `-1`).
- Card fields mirror `DeckCard` in `web/model/deck.ts` (`type`, `entity`, `title`,
  `entity2`, `icon`, `iconAlt`, `w`, `h`, `col`, `row`, `page`, `align`, colours, and
  the boolean flags). Card `type` numbers are the frozen wire enum in
  `web/card_types.ts`.
