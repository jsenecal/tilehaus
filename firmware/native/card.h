#pragma once
#include "lvgl.h"
#include "card_config.h"
#include <memory>
#include <vector>
#include "grid_layout.h"   // TileSpan

namespace tilehaus {

// Fonts resolved from ESPHome `font:` ids at boot and passed into cards, so the
// native headers stay pure-LVGL (no esphome font types).
struct CardFonts {
  const lv_font_t *icon = nullptr;    // font_icon_main
  const lv_font_t *value = nullptr;   // font_number_value
  const lv_font_t *body = nullptr;    // font_text_body
  const lv_font_t *medium = nullptr;  // font_number_medium (compact values)
  const lv_font_t *small = nullptr;   // font_text_small (secondary lines)
  // "tight" matches the vocabulary the tile-sizing policy uses (kTileTightWidth,
  // TextScale::Tight); "small" was already taken by the unrelated 15px
  // secondary-text font above.
  const lv_font_t *body_tight = nullptr;  // font_text_body_tight (narrow tiles)
  const lv_font_t *clock_xl = nullptr;    // font_clock_xl (screensaver clock)
  const lv_font_t *title = nullptr;       // font_title (page titles)
  const lv_font_t *caption = nullptr;     // font_caption (mono captions)
  const lv_font_t *clock_mono = nullptr;  // font_clock_mono (header clock)
};

struct Card {
  virtual TileSpan default_size() const = 0;
  // Cards that paint their own background (or want none) return false so the
  // host skips style_cell(). Default true keeps the standard tile chrome.
  virtual bool wants_chrome() const { return true; }
  virtual void build(lv_obj_t *cell, const CardConfig &cfg,
                     const CardFonts &fonts) = 0;
  virtual void bind(const CardConfig &cfg) = 0;
  // Re-resolve colours against the current deck accent, without rebuilding.
  // Rebuilding is not an option: HA state subscriptions cannot be torn down at
  // runtime, so a rebuild would duplicate every subscription. Re-theming an
  // existing widget has no such problem.
  virtual void restyle() {}
  // A card can be destroyed while the panel runs (rebuilt for a cosmetic deck
  // change, card_host.h), so a timer whose user data is the card, or anything
  // in it, must go with it: create it through own_timer().
  virtual ~Card() {
    for (lv_timer_t *t : timers_) lv_timer_delete(t);
  }

 protected:
  lv_timer_t *own_timer(lv_timer_t *t) {
    timers_.push_back(t);
    return t;
  }

 private:
  std::vector<lv_timer_t *> timers_;
};

std::unique_ptr<Card> make_card(CardType type);

}  // namespace tilehaus
