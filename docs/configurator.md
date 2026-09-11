# Configurator

The device serves a browser-based configurator at its own IP address (`/`).
It loads the current deck (or starts empty on an unconfigured panel), and
lets you edit pages, tiles, and deck-wide settings before saving.

## Pages

The deck is organized into pages. Page 0 is always **Home** and can't be
renamed away from being the first page or deleted. The page bar across the
top lets you:

- Switch pages by clicking a page chip.
- Add a page with **+ Page**. A new page starts with a single **Back** tile
  (type 22) so it's never a dead end — tap it to pop back to the page you
  came from.
- Rename the current page (any page except Home) with the text field next to
  its chip.
- Delete the current page (any page except Home) with **Delete page**. Tiles
  on a deleted page are removed, and later pages shift down to fill the gap.

Use a **Page** tile (type 21) on another page to link to a specific page by
name; see the [card reference](cards.md#types).

## The grid

Tiles sit on a grid, sized as **columns × visible rows**. The default is
10×6. The maximum is derived from the panel's own display resolution (fetched
from the device as `X-Display-Width`/`X-Display-Height` when the configurator
loads) divided by a minimum usable cell size — so you can't set a grid finer
than the screen can actually render. Shrinking the grid can push existing
tiles off it; use **Auto-arrange** to repack tiles to the top-left and clear
gaps.

A page can have more rows of tiles than fit on screen at once — the panel
shows the configured number of visible rows and a taller deck simply scrolls.

## Adding, moving, and resizing tiles

- **+ Add tile**: pick a card type from the dropdown (listed alphabetically
  by label; the underlying type numbers are fixed and don't reflect this
  order) and click **+ Add tile**. New tiles auto-place into the first open
  grid slot on the current page.
- **Drag**: tiles can be dragged directly in the grid pane to reposition
  them.
- **Resize**: drag a tile's corner in the grid pane, or set exact **Width**
  and **Height** (in grid cells) in the tile's settings pane. Width is capped
  at the grid's column count.
- **Select**: click a tile to open its settings in the form pane on the
  right.
- **Delete**: with a tile selected, use **Delete tile** in the settings pane.

## Per-tile fields

Every card type exposes an **Entity** field (a Home Assistant entity ID) and
a **Title** (the label shown on the tile) — except a few types that repurpose
those fields for something else:

- **Header** (11) uses all three text fields as entity IDs: a greeting
  template sensor, a subtitle template sensor (in the Title slot), and a
  weather entity.
- **Weather** (7) uses Entity 2 and Icon (alt) as optional `input_number`
  helpers holding today's forecast high/low.
- **Page** (21) uses Entity 2 as a page picker (the target page) instead of
  an entity ID, and Entity as an optional tracked entity for the tile's
  on/off tint.
- **Back** (22) only has a Title field.

See the full [card reference](cards.md) for the field layout and flags of
each type.

Entity fields are typeahead: as you type, matching entity IDs from the
connected Home Assistant instance appear in a dropdown. Picking (or leaving)
an entity on the primary **Entity** field auto-fills a blank Title and icon
from that entity's Home Assistant metadata, without overwriting anything
you've already typed.

Every tile also has:

- **Width / Height** — size in grid cells.
- **Icon / Icon (alt)** — pick from the panel's built-in icon set. Icon (alt)
  is used for the tile's alternate state (light off, cover open, unlocked,
  etc.) where the card type supports it; blank falls back to the primary
  icon.
- **Active colour / Inactive colour** — override the tile's tint for its
  on/active and off/inactive states, or leave on "card default" to use the
  built-in color.
- Type-specific flags such as **Hide label**, **Detail chevron**, **Follow
  colour**, and **Transparent background** — see
  [Card reference](cards.md#flags) for which types support which flags.

## Home Assistant connection

The Home Assistant bar (collapsed once connected) holds:

- **Base URL** — your HA instance, e.g. `http://homeassistant.local:8123`.
- **Long-lived access token** — generate one from your HA user profile.

Click **Connect**. Credentials are saved in the browser's local storage so
they survive a reload. Once connected, the bar shows the entity count and
entity fields gain live typeahead.

If the connection fails, the most common cause is CORS: Home Assistant needs
to allow requests from the configurator's origin. Add the panel's origin to
`http.cors_allowed_origins` in HA's `configuration.yaml` and restart HA.

## Accent colour

The **Accent** color picker in the toolbar sets a deck-wide accent used by
the panel's built-in card colors. It previews live in the configurator page
itself. The reset button (⟲) clears it back to the built-in default (amber).

## Save = reboot-to-apply

**Save** validates the current deck, writes it to the device (`PUT
/api/v1/config`), and the panel reboots to apply it — cleanly rebuilding the
whole card tree and its Home Assistant subscriptions rather than patching
live state. The configurator polls the device until it's back and reloads
the applied deck.

**Reset** clears the stored deck entirely (`DELETE /api/v1/config`) and
reboots the panel back to the "Awaiting Configuration" screen.

If the deck changed on the device between when you loaded the configurator
and when you click Save (someone else saved, or you pushed a config via the
API), Save reports a conflict and keeps your edits — press Save again to
retry against the now-current state.
