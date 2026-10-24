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

## Unlocking the panel for writes

Reading is always open. Writing (`PUT` and `DELETE`) needs the panel unlocked
from Home Assistant, and only one client can write at a time:

1. Turn on the panel's **Configurator** switch in Home Assistant (a config
   entity on the device page, e.g. `switch.tilehaus_office_configurator`).
2. The first client to claim a session (`POST /api/v1/session`) gets a random
   128-bit key. The configurator and `deck_cli.js` claim automatically on
   their first save.
3. Every write must send that key as `Authorization: Bearer <key>`. Other
   clients get `409` on a claim and `401` on a write.
4. The session lasts until the switch goes off. Turn it off by hand to revoke
   the key, or let it time out after **30 minutes without configurator use**
   (a claim, a write, or a session check with the key). The timeout is the
   `configurator_timeout` substitution, in minutes; `0` disables it.

The switch and the key survive reboots, so a save (which reboots the panel)
does not end the session. The idle timer restarts at boot.

The **Configurator status** sensor shows `Locked`, `Open`, or
`In use by <ip>`, so you can see which device holds the session.

!!! warning
    The API is plain HTTP. The lock stops other devices on your network and
    accidental pushes from writing, but someone capturing your Wi-Fi traffic
    could read the key while a session is open. Leave the switch off when you
    are not configuring.

### `GET /api/v1/session`

```json
{"state": "claimed", "mine": true, "timeoutMinutes": 30}
```

`state` is `locked`, `open` (unlocked, nobody has claimed it) or `claimed`.
`mine` is true when the request's `Authorization` header carries the claimed
key; a check with a valid key also counts as use and restarts the idle timer.

### `POST /api/v1/session`

- **201 Created** — `{"key": "<32 hex chars>"}`. Keep it; it is not shown again.
- **409 Conflict** — another client already holds the session.
- **423 Locked** — the Configurator switch is off.

## Endpoints

The config itself lives at `/api/v1/config`.

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
| `X-Deck-Max-Type` | The newest card type this firmware renders (e.g. `25`). Firmware too old to send it knows types up to `22`. A deck with a newer type is refused. |

### `PUT /api/v1/config`

Writes a new document.

- Request headers: `Authorization: Bearer <key>` (see
  [Unlocking the panel](#unlocking-the-panel-for-writes)),
  `Content-Type: application/vnd.tilehaus.deck`, `If-Match:
  "<generation>"` (required — the generation you last read via `GET`).
- **204 No Content** on success, with a new `ETag`. The panel then reboots to
  apply the new deck.
- **409 Conflict** if `If-Match` doesn't match the device's current
  generation (someone else changed it first) — re-`GET` and retry.
- **400/415/428** on a malformed body, wrong content type, or a missing/bad
  `If-Match`. A rejected body's 400 says why, e.g. `Deck not saved: tile 3 is
  card type 24, but this firmware knows types up to 22: update the panel's
  firmware`.
- **401 Unauthorized** without the session key; **423 Locked** while the
  Configurator switch is off. Both are checked before anything else.

### `DELETE /api/v1/config`

Clears the stored deck (needs the session key and `If-Match`, same rules as `PUT`).

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
- `session` — show whether the panel is locked, open or claimed, and whether
  this machine holds the session.
- `build <spec.json> <out.bin>` — encode a spec to a `.bin` file offline
  (with a round-trip decode check), without talking to a device.

`--device <host>` names the panel (`tilehaus-office.local`, or an IP) and is
required for every command except `build`. `push` checks the deck against the
panel's `X-Deck-Max-Type` first and stops, naming the tile, if the panel's
firmware is too old for one of its cards.

`push` and `reset` need the Configurator switch on. The first one claims the
session and caches its key per device in `~/.config/tilehaus/sessions.json`
(or under `$XDG_CONFIG_HOME`), so later pushes reuse it until the switch goes
off. `--key <hex>` uses a key you already have instead.

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
node scripts/deck_cli.js push my-deck.json --device tilehaus-office.local
```

This reads `my-deck.json`, encodes it to a DECK document, fetches the
panel's current `ETag`, `PUT`s the new document with that `If-Match`, and
polls the panel until it comes back up after the apply reboot.
