#pragma once
#include "lvgl.h"
#include "card_config.h"
#include <memory>
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
  // Appended last: bindings.yaml aggregate-initialises this struct positionally,
  // so inserting above would silently reassign every field after it.
  const lv_font_t *body_small = nullptr;  // font_text_body_small (tight tiles)
};

struct Card {
  virtual TileSpan default_size() const = 0;
  // Cards that paint their own background (or want none) return false so the
  // host skips style_cell(). Default true keeps the standard tile chrome.
  virtual bool wants_chrome() const { return true; }
  virtual void build(lv_obj_t *cell, const CardConfig &cfg,
                     const CardFonts &fonts) = 0;
  virtual void bind(const CardConfig &cfg) = 0;
  virtual ~Card() = default;
};

std::unique_ptr<Card> make_card(CardType type);

}  // namespace tilehaus
