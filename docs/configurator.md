# Configurator

The device serves a browser-based configurator at its own IP address (`/`).
It loads the current deck (or starts empty on an unconfigured panel), and
lets you edit pages, tiles, and deck-wide settings before saving.

It is laid out like the panel looks: pages and deck settings on the left,
a **preview** of the panel's screen in the middle, and the selected tile's
settings on the right. On a narrow screen the three stack.

## The preview

The preview is the panel's screen at its real resolution, scaled to fit:
tiles are placed with the firmware's own grid arithmetic and drawn with its
colours, fonts and sizes, so a tile looks here the way it will on the glass.

- Connected to Home Assistant, tiles show **live states**: a light that is on
  is lit, a sensor shows its reading (number large, unit small), a cover's
  shade sits at its position, a gauge's dial fills. States refresh every 30
  seconds.
- A tile bound to an entity Home Assistant doesn't know gets a red outline and
  an **UNKNOWN ENTITY** tag, before you save it to the panel.
- **Day / Night** switches the page behind the tiles to the panel's night mode.
- Rows past the screen's bottom edge are marked with a dashed line; on the
  panel they scroll.

## Pages

The deck is organized into pages. Page 0 is always **Home** and can't be
renamed away from being the first page or deleted. The page list on the left
(with each page's tile count) lets you:

- Switch pages by clicking one.
- Add a page with **+ New page**. A new page starts with its **Back button**
  on, so it's never a dead end.
- Rename the current page (any page except Home) with the text field under
  the list.
- Turn the current page's **Back button** on or off (any page except Home).
  The panel draws it in the page's top-left corner: a black square with an
  arrow, like the one on the full-screen views. If the page has a title in its
  top-left tile (a transparent Blank in row 0), the button sits beside the
  title; otherwise it floats in the corner, over the grid, and stays put while
  the page scrolls. Tapping it goes back to the page you came from, or Home.
  Turn it off to place a **Back** tile (type 22) of your own instead, or to
  have no way back on that page.
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
- **Drag**: tiles can be dragged directly in the preview to reposition
  them.
- **Swap**: drag a tile and release it over another tile on the same page, and
  the two trade places. Each takes the other's spot and size, so the layout
  stays the same. The other tile is outlined while you hover.
- **Resize**: drag the selected tile's corner handle in the preview, or set exact **Width**
  and **Height** (in grid cells) in the tile's settings pane. Width is capped
  at the grid's column count.
- **Select**: click a tile to open its settings on the right.
- **Delete**: with a tile selected, use its ✕ in the preview, or **Delete
  tile** in its settings.

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
- **Icon / Icon (alt)** — pick from the panel's built-in icon set (the picker
  works without an internet connection). Icon (alt)
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

The Home Assistant section at the bottom of the left column (collapsed once
connected) holds:

- **Base URL** — your HA instance, e.g. `http://homeassistant.local:8123`.
- **Long-lived access token** — generate one from your HA user profile.

Click **Connect**. Credentials are saved in the browser's local storage so
they survive a reload. Once connected, it shows the entity count, entity
fields gain live typeahead, and the preview shows live states.

If the connection fails, the most common cause is CORS: Home Assistant needs
to allow requests from the configurator's origin. Add the panel's origin to
`http.cors_allowed_origins` in HA's `configuration.yaml` and restart HA.

## Accent colour

The **Accent** colour picker under **Deck** sets the colour lit tiles use on
the panel. The preview and the configurator's own buttons follow it live.
**Reset** puts it back to the panel's lime.

## Undo and backups

- **Undo / Redo** (or <kbd>Ctrl</kbd>+<kbd>Z</kbd> and
  <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd>) step through your edits since
  the page loaded. A burst of typing or a colour drag counts as one step.
  Inside a text field, <kbd>Ctrl</kbd>+<kbd>Z</kbd> undoes the text instead.
  Undo also brings back a deck you just reset — press Save to put it back on
  the panel.
- **Export** downloads the deck as JSON (`tilehaus-office-2026-10-23.json`).
  It is the same spec `scripts/deck_cli.js push` takes.
- **Import** loads a JSON file into the editor. It is checked first, so a
  file the panel would reject is refused with the reason. Nothing reaches the
  panel until you press Save, and Undo reverts the import. Use Export on one
  panel and Import on another to copy a layout.
- Closing the tab with unsaved changes asks for confirmation.

## Unlocking

The panel only accepts saves while its **Configurator** switch is on in Home
Assistant. While it is off, **Save** and **Reset** are replaced by a
*Locked* notice; turn the switch on and they come back within a few seconds
(or click the notice to check right away). You can keep editing while it is
locked — nothing is lost.

Your first Save claims the panel for this browser. Until the switch goes off,
other devices see *In use from another device* in place of Save. The switch
turns itself off after 30 minutes without use; editing in an open configurator
counts as use once you have saved. See
[Config API](config-api.md#unlocking-the-panel-for-writes) for the details.

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
