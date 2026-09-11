# Config API

The device stores its tile deck as a versioned binary document (media type
`application/vnd.tilehaus.deck`) and exposes it at `/api/v1/config`, served
by the same device alongside the browser configurator. The configurator
itself is just a client of this API — anything it can do, you can do by
talking to the endpoint directly.

## The DECK document

A DECK document encodes deck-wide settings (grid size, accent colour, page
names) plus the list of cards (type, entity bindings, title, size, position,
icons, colors, and flags — see [Card reference](cards.md) for what each card
type stores). It's compact, versioned, and decoded/encoded with the same
codec on both the web configurator and the firmware, so what you push is
exactly what the panel renders.

## Endpoints

All three verbs live at `/api/v1/config`.

### `GET /api/v1/config`

Returns the currently stored document.

- **200** — body is the binary DECK document, `Content-Type:
  application/vnd.tilehaus.deck`.
- **204 No Content** — the panel has no config stored yet (fresh device,
  showing "Awaiting Configuration").

Response headers on every GET:

| Header | Meaning |
|--------|---------|
| `ETag` | Current config generation, quoted (e.g. `"3"`). Use it as `If-Match` on the next write. |
| `X-Display-Width` | The panel's display width in pixels. |
| `X-Display-Height` | The panel's display height in pixels. |

### `PUT /api/v1/config`

Writes a new document.

- Request headers: `Content-Type: application/vnd.tilehaus.deck`, `If-Match:
  "<generation>"` (required — the generation you last read via `GET`).
- **204 No Content** on success, with a new `ETag`. The panel then reboots to
  apply the new deck.
- **409 Conflict** if `If-Match` doesn't match the device's current
  generation (someone else changed it first) — re-`GET` and retry.
- **400/415/428** on a malformed body, wrong content type, or a missing/bad
  `If-Match`.

### `DELETE /api/v1/config`

Clears the stored deck (guarded by `If-Match`, same rules as `PUT`).

- **204 No Content** on success. The panel reboots to the "Awaiting
  Configuration" screen.
- **409 Conflict** on a stale `If-Match`.

## Reboot-to-apply

Both `PUT` and `DELETE` apply by rebooting the panel — there's no live,
in-place rebuild of the running deck. This is deliberate: ESPHome's Home
Assistant API client can't cleanly unsubscribe from entities, so a clean
reboot is what re-establishes correct HA bindings for the new deck. Expect
the panel to briefly go offline and come back up after a successful write.

## Safety: wait after an OTA flash

If you've just OTA-flashed the panel, **wait at least 60 seconds before
pushing a config change** (`PUT` or `DELETE`). A config-apply reboot too soon
after an OTA reboot can trip the P4's boot-stability check into safe mode.
This only matters right after a firmware flash — routine config pushes on an
already-settled panel are unaffected.

## `scripts/deck_cli.js`

A small Node CLI wraps the same codec the web configurator uses, so you can
inspect or script a panel's deck without the browser:

```bash
node scripts/deck_cli.js <status|push <spec.json>|reset|build <spec.json> <out.bin>> [--device <ip>]
```

- `status` — GET the current config and print pages, grid size, accent, and
  each card's type/size/position/entity/title.
- `push <spec.json>` — encode a deck spec and PUT it (using the current
  ETag), then poll until the panel is back up.
- `reset` — DELETE the stored config.
- `build <spec.json> <out.bin>` — encode a spec to a `.bin` file offline
  (with a round-trip decode check), without talking to a device.

`--device <ip>` targets a specific panel; it defaults to `192.168.252.221`.

### Deck JSON shape

```json
{
  "pages": ["Home", "Bedroom"],
  "gridCols": 10,
  "gridRows": 6,
  "accent": -1,
  "cards": [
    {
      "type": 3,
      "entity": "light.office_desk_lamp",
      "title": "Desk Lamp",
      "entity2": "",
      "icon": "",
      "iconAlt": "",
      "w": 2,
      "h": 2,
      "col": 0,
      "row": 0,
      "page": 0,
      "align": 5,
      "activeColor": -1,
      "inactiveColor": -1,
      "hideLabel": false,
      "detail": true,
      "followColor": true,
      "showHilo": false,
      "transparent": false,
      "showWeather": true,
      "showClock": true,
      "tightMargins": false
    }
  ]
}
```

`accent` of `-1` uses the built-in default colour; `activeColor`/
`inactiveColor` of `-1` use the card's built-in colour for that state. A spec
file can also be a bare array of cards instead of the full object — in that
case `pages`, `gridCols`, `gridRows`, and `accent` fall back to their
defaults (`["Home"]`, `10`, `6`, `-1`).

### Example

```bash
node scripts/deck_cli.js push my-deck.json --device 192.168.252.221
```

This reads `my-deck.json`, encodes it to a DECK document, fetches the
panel's current `ETag`, `PUT`s the new document with that `If-Match`, and
polls the panel until it comes back up after the apply reboot.
