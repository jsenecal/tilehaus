#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "card_config.h"

namespace tilehaus {

// A deck as decoded: what deck_live_change compares.
struct DeckParts {
  std::vector<CardConfig> cards;
  int32_t accent = -1;
  std::vector<std::string> pages;
  std::vector<bool> page_back;
  int grid_cols = 0, grid_rows = 0;
};

// What a saved deck changes, and whether the panel can apply it without a
// reboot. A change is live when it only touches how tiles look: their title,
// icon, colours, label, alignment and background, and the deck's accent.
// Anything else (a tile added, removed, moved, resized, retyped or pointed at
// another entity, a page or grid change, a flag that adds a control) means a
// reboot, since Home Assistant subscriptions are fixed once HA has connected.
// Pure, so it is host-tested.
struct LiveChange {
  bool live = false;
  std::vector<size_t> cards;  // indices of the tiles to rebuild
  bool accent = false;        // the deck's accent changed
};

// Same tile, same subscriptions and controls: only its looks may differ.
inline bool same_tile_shape(const CardConfig &a, const CardConfig &b) {
  return a.type == b.type && a.entity == b.entity && a.entity2 == b.entity2 &&
         a.w == b.w && a.h == b.h && a.col == b.col && a.row == b.row &&
         a.page == b.page && a.detail == b.detail &&
         a.follow_color == b.follow_color && a.show_hilo == b.show_hilo &&
         a.show_weather == b.show_weather && a.show_clock == b.show_clock;
}

inline bool same_tile_looks(const CardConfig &a, const CardConfig &b) {
  return a.title == b.title && a.icon == b.icon && a.icon_alt == b.icon_alt &&
         a.active_color == b.active_color && a.inactive_color == b.inactive_color &&
         a.hide_label == b.hide_label && a.transparent == b.transparent &&
         a.tight_margins == b.tight_margins && a.align == b.align;
}

inline LiveChange deck_live_change(const DeckParts &from, const DeckParts &to) {
  LiveChange c;
  if (from.cards.size() != to.cards.size() || from.pages != to.pages ||
      from.page_back != to.page_back || from.grid_cols != to.grid_cols ||
      from.grid_rows != to.grid_rows) {
    return c;
  }
  for (size_t i = 0; i < to.cards.size(); ++i) {
    if (!same_tile_shape(from.cards[i], to.cards[i])) return c;
    if (!same_tile_looks(from.cards[i], to.cards[i])) c.cards.push_back(i);
  }
  c.accent = from.accent != to.accent;
  c.live = true;
  return c;
}

}  // namespace tilehaus
