#pragma once
#include "lvgl.h"
#include <cstddef>
#include <cstdint>
#include "palette.h"  // kTileBg and friends

namespace tilehaus {

// Editorial light theme (experiment): an off-white page, square charcoal
// tiles, near-black heavy type on the page itself, and a lime accent. Tiles
// keep their white-on-dark content, so the palette only has to change where
// text sits directly on the page (header, section titles).
inline constexpr uint32_t kPageBg = 0xE9E6E0;     // warm off-white page
inline constexpr uint32_t kInk = 0x141414;        // text on the page
inline constexpr uint32_t kInkMuted = 0x5C5955;   // captions on the page
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

inline uint32_t style_rgb(lv_color_t c) { return lv_color_to_u32(c) & 0xFFFFFF; }

// Whether `part` has an opaque background; if so, its colour in *rgb.
inline bool opaque_bg(lv_obj_t *obj, lv_part_t part, uint32_t *rgb) {
  if (lv_obj_get_style_bg_opa(obj, part) < LV_OPA_50) return false;
  *rgb = style_rgb(lv_obj_get_style_bg_color(obj, part));
  return true;
}

// A solid, square rectangle drawn straight into a layer (draw callbacks).
inline void fill_rect(lv_layer_t *layer, const lv_area_t &r, uint32_t rgb) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_hex(rgb);
  d.bg_opa = LV_OPA_COVER;
  lv_draw_rect(layer, &d, &r);
}

// A solid ink bar object (the title bar under a page title).
inline lv_obj_t *add_ink_bar(lv_obj_t *parent, int w, int h) {
  lv_obj_t *bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, w, h);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(bar, lv_color_hex(kInk), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  return bar;
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
  uint32_t bg;
  if (opaque_bg(obj, LV_PART_MAIN, &bg)) on_light = needs_dark_ink(bg);
  if (on_light && lv_obj_check_type(obj, &lv_label_class)) {
    const uint32_t col = style_rgb(lv_obj_get_style_text_color(obj, LV_PART_MAIN));
    if (col == kTileInk) {
      lv_obj_set_style_text_color(obj, lv_color_hex(kInk), 0);
    } else if (col == kBlack) {
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

// Map `cur` into the current mode and, if it changed, hand the result to
// `set`. Returns whether `cur` was one of `pairs` at all.
template <size_t N, typename Set>
inline bool remap(uint32_t cur, const ThemePair (&pairs)[N], Set set) {
  bool hit = false;
  const uint32_t want = theme_color(cur, pairs, &hit);
  if (hit && want != cur) set(lv_color_hex(want));
  return hit;
}

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
    remap(style_rgb(lv_obj_get_style_outline_color(obj, LV_PART_MAIN)), kBgPairs,
          [obj](lv_color_t c) { lv_obj_set_style_outline_color(obj, c, 0); });
  }
  uint32_t bg;
  if (opaque_bg(obj, LV_PART_MAIN, &bg)) {
    on_page = remap(bg, kBgPairs,
                    [obj](lv_color_t c) { lv_obj_set_style_bg_color(obj, c, 0); });
  }
  if (on_page && lv_obj_check_type(obj, &lv_label_class)) {
    remap(style_rgb(lv_obj_get_style_text_color(obj, LV_PART_MAIN)), kTextPairs,
          [obj](lv_color_t c) { lv_obj_set_style_text_color(obj, c, 0); });
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
  add_ink_bar(col, kTitleBarW, kTitleBarH);
  return col;
}

}  // namespace tilehaus
