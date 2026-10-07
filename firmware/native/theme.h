#pragma once
#include "lvgl.h"
#include <cstddef>
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
// Night mode: the page goes near-black and its text near-white. Tiles keep
// their charcoal, so night reads as the original dark look. The night ink is a
// warm off-white rather than pure white so the swap can tell page text (which
// flips) from tile text (always 0xFFFFFF, never flips) going back to day.
inline constexpr uint32_t kNightBg = 0x0E0E0E;
inline constexpr uint32_t kNightInk = 0xF2F0EC;
inline constexpr uint32_t kNightInkMuted = 0x8E8B86;
inline constexpr uint32_t kTitleBarW = 64;        // the bar under a page title
inline constexpr uint32_t kTitleBarH = 8;

// Perceived brightness of 0xRRGGBB, 0..255 (Rec. 601 weights).
inline int color_luma(uint32_t rgb) {
  const int r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
  return (r * 299 + g * 587 + b * 114) / 1000;
}

// A light tile fill (lime, a pale follow colour) needs dark text.
inline bool needs_dark_ink(uint32_t bg) { return color_luma(bg) > 150; }

// Corner brackets: a light tile (lime when on, a pale follow colour) gets dark
// L-shaped marks in its four corners, where rounded corners used to be. Drawn
// in the post pass so they sit above the tile's content, and decided per frame
// from the tile's current fill, so they come and go with state (and ride the
// colour transition) without any card having to know about them.
inline constexpr int kCornerArm = 6;     // length of each bracket arm
inline constexpr int kCornerThick = 2;   // stroke weight

inline bool &night_mode();

// Bracket corners, as bits: which of a box's corners get a mark.
enum : uint8_t { kCornerTL = 1, kCornerTR = 2, kCornerBL = 4, kCornerBR = 8,
                 kCornersTop = 3, kCornersBottom = 12, kCornersAll = 15 };

inline void draw_corner_marks(lv_layer_t *layer, const lv_area_t &a,
                              uint8_t corners) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_hex(kInk);
  d.bg_opa = LV_OPA_COVER;
  auto rect = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    lv_area_t r = {x1, y1, x2, y2};
    lv_draw_rect(layer, &d, &r);
  };
  const int32_t L = kCornerArm - 1, T = kCornerThick - 1;
  if (corners & kCornerTL) {  // ┌
    rect(a.x1, a.y1, a.x1 + L, a.y1 + T);
    rect(a.x1, a.y1, a.x1 + T, a.y1 + L);
  }
  if (corners & kCornerTR) {  // ┐
    rect(a.x2 - L, a.y1, a.x2, a.y1 + T);
    rect(a.x2 - T, a.y1, a.x2, a.y1 + L);
  }
  if (corners & kCornerBL) {  // └
    rect(a.x1, a.y2 - T, a.x1 + L, a.y2);
    rect(a.x1, a.y2 - L, a.x1 + T, a.y2);
  }
  if (corners & kCornerBR) {  // ┘
    rect(a.x2 - L, a.y2 - T, a.x2, a.y2);
    rect(a.x2 - T, a.y2 - L, a.x2, a.y2);
  }
}

// The fill of `part` right now, if it is opaque and light enough to mark.
inline bool part_is_light(lv_obj_t *obj, lv_part_t part) {
  if (lv_obj_get_style_bg_opa(obj, part) < LV_OPA_50) return false;
  if (lv_obj_get_style_opa(obj, part) < LV_OPA_50) return false;
  return needs_dark_ink(
      lv_color_to_u32(lv_obj_get_style_bg_color(obj, part)) & 0xFFFFFF);
}

// Tiles, buttons, option tiles: marks while the object's own fill is light.
inline void corner_marks_draw_cb(lv_event_t *e) {
  if (night_mode()) return;  // a day-page detail; night keeps plain tiles
  lv_obj_t *obj = lv_event_get_target_obj(e);
  if (!part_is_light(obj, LV_PART_MAIN)) return;
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  draw_corner_marks(lv_event_get_layer(e), a, kCornersAll);
}

// Idempotent: a tile can reach this through more than one shared style (an
// action tile is both a cell and a flashing button).
inline void add_corner_marks(lv_obj_t *obj) {
  lv_obj_remove_event_cb(obj, corner_marks_draw_cb);
  lv_obj_add_event_cb(obj, corner_marks_draw_cb, LV_EVENT_DRAW_POST, nullptr);
}

// A tile whose light part is a child panel (the cover tile's shade, hanging
// from the top): mark the tile's corners that the panel covers, while light.
inline void fill_marks_draw_cb(lv_event_t *e) {
  if (night_mode()) return;
  lv_obj_t *cell = lv_event_get_target_obj(e);
  auto *fill = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
  if (!fill || lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN)) return;
  if (!part_is_light(fill, LV_PART_MAIN)) return;
  lv_area_t a, f;
  lv_obj_get_coords(cell, &a);
  lv_obj_get_coords(fill, &f);
  auto covers = [&](int32_t x, int32_t y) {
    return x >= f.x1 && x <= f.x2 && y >= f.y1 && y <= f.y2;
  };
  uint8_t corners = 0;
  if (covers(a.x1, a.y1)) corners |= kCornerTL;
  if (covers(a.x2, a.y1)) corners |= kCornerTR;
  if (covers(a.x1, a.y2)) corners |= kCornerBL;
  if (covers(a.x2, a.y2)) corners |= kCornerBR;
  if (corners) draw_corner_marks(lv_event_get_layer(e), a, corners);
}

inline void add_fill_corner_marks(lv_obj_t *cell, lv_obj_t *fill) {
  lv_obj_add_event_cb(cell, fill_marks_draw_cb, LV_EVENT_DRAW_POST, fill);
}

// Fill sliders: the light part is the fill (indicator), which only reaches
// some corners. A vertical fill grows from the bottom: the bottom corners sit
// on the fill once there is any, the top corners only when it is full; a
// horizontal one grows from the left. Corners the fill doesn't reach are
// judged on the track instead.
inline void slider_marks_draw_cb(lv_event_t *e) {
  if (night_mode()) return;
  lv_obj_t *s = lv_event_get_target_obj(e);
  const int32_t v = lv_slider_get_value(s);
  const bool any = v > lv_slider_get_min_value(s);
  const bool full = v >= lv_slider_get_max_value(s);
  const bool fill_light = part_is_light(s, LV_PART_INDICATOR);
  const bool track_light = part_is_light(s, LV_PART_MAIN);
  lv_area_t a;
  lv_obj_get_coords(s, &a);
  const bool vertical = lv_area_get_height(&a) > lv_area_get_width(&a);
  const uint8_t near = vertical ? kCornersBottom : (kCornerTL | kCornerBL);
  const uint8_t far = kCornersAll & ~near;
  uint8_t corners = 0;
  if (any ? fill_light : track_light) corners |= near;
  if (full ? fill_light : track_light) corners |= far;
  if (corners) draw_corner_marks(lv_event_get_layer(e), a, corners);
}

inline void add_slider_corner_marks(lv_obj_t *slider) {
  lv_obj_add_event_cb(slider, slider_marks_draw_cb, LV_EVENT_DRAW_POST, nullptr);
}

// No rounded corners anywhere. LVGL's default theme rounds what it styles —
// slider tracks and knobs, arc ends, dropdown lists, buttons, plain objects —
// so this theme sits on top of it and squares every part of every widget as
// it is created. Installed once at boot, before any UI is built.
inline void square_theme_apply_cb(lv_theme_t *, lv_obj_t *obj) {
  static lv_style_t sq;
  static bool inited = false;
  if (!inited) {
    lv_style_init(&sq);
    lv_style_set_radius(&sq, 0);
    lv_style_set_arc_rounded(&sq, false);
    inited = true;
  }
  static const lv_part_t kParts[] = {LV_PART_MAIN, LV_PART_INDICATOR,
                                     LV_PART_KNOB, LV_PART_ITEMS,
                                     LV_PART_SELECTED, LV_PART_SCROLLBAR};
  for (lv_part_t p : kParts) lv_obj_add_style(obj, &sq, p);
}

inline void install_square_theme() {
  lv_display_t *disp = lv_display_get_default();
  if (!disp) return;
  lv_theme_t *base = lv_display_get_theme(disp);
  lv_theme_t *th = lv_theme_create();
  if (!th) return;
  if (base) lv_theme_set_parent(th, base);
  lv_theme_set_apply_cb(th, square_theme_apply_cb);
  lv_display_set_theme(disp, th);
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

inline bool &night_mode() {
  static bool night = false;
  return night;
}

// Day <-> night colour pairs for things on the page. Backgrounds: the page and
// the ink-coloured title bar / back button. Text: ink, muted ink, and the page
// colour itself (the back arrow is cut out of its button in the page colour).
struct ThemePair { uint32_t day, night; };
inline constexpr ThemePair kBgPairs[] = {{kPageBg, kNightBg}, {kInk, kNightInk}};
inline constexpr ThemePair kTextPairs[] = {
    {kInk, kNightInk}, {kInkMuted, kNightInkMuted}, {kPageBg, kNightBg}};

// The colour `c` should be in the current mode, if it is one of `pairs` (in
// either mode); sets *hit accordingly.
template <size_t N>
inline uint32_t theme_color(uint32_t c, const ThemePair (&pairs)[N], bool *hit) {
  for (const ThemePair &p : pairs) {
    if (c == p.day || c == p.night) {
      *hit = true;
      return night_mode() ? p.night : p.day;
    }
  }
  *hit = false;
  return c;
}

inline uint32_t style_rgb(lv_color_t c) { return lv_color_to_u32(c) & 0xFFFFFF; }

// Bring everything that sits on the page into the current mode. `on_page`
// tracks whether the nearest opaque ancestor is the page (or a page-coloured
// piece like the title bar): an opaque tile, lime state tile or modal row cuts
// its subtree off, so tile content never flips. Idempotent — it maps colours
// from either palette to the current one — so it is safe to run on every
// toggle, after the deck builds, and each time a modal opens.
inline void retheme(lv_obj_t *obj, bool on_page = false) {
  // An outline is drawn outside the object, on whatever surrounds it — so it
  // follows the page when its parent sits on the page, whatever the object's
  // own fill (the brightness slider's ink outline around a charcoal track).
  if (on_page && lv_obj_get_style_outline_width(obj, LV_PART_MAIN) > 0) {
    bool hit = false;
    const uint32_t col = style_rgb(lv_obj_get_style_outline_color(obj, LV_PART_MAIN));
    const uint32_t want = theme_color(col, kBgPairs, &hit);
    if (hit && want != col) lv_obj_set_style_outline_color(obj, lv_color_hex(want), 0);
  }
  if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) >= LV_OPA_50) {
    bool hit = false;
    const uint32_t bg = style_rgb(lv_obj_get_style_bg_color(obj, LV_PART_MAIN));
    const uint32_t want = theme_color(bg, kBgPairs, &hit);
    if (hit && want != bg) lv_obj_set_style_bg_color(obj, lv_color_hex(want), 0);
    on_page = hit;
  }
  if (on_page && lv_obj_check_type(obj, &lv_label_class)) {
    bool hit = false;
    const uint32_t col = style_rgb(lv_obj_get_style_text_color(obj, LV_PART_MAIN));
    const uint32_t want = theme_color(col, kTextPairs, &hit);
    if (hit && want != col) lv_obj_set_style_text_color(obj, lv_color_hex(want), 0);
  }
  const uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < n; ++i) retheme(lv_obj_get_child(obj, i), on_page);
}

// Re-theme the whole display (the grid's screen and the modals' top layer).
// A no-op before LVGL is up — the switch restores its state during setup,
// before the deck is built, and the build applies the mode itself.
inline void apply_theme() {
  if (!lv_is_initialized() || !lv_display_get_default()) return;
  if (lv_obj_t *scr = lv_screen_active()) {
    // The screen is the page by definition; set it outright rather than rely
    // on how ESPHome styled its opacity.
    lv_obj_set_style_bg_color(scr, lv_color_hex(night_mode() ? kNightBg : kPageBg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    retheme(scr, true);
  }
  retheme(lv_layer_top());
  // Corner brackets are drawn per frame from night_mode(), not stored in any
  // style, so recolouring alone would not repaint them.
  lv_obj_invalidate(lv_layer_top());
  if (lv_obj_t *scr = lv_screen_active()) lv_obj_invalidate(scr);
}

inline void set_night_mode(bool on) {
  night_mode() = on;
  apply_theme();
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
