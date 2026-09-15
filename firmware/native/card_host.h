#pragma once
#include "lvgl.h"
#include <vector>
#include <memory>
#include "card.h"
#include "card_style.h"
#include "card_factory.h"
#include "card_deck.h"
#include "awaiting_config.h"
#include "deck_document.h"
#include "deck_store.h"
#include "grid.h"
#include "surface.h"
#include "splash.h"
#include "page_nav.h"
#include "tile_scale.h"

namespace tilehaus {

inline std::vector<std::unique_ptr<Card>> &live_cards() {
  static std::vector<std::unique_ptr<Card>> v;
  return v;
}

// Builds `deck` into `container`, APPENDING to live_cards() (does not clear —
// callers building multiple pages in sequence need earlier pages' cards to
// stay alive) and binding only the cards it just created.
inline void build_page(lv_obj_t *container, const std::vector<CardConfig> &deck,
                       int unit_px, int pad_px, int gap_px, int subdivisions,
                       int base_cols, int base_rows, int radius,
                       const CardFonts &fonts,
                       int override_cols = 0, int override_rows = 0) {
  auto &cards = live_cards();
  const size_t start = cards.size();
  // Same numbers poc_build_grid will use below, computed up front so each card
  // can be built at the right text size — no layout pass, no reflow.
  lv_display_t *disp = lv_obj_get_display(container);
  const GridMetrics gm = compute_grid_metrics(
      lv_display_get_horizontal_resolution(disp),
      lv_display_get_vertical_resolution(disp), unit_px, pad_px, gap_px,
      subdivisions, base_cols, base_rows, override_cols, override_rows);
  std::vector<GridTile> tiles;
  std::vector<CardConfig> cfgs;
  tiles.reserve(deck.size());
  for (const auto &cfg : deck) {
    lv_obj_t *cell = lv_obj_create(container);
    std::unique_ptr<Card> card = make_card(cfg.type);
    if (card->wants_chrome()) {
      style_cell(cell, radius);
    } else {
      lv_obj_remove_style_all(cell);
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(cell, 0, 0);
      lv_obj_set_style_pad_all(cell, kTileInset, 0);
      lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    }
    // Opt-in transparent background (section-header tiles): drop the fill the
    // chrome path just set, keeping the inset so the content still aligns.
    if (cfg.transparent) lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    TileSpan sz = (cfg.w > 0 && cfg.h > 0) ? TileSpan{cfg.w, cfg.h}
                                           : card->default_size();
    tile_compact() = (sz.w <= 1 && sz.h <= 1);  // 1x1 → centred icon, no label
    tile_metrics() = {tile_pixel_width(gm, sz.w), tile_pixel_height(gm, sz.h)};
    // Pass the fonts through untouched: eleven cards persist this struct and
    // hand it to their modal or dialog, so scaling it here would shrink
    // full-screen overlays too. The tile-only choice happens inside add_name
    // and tile_value_font, which read tile_metrics() set just above.
    card->build(cell, cfg, fonts);
    tile_compact() = false;
    tile_metrics() = {0, 0};
    tiles.push_back(GridTile{cell, sz.w, sz.h, cfg.col, cfg.row});
    cfgs.push_back(cfg);
    cards.push_back(std::move(card));
  }
  poc_build_grid(container, tiles, unit_px, pad_px, gap_px, subdivisions,
                 base_cols, base_rows, override_cols, override_rows);
  for (size_t i = 0; i < cfgs.size(); ++i) cards[start + i]->bind(cfgs[i]);
}

// Builds `deck` into `root`. Assumes `root` has already been emptied.
inline void build_deck(lv_obj_t *root, const std::vector<CardConfig> &deck,
                       int unit_px, int pad_px, int gap_px, int subdivisions,
                       int base_cols, int base_rows, int radius,
                       const CardFonts &fonts) {
  live_cards().clear();
  build_page(root, deck, unit_px, pad_px, gap_px, subdivisions, base_cols,
             base_rows, radius, fonts);
}

// Boot entry: build the stored deck, or show the "Awaiting Configuration"
// screen when nothing valid is stored. New configs are applied by rebooting
// (see deck_endpoint::apply_pending_reboot), so the deck is built only here, at
// boot — there is no live in-place rebuild, which would require tearing down HA
// state subscriptions ESPHome cannot remove. `default_deck()` remains available
// in card_deck.h as a reference/example deck but is no longer the empty-store
// fallback (that is now the awaiting screen).
inline void poc_build_cards(lv_obj_t *root, int unit_px, int pad_px, int gap_px,
                            int subdivisions, int base_cols, int base_rows,
                            int radius, const CardFonts &fonts) {
  app_grid_root() = root;
  DeckStore &store = deck_store();
  std::vector<CardConfig> deck;
  int32_t accent = -1;
  std::vector<std::string> pages;
  int grid_cols = kDeckGridColsDefault, grid_rows = kDeckGridRowsDefault;
  if (store.empty() || !decode_deck(store.data(), store.length(), deck, &accent,
                                    &pages, &grid_cols, &grid_rows)) {
    awaiting_build(root, fonts);
    return;
  }
  deck_accent() = accent;  // drives the default on-tint before any card is built
  deck_default_accent() = accent;  // what light.accent reverts to when switched off
  lv_display_t *disp = lv_display_get_default();
  const int max_cols = grid_max_cells(lv_display_get_horizontal_resolution(disp));
  const int max_rows = grid_max_cells(lv_display_get_vertical_resolution(disp));
  if (grid_cols < 2) grid_cols = 2;
  if (grid_cols > max_cols) grid_cols = max_cols;
  if (grid_rows < 2) grid_rows = 2;
  if (grid_rows > max_rows) grid_rows = max_rows;
  // Each page container carries the grid inset (pad_px); the root must stay
  // flush or its own theme padding would double the margin.
  lv_obj_set_style_pad_all(root, 0, 0);
  auto &nav = page_nav();
  nav = PageNav{};
  nav.names = pages;
  live_cards().clear();
  for (size_t p = 0; p < pages.size(); ++p) {
    lv_obj_t *page = lv_obj_create(root);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_align(page, LV_ALIGN_TOP_LEFT, 0, 0);
    std::vector<CardConfig> subset;
    for (const auto &c : deck) if (c.page == static_cast<int>(p)) subset.push_back(c);
    build_page(page, subset, unit_px, pad_px, gap_px, subdivisions, base_cols,
               base_rows, radius, fonts, grid_cols, grid_rows);
    nav.containers.push_back(page);
  }
  // Back navigation is a placeable "Back" tile (CardType::Back), not an overlay;
  // nav.back_btn stays null (PageNav::show guards for it).
  nav.show(0);
}

}  // namespace tilehaus
