#pragma once

namespace tilehaus {

// How much horizontal room a tile has before its default text stops fitting.
// On the office panel a 2-column tile is 156px and a 4-column one is 322px, so
// 200 separates them with room to spare either side. Below this, a tile takes
// the smaller body font and drops its sensor value a rung.
//
// Derived from glyph advance-width arithmetic against a 124px content area
// (156px less two 16px insets), not from measuring rendered text — expect to
// tune it against a real panel.
inline constexpr int kTileTightWidth = 200;

// Minimum content height (tile height less both insets) for the header to stack
// its clock over its date. The stacked pair needs ~90px; a one-row slot offers
// 40px, which clipped the date.
inline constexpr int kHeaderStackMinContentHeight = 96;

enum class TextScale { Default, Tight };

// Deliberately returns a class, not an lv_font_t* — keeping LVGL out of this
// header is what makes the policy host-testable. card_host.h maps it to fonts.
inline TextScale text_scale_for(int tile_px_w) {
  return tile_px_w < kTileTightWidth ? TextScale::Tight : TextScale::Default;
}

inline bool header_clock_stacks(int content_px_h) {
  return content_px_h >= kHeaderStackMinContentHeight;
}

}  // namespace tilehaus
