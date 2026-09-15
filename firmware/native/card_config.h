#pragma once
#include <cstdint>
#include <string>

namespace tilehaus {

enum class CardType {
  Blank, Sensor, Toggle, LightSlider, LightControl, Cover,
  Presence, Weather, Scene, Button, Lock, Header, Climate, Wled, DoorWindow,
  NumberSlider, OptionSelect, Fan, Alarm, WeatherForecast,
  Camera,  // reserved (20): card removed; keeps Page/Back at 21/22
  Page, Back
};

struct CardConfig {
  CardType type = CardType::Blank;
  std::string entity;
  std::string title;
  int w = 0, h = 0;      // sub-unit size; 0 = the type's default
  std::string entity2;
  std::string icon;      // MDI glyph (UTF-8), empty = no icon
  std::string icon_alt;  // alternate-state glyph (light off / cover open); empty = no swap
  // Per-tile background colors for state-tinting tiles (Toggle, LightControl,
  // Lock, Presence, action tiles). 24-bit hex (0xRRGGBB); -1 = use the card's
  // built-in default. "active" = on / occupied / locked / flash; "inactive" =
  // off / clear / unlocked / rest.
  int32_t active_color = -1;
  int32_t inactive_color = -1;
  bool hide_label = false;  // true = draw the icon only, no on-tile name label
  // Opt-in "more-info" affordance: a small top-right chevron that opens the
  // entity's detail modal (when one exists for its domain). Slider/Toggle tiles.
  bool detail = false;
  // Opt-in: when on, tint the tile with the light's actual current colour
  // (from rgb_color, scaled by brightness) instead of the fixed active_color.
  // LightControl tiles only; ignored for non-light entities.
  bool follow_color = false;
  // Header only: show today's high/low beside the weather temperature.
  bool show_hilo = false;
  // Any tile: drop the background (transparent) — for section-header tiles.
  bool transparent = false;
  // Blank tile: shrink the inset so the icon/text sit close to the edges.
  bool tight_margins = false;
  // Header only: show the weather cluster / the clock+date. Default true; turn
  // off to use the Header as a plain text section header. (On the wire these are
  // stored inverted — a set bit means "hidden" — so old decks default to shown.)
  bool show_weather = true;
  bool show_clock = true;
  // Blank tile text/icon alignment (DECK v4): halign|(valign<<2), each 0/1/2
  // (left/centre/right, top/centre/bottom). Default 5 = centre/centre.
  int align = 5;
  // Grid position (DECK v2). Kept at the end so default_deck()'s positional
  // aggregate initializers stay valid; v1 decks get these migrated on decode.
  // Assigned by name in decode_deck, so struct order is independent of the wire.
  int col = 0, row = 0;
  int page = 0;  // which page this card lives on (0 = Home)
};

// Deck-level accent colour (0xRRGGBB) or -1 for the built-in card colours. Set
// once at boot from the decoded deck; read by tiles/modals as their default
// on-tint (per-tile Active/Inactive colours still override via resolve_color).
// The accent the deck itself specifies, captured when the deck loads.
//
// Kept separate from deck_accent() — which light.accent overwrites live — so
// turning that light off can restore the deck's own colour. It must NOT be
// captured lazily on first use: light.accent is restore_mode ALWAYS_OFF, and
// ESPHome fires a light's on_state from LightState::setup(), which runs well
// before the on_boot block (priority -100) that loads the deck. A lazy capture
// therefore latches -1 on every boot and permanently breaks "off = the deck's
// accent" for exactly the decks that set one.
inline int32_t &deck_default_accent() {
  static int32_t accent = -1;
  return accent;
}

inline int32_t &deck_accent() {
  static int32_t accent = -1;
  return accent;
}
inline uint32_t accent_or(uint32_t builtin) {
  const int32_t a = deck_accent();
  return a >= 0 ? static_cast<uint32_t>(a) : builtin;
}

// The generic "this tile is on" colour, and the built-in it falls back to when
// the deck sets no accent.
//
// Use this for any tile whose active state carries no domain-specific meaning —
// toggles, scenes, buttons, presence, lights, page tiles. Writing the literal
// instead is how Toggle, Scene and Presence drifted out of sync with the accent
// while Light and Page followed it.
//
// Tiles whose active colour *means* something keep their own: a lock's green
// locked / red unlocked, an alarm's red armed, a fan's blue running. Recolouring
// those to the accent would throw away the signal.
inline constexpr uint32_t kTileOnBuiltin = 0xFF8C00;
inline uint32_t tile_on_color() { return accent_or(kTileOnBuiltin); }

}  // namespace tilehaus
