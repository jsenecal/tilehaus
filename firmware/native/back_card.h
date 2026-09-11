#pragma once
#include "lvgl.h"
#include <string>
#include "card.h"
#include "card_style.h"
#include "page_nav.h"

namespace tilehaus {

// A small navigation tile that pops the page back stack on tap. Placed on a
// subpage so you can return; fully configurable (icon/title/colour/size).
struct BackCard : Card {
  TileSpan default_size() const override { return {1, 1}; }

  void build(lv_obj_t *cell, const CardConfig &cfg, const CardFonts &fonts) override {
    const std::string glyph = cfg.icon.empty() ? std::string("\U000F0141") : cfg.icon;  // mdi-chevron-left
    // 1x1 centring + label-drop is handled centrally (tile_compact in build_page).
    add_icon(cell, fonts.icon, glyph);
    add_name(cell, fonts.body, cfg.title, cfg.hide_label);
    if (cfg.active_color >= 0)
      lv_obj_set_style_bg_color(cell, lv_color_hex(static_cast<uint32_t>(cfg.active_color)), 0);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    lv_obj_add_event_cb(cell, [](lv_event_t *) { page_nav().back(); }, LV_EVENT_CLICKED, nullptr);
  }

  void bind(const CardConfig &) override {}
};

}  // namespace tilehaus
