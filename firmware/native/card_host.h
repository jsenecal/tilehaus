#pragma once
#include "lvgl.h"
#include "esp_log.h"
#include <vector>
#include <memory>
#include "card.h"
#include "card_style.h"
#include "card_factory.h"
#include "card_deck.h"
#include "awaiting_config.h"
#include "deck_document.h"
#include "deck_live.h"
#include "deck_store.h"
#include "grid.h"
#include "ha.h"
#include "overlay.h"
#include "theme.h"
#include "surface.h"
#include "splash.h"
#include "page_nav.h"
#include "tile_scale.h"

namespace tilehaus {

inline std::vector<std::unique_ptr<Card>> &live_cards() {
  static std::vector<std::unique_ptr<Card>> v;
  return v;
}

// Where each live card sits, parallel to live_cards(): what a live rebuild
// (rebuild_tile) needs to build a replacement in the same place.
struct LiveTile {
  lv_obj_t *cell = nullptr;
  lv_obj_t *page = nullptr;
  int deck_index = -1;  // into the deck as stored; -1 for a deck built ad hoc
  CardConfig cfg;
  TileSpan size{1, 1};
  int px_w = 0, px_h = 0;
  bool took_back = false;  // this tile drew its page's Back button (BlankCard)
};
inline std::vector<LiveTile> &live_tiles() {
  static std::vector<LiveTile> v;
  return v;
}

// A fresh cell styled for `card`: the tile chrome, or none.
inline void prepare_cell(lv_obj_t *cell, const Card &card, const CardConfig &cfg) {
  if (card.wants_chrome()) {
    style_cell(cell);
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
}

// Build `card` into `cell` at its tile size. Pass the fonts through
// untouched: eleven cards persist this struct and hand it to their modal or
// dialog, so scaling it here would shrink full-screen overlays too. The
// tile-only choice happens inside add_name and tile_value_font, which read
// tile_metrics() set here. Listeners the card registers are tagged with it
// (ha_owner) so a live rebuild can drop them.
inline void build_card(Card &card, lv_obj_t *cell, const CardConfig &cfg,
                       const CardFonts &fonts, TileSpan sz, int px_w, int px_h) {
  tile_compact() = (sz.w <= 1 && sz.h <= 1);  // 1x1 → centred icon, no label
  tile_metrics() = {px_w, px_h};
  ha_owner() = &card;
  card.build(cell, cfg, fonts);
  ha_owner() = nullptr;
  tile_compact() = false;
  tile_metrics() = {0, 0};
}

inline void bind_card(Card &card, const CardConfig &cfg) {
  ha_owner() = &card;
  card.bind(cfg);
  ha_owner() = nullptr;
  ha_flush_replays();
}

// Builds `deck` into `container`, APPENDING to live_cards() (does not clear —
// callers building multiple pages in sequence need earlier pages' cards to
// stay alive) and binding only the cards it just created. `deck_index[i]` is
// where deck[i] sits in the stored deck, when known.
inline void build_page(lv_obj_t *container, const std::vector<CardConfig> &deck,
                       int unit_px, int pad_px, int gap_px, int subdivisions,
                       int base_cols, int base_rows, const CardFonts &fonts,
                       int override_cols = 0, int override_rows = 0,
                       const std::vector<int> *deck_index = nullptr) {
  auto &cards = live_cards();
  auto &where = live_tiles();
  const size_t start = cards.size();
  // Same numbers poc_build_grid will use below, computed up front so each card
  // can be built at the right text size — no layout pass, no reflow.
  lv_display_t *disp = lv_obj_get_display(container);
  const GridMetrics gm = compute_grid_metrics(
      lv_display_get_horizontal_resolution(disp),
      lv_display_get_vertical_resolution(disp), unit_px, pad_px, gap_px,
      subdivisions, base_cols, base_rows, override_cols, override_rows);
  std::vector<GridTile> tiles;
  tiles.reserve(deck.size());
  for (size_t i = 0; i < deck.size(); ++i) {
    const CardConfig &cfg = deck[i];
    lv_obj_t *cell = lv_obj_create(container);
    std::unique_ptr<Card> card = make_card(cfg.type);
    prepare_cell(cell, *card, cfg);
    TileSpan sz = (cfg.w > 0 && cfg.h > 0) ? TileSpan{cfg.w, cfg.h}
                                           : card->default_size();
    LiveTile t;
    t.cell = cell;
    t.page = container;
    t.deck_index = deck_index && i < deck_index->size() ? (*deck_index)[i] : -1;
    t.cfg = cfg;
    t.size = sz;
    t.px_w = tile_pixel_width(gm, sz.w);
    t.px_h = tile_pixel_height(gm, sz.h);
    const bool back_before = page_back_pending();
    build_card(*card, cell, cfg, fonts, sz, t.px_w, t.px_h);
    t.took_back = back_before && !page_back_pending();
    tiles.push_back(GridTile{cell, sz.w, sz.h, cfg.col, cfg.row});
    cards.push_back(std::move(card));
    where.push_back(std::move(t));
  }
  poc_build_grid(container, tiles, unit_px, pad_px, gap_px, subdivisions,
                 base_cols, base_rows, override_cols, override_rows);
  for (size_t i = start; i < cards.size(); ++i) bind_card(*cards[i], where[i].cfg);
}

// Builds `deck` into `root`. Assumes `root` has already been emptied.
inline void build_deck(lv_obj_t *root, const std::vector<CardConfig> &deck,
                       int unit_px, int pad_px, int gap_px, int subdivisions,
                       int base_cols, int base_rows, const CardFonts &fonts) {
  live_cards().clear();
  live_tiles().clear();
  build_page(root, deck, unit_px, pad_px, gap_px, subdivisions, base_cols,
             base_rows, fonts);
}

// What the boot build used, kept for a live rebuild (apply_live_deck).
struct BuildContext {
  bool built = false;  // a deck was built (not the awaiting screen)
  CardFonts fonts{};
  DeckParts deck;      // the deck the panel is showing
};
inline BuildContext &build_context() {
  static BuildContext c;
  return c;
}

// Boot entry: build the stored deck, or show the "Awaiting Configuration"
// screen when nothing valid is stored. A new deck that changes only how tiles
// look is applied in place (apply_live_deck); anything else by a reboot (see
// deck_endpoint::apply_pending_reboot), since HA subscriptions can't be added
// once HA has connected. `default_deck()` remains available in card_deck.h as
// a reference/example deck but is no longer the empty-store fallback (that is
// now the awaiting screen).
inline void poc_build_cards(lv_obj_t *root, int unit_px, int pad_px, int gap_px,
                            int subdivisions, int base_cols, int base_rows,
                            const CardFonts &fonts) {
  app_grid_root() = root;
  DeckStore &store = deck_store();
  std::vector<CardConfig> deck;
  int32_t accent = -1;
  std::vector<std::string> pages;
  std::vector<bool> page_back;
  int grid_cols = kDeckGridColsDefault, grid_rows = kDeckGridRowsDefault;
  if (store.empty() || !decode_deck(store.data(), store.length(), deck, &accent,
                                    &pages, &grid_cols, &grid_rows, nullptr, &page_back)) {
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
  live_tiles().clear();
  BuildContext &ctx = build_context();
  ctx.built = true;
  ctx.fonts = fonts;
  ctx.deck = DeckParts{deck, accent, pages, page_back, grid_cols, grid_rows};
  for (size_t p = 0; p < pages.size(); ++p) {
    lv_obj_t *page = lv_obj_create(root);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_align(page, LV_ALIGN_TOP_LEFT, 0, 0);
    std::vector<CardConfig> subset;
    std::vector<int> index;
    for (size_t i = 0; i < deck.size(); ++i) {
      if (deck[i].page != static_cast<int>(p)) continue;
      subset.push_back(deck[i]);
      index.push_back(static_cast<int>(i));
    }
    // A page set to show a Back button (never Home: nothing to go back to).
    page_back_pending() = p != 0 && p < page_back.size() && page_back[p];
    build_page(page, subset, unit_px, pad_px, gap_px, subdivisions, base_cols,
               base_rows, fonts, grid_cols, grid_rows, &index);
    if (page_back_pending()) {
      // No title in the corner took it: float it there, over the grid, fixed
      // while the page scrolls.
      page_back_pending() = false;
      lv_obj_t *b = add_back_square(page, fonts.icon, kPageBackSide, [](lv_event_t *) {
        page_nav().back_or_home();
      }, nullptr);
      lv_obj_add_flag(b, LV_OBJ_FLAG_FLOATING);
      lv_obj_align(b, LV_ALIGN_TOP_LEFT, pad_px, pad_px);
    }
    nav.containers.push_back(page);
  }
  // Back navigation is a placeable "Back" tile (CardType::Back), or a page's
  // own Back button (above); nav.back_btn stays null (PageNav::show guards for it).
  nav.show(0);
}

// Replace live card `i` with a new one built from `cfg`, in the same place.
// The old card goes completely: its listeners (ha_release), then its cell,
// then the card itself, whose destructor takes its modals and timers. The new
// card is built and bound as at boot; its listeners get the current values.
inline void rebuild_tile(size_t i, const CardConfig &cfg) {
  auto &cards = live_cards();
  auto &where = live_tiles();
  if (i >= cards.size() || i >= where.size()) return;
  LiveTile &t = where[i];
  ha_release(cards[i].get());

  lv_obj_t *old = t.cell;
  lv_obj_t *cell = lv_obj_create(t.page);
  std::unique_ptr<Card> card = make_card(cfg.type);
  prepare_cell(cell, *card, cfg);
  lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH,
                       lv_obj_get_style_grid_cell_column_pos(old, LV_PART_MAIN),
                       lv_obj_get_style_grid_cell_column_span(old, LV_PART_MAIN),
                       LV_GRID_ALIGN_STRETCH,
                       lv_obj_get_style_grid_cell_row_pos(old, LV_PART_MAIN),
                       lv_obj_get_style_grid_cell_row_span(old, LV_PART_MAIN));
  lv_obj_move_to_index(cell, lv_obj_get_index(old));
  lv_obj_delete(old);
  cards[i].reset();

  page_back_pending() = t.took_back;
  build_card(*card, cell, cfg, build_context().fonts, t.size, t.px_w, t.px_h);
  page_back_pending() = false;
  t.cell = cell;
  t.cfg = cfg;
  cards[i] = std::move(card);
  bind_card(*cards[i], cfg);
}

// Apply a saved deck in place, when deck_live_change says it only changes
// looks. Returns false when it can't (nothing built yet, or the deck differs
// from what is showing), so the caller reboots instead.
inline bool apply_live_deck(const DeckParts &next) {
  BuildContext &ctx = build_context();
  if (!ctx.built) return false;
  const LiveChange change = deck_live_change(ctx.deck, next);
  if (!change.live) return false;
  // Close whatever is open first: a modal may belong to a card about to go.
  if (Overlay *o = active_overlay()) {
    if (o->on_close_) o->on_close_();
    else o->hide();
  }
  const uint32_t t0 = lv_tick_get();
  for (size_t d : change.cards) {
    for (size_t i = 0; i < live_tiles().size(); ++i) {
      if (live_tiles()[i].deck_index == static_cast<int>(d)) {
        rebuild_tile(i, next.cards[d]);
        break;
      }
    }
  }
  if (change.accent) {
    // light.accent may be overriding the deck's accent: then only its "off"
    // colour changes.
    const bool following_deck = deck_accent() == deck_default_accent();
    deck_default_accent() = next.accent;
    if (following_deck) deck_accent() = next.accent;
    for (auto &c : live_cards()) c->restyle();
  }
  ctx.deck = next;
  apply_theme();  // rebuilt tiles come out in day colours
  // A tile with a heavy modal (WLED's effect list) takes most of a second.
  ESP_LOGI("tilehaus.live", "applied live: %u tile(s) rebuilt in %u ms",
           static_cast<unsigned>(change.cards.size()), static_cast<unsigned>(lv_tick_elaps(t0)));
  return true;
}

}  // namespace tilehaus
