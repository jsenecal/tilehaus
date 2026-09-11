#pragma once
#include "lvgl.h"
#include "card.h"
#include "card_style.h"

namespace tilehaus {

// A plain tile: an optional inline icon + title, positioned per cfg.align. With
// the transparent flag it becomes a section header.
struct BlankCard : Card {
  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    // Reduced margins: shrink the inset style_cell applied, so the content can
    // sit close to the tile edges (handy for section headers).
    if (cfg.tight_margins) lv_obj_set_style_pad_all(cell, 2, 0);

    // Icon and title sit inline in a content-sized row; the row is then aligned
    // within the tile per cfg.align.
    lv_obj_t *box = lv_obj_create(cell);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(box, 8, 0);

    if (!cfg.icon.empty()) {
      lv_obj_t *ic = lv_label_create(box);
      lv_obj_set_style_text_color(ic, lv_color_hex(0xFFFFFF), 0);
      if (fonts.icon) lv_obj_set_style_text_font(ic, fonts.icon, 0);
      lv_label_set_text(ic, cfg.icon.c_str());
    }
    if (!cfg.title.empty()) {
      lv_obj_t *lbl = lv_label_create(box);
      lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
      if (fonts.body) lv_obj_set_style_text_font(lbl, fonts.body, 0);
      lv_label_set_text(lbl, cfg.title.c_str());
    }

    // align byte: halign in low 2 bits, valign in bits 2-3 (0/1/2 each).
    static const lv_align_t kAlign[3][3] = {
        {LV_ALIGN_TOP_LEFT, LV_ALIGN_TOP_MID, LV_ALIGN_TOP_RIGHT},
        {LV_ALIGN_LEFT_MID, LV_ALIGN_CENTER, LV_ALIGN_RIGHT_MID},
        {LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_MID, LV_ALIGN_BOTTOM_RIGHT},
    };
    int h = cfg.align & 0x3;
    int v = (cfg.align >> 2) & 0x3;
    if (h > 2) h = 1;
    if (v > 2) v = 1;
    lv_obj_align(box, kAlign[v][h], 0, 0);
  }

  void bind(const CardConfig &) override {}
};

}  // namespace tilehaus
