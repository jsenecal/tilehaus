#pragma once
#include "lvgl.h"
#include <cstdint>

namespace tilehaus {

// Editorial light theme (experiment): an off-white page, square charcoal
// tiles, near-black heavy type on the page itself, and a lime accent. Tiles
// keep their white-on-dark content, so the palette only has to change where
// text sits directly on the page (header, section titles).
inline constexpr uint32_t kPageBg = 0xE9E6E0;     // warm off-white page
inline constexpr uint32_t kInk = 0x141414;        // text on the page
inline constexpr uint32_t kInkMuted = 0x5C5955;   // captions on the page
inline constexpr uint32_t kTileBg = 0x313131;     // tile chrome (cards still repaint with the literal)
inline constexpr uint32_t kTitleBarW = 64;        // the bar under a page title
inline constexpr uint32_t kTitleBarH = 8;

// Perceived brightness of 0xRRGGBB, 0..255 (Rec. 601 weights).
inline int color_luma(uint32_t rgb) {
  const int r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
  return (r * 299 + g * 587 + b * 114) / 1000;
}

// A light tile fill (lime, a pale follow colour) needs dark text.
inline bool needs_dark_ink(uint32_t bg) { return color_luma(bg) > 150; }

// Re-ink a tile's labels for its current fill: white text turns near-black on
// a light fill (and back), and the black legibility twins add_icon/add_name
// draw behind white text are hidden while the text is dark. Other colours
// (muted greys, the chevron) are left alone.
inline void ink_labels_for(lv_obj_t *obj, uint32_t bg) {
  const bool dark = needs_dark_ink(bg);
  const uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < n; ++i) {
    lv_obj_t *c = lv_obj_get_child(obj, i);
    if (lv_obj_check_type(c, &lv_label_class)) {
      const uint32_t col =
          lv_color_to_u32(lv_obj_get_style_text_color(c, LV_PART_MAIN)) &
          0xFFFFFF;
      if (col == 0xFFFFFF || col == kInk) {
        lv_obj_set_style_text_color(c, lv_color_hex(dark ? kInk : 0xFFFFFF), 0);
      } else if (col == 0x000000) {
        lv_obj_set_style_text_opa(c, dark ? LV_OPA_TRANSP : LV_OPA_60, 0);
      }
    }
    ink_labels_for(c, bg);
  }
}

// Corner brackets: a light tile (lime when on, a pale follow colour) gets dark
// L-shaped marks in its four corners, where rounded corners used to be. Drawn
// in the post pass so they sit above the tile's content, and decided per frame
// from the tile's current fill, so they come and go with state (and ride the
// colour transition) without any card having to know about them.
inline constexpr int kCornerArm = 6;     // length of each bracket arm
inline constexpr int kCornerThick = 2;   // stroke weight

inline void corner_marks_draw_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target_obj(e);
  if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) < LV_OPA_50) return;
  const uint32_t bg =
      lv_color_to_u32(lv_obj_get_style_bg_color(obj, LV_PART_MAIN)) & 0xFFFFFF;
  if (!needs_dark_ink(bg)) return;

  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_hex(kInk);
  d.bg_opa = LV_OPA_COVER;
  d.radius = 0;
  auto rect = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    lv_area_t r = {x1, y1, x2, y2};
    lv_draw_rect(layer, &d, &r);
  };
  const int32_t L = kCornerArm - 1, T = kCornerThick - 1;
  rect(a.x1, a.y1, a.x1 + L, a.y1 + T);  // ┌
  rect(a.x1, a.y1, a.x1 + T, a.y1 + L);
  rect(a.x2 - L, a.y1, a.x2, a.y1 + T);  // ┐
  rect(a.x2 - T, a.y1, a.x2, a.y1 + L);
  rect(a.x1, a.y2 - T, a.x1 + L, a.y2);  // └
  rect(a.x1, a.y2 - L, a.x1 + T, a.y2);
  rect(a.x2 - L, a.y2 - T, a.x2, a.y2);  // ┘
  rect(a.x2 - T, a.y2 - L, a.x2, a.y2);
}

inline void add_corner_marks(lv_obj_t *obj) {
  lv_obj_add_event_cb(obj, corner_marks_draw_cb, LV_EVENT_DRAW_POST, nullptr);
}

// Re-ink text that sits on the light page rather than inside a dark control:
// white becomes ink, light greys become the muted caption grey, and the black
// legibility twins disappear. Walks down from `obj`, tracking whether the
// nearest opaque ancestor is light, so labels inside dark buttons and panels
// are left white. Built for modal bodies that were designed on near-black.
inline void ink_page_text(lv_obj_t *obj, bool on_light) {
  if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) >= LV_OPA_50) {
    const uint32_t bg =
        lv_color_to_u32(lv_obj_get_style_bg_color(obj, LV_PART_MAIN)) & 0xFFFFFF;
    on_light = needs_dark_ink(bg);
  }
  if (on_light && lv_obj_check_type(obj, &lv_label_class)) {
    const uint32_t col =
        lv_color_to_u32(lv_obj_get_style_text_color(obj, LV_PART_MAIN)) & 0xFFFFFF;
    if (col == 0xFFFFFF) {
      lv_obj_set_style_text_color(obj, lv_color_hex(kInk), 0);
    } else if (col == 0x000000) {
      lv_obj_set_style_text_opa(obj, LV_OPA_TRANSP, 0);
    } else if (needs_dark_ink(col)) {
      lv_obj_set_style_text_color(obj, lv_color_hex(kInkMuted), 0);
    }
  }
  const uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < n; ++i) ink_page_text(lv_obj_get_child(obj, i), on_light);
}

// Uppercase ASCII in place — captions are set in spaced capitals.
inline void ascii_upper(char *s) {
  for (; *s; ++s)
    if (*s >= 'a' && *s <= 'z') *s = static_cast<char>(*s - 'a' + 'A');
}

// Page title: heavy near-black type with a short thick bar beneath it.
// Returns the content-sized column holding both, for the caller to align.
inline lv_obj_t *add_page_title(lv_obj_t *parent, const lv_font_t *font,
                                const char *text) {
  lv_obj_t *col = lv_obj_create(parent);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, 4, 0);
  // Decoration only: never swallow a tap meant for what sits around a title.
  lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *lbl = lv_label_create(col);
  lv_obj_set_style_text_color(lbl, lv_color_hex(kInk), 0);
  if (font) lv_obj_set_style_text_font(lbl, font, 0);
  lv_obj_set_style_text_letter_space(lbl, -1, 0);
  lv_label_set_text(lbl, text);
  lv_obj_t *bar = lv_obj_create(col);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, kTitleBarW, kTitleBarH);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(bar, lv_color_hex(kInk), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  return col;
}

}  // namespace tilehaus
