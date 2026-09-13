#pragma once
#include "lvgl.h"
#include <cstdint>
#include <string>
#include "card_config.h"  // accent_or
#include "card.h"        // CardFonts
#include "tile_scale.h"   // text_scale_for

namespace tilehaus {

// Resolve a per-tile configured color against a card's built-in default: a
// configured value >= 0 is a 24-bit 0xRRGGBB override; -1 (the CardConfig
// default) means "keep the card default".
inline uint32_t resolve_color(int32_t configured, uint32_t fallback) {
  return configured >= 0 ? static_cast<uint32_t>(configured) : fallback;
}

// Set true (by build_page) while a single-cell (1x1) tile is being built: its
// icon then centres and its name label is dropped, since there's no room for the
// icon-top-left + label-below layout. Judged by cell span, not pixels.
inline bool &tile_compact() {
  static bool compact = false;
  return compact;
}

// Pixel size of the tile currently being built, set by build_page around each
// card->build(). Same idiom as tile_compact() above: cards that need to adapt
// to their own size read it during build, and it is meaningless outside that
// window. Only HeaderCard uses it today.
struct TileMetrics { int px_w; int px_h; };
inline TileMetrics &tile_metrics() {
  static TileMetrics m{0, 0};
  return m;
}

// Inner content inset shared by every tile: the cell padding for normal cards,
// and the manual label offset for cards that drop the padding (fill slider).
// One place so all tiles line up.
inline constexpr int kTileInset = 16;

// Ease duration for value glides (fill sliders, cover shade). Applied to
// externally-pushed state changes (HA) and tap-to-position; a live drag stays
// 1:1 with the finger and is never animated.
inline constexpr uint32_t kSliderAnimMs = 100;

// Shared tile background: espcontrol's proven control look.
inline void style_cell(lv_obj_t *cell, int radius) {
  lv_obj_set_style_bg_color(cell, lv_color_hex(0x313131), 0);
  lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(cell, radius, 0);
  lv_obj_set_style_border_width(cell, 0, 0);
  lv_obj_set_style_pad_all(cell, kTileInset, 0);
  lv_obj_set_style_clip_corner(cell, true, 0);
  // Tiles never scroll internally: clip overflow to the rounded rect instead of
  // spawning a scrollbar when content sits a few px over the cell.
  lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
}

// The body/value font for the tile currently being built: the tight rung on a
// narrow tile, the default otherwise.
//
// This choice deliberately lives here rather than in the CardFonts handed to
// Card::build(). Eleven cards persist that struct (`fonts_ = fonts`) and later
// hand it to their modal, dialog or PIN pad — so scaling it there would follow
// them into full-screen overlays, which have none of a tile's width problem.
// add_name and the sensor value are called only from tile bodies, so choosing
// here keeps the scaling where it belongs by construction.
// build_page resets tile_metrics() to {0,0} between tiles, and text_scale_for(0)
// is Tight — so read the width defensively: a call from outside the build window
// must fail toward the FULL-SIZE font. Every modal header already includes this
// file, and add_name is the only labelled-text helper in it, so a future modal
// reaching for it is plausible; when that happens it should look like a wide
// tile, never silently shrink the way the bug this replaced did.
inline bool tile_is_tight() {
  const int px_w = tile_metrics().px_w;
  return px_w > 0 && text_scale_for(px_w) == TextScale::Tight;
}

inline const lv_font_t *tile_body_font(const CardFonts &fonts) {
  return (tile_is_tight() && fonts.body_tight) ? fonts.body_tight : fonts.body;
}
inline const lv_font_t *tile_value_font(const CardFonts &fonts) {
  return (tile_is_tight() && fonts.medium) ? fonts.medium : fonts.value;
}

// Icon label anchored top-left. Skipped when the glyph is empty.
// A soft dark drop-shadow twin behind an icon/name label, so the white glyphs
// stay legible on any tile background (especially follow-colour tiles that go
// light). The twin is a sibling created BEFORE the white label (so it renders
// behind), aligned identically but nudged +1px, kept at 60% so it reads as
// gentle depth rather than a hard outline. The white label links to its twin
// via user_data so set_icon_glyph keeps a swapped glyph in sync.
inline lv_obj_t *add_text_shadow(lv_obj_t *cell, const lv_font_t *font,
                                 const std::string &text, bool wrap,
                                 lv_align_t align, int dx, int dy) {
  lv_obj_t *t = lv_label_create(cell);
  lv_obj_set_style_text_color(t, lv_color_hex(0x000000), 0);
  lv_obj_set_style_text_opa(t, LV_OPA_60, 0);
  if (font) lv_obj_set_style_text_font(t, font, 0);
  if (wrap) {
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, LV_PCT(100));
  }
  lv_label_set_text(t, text.c_str());
  lv_obj_align(t, align, dx + 1, dy + 1);
  return t;
}

inline lv_obj_t *add_icon(lv_obj_t *cell, const lv_font_t *font,
                          const std::string &glyph,
                          lv_align_t align = LV_ALIGN_TOP_LEFT,
                          int dx = 0, int dy = 0) {
  if (glyph.empty()) return nullptr;
  // On a single-cell tile, centre the icon (only when the caller kept the
  // default top-left anchor — an explicit align is respected).
  if (tile_compact() && align == LV_ALIGN_TOP_LEFT) { align = LV_ALIGN_CENTER; dx = 0; dy = 0; }
  lv_obj_t *sh = add_text_shadow(cell, font, glyph, false, align, dx, dy);
  lv_obj_t *lbl = lv_label_create(cell);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
  if (font) lv_obj_set_style_text_font(lbl, font, 0);
  lv_label_set_text(lbl, glyph.c_str());
  lv_obj_align(lbl, align, dx, dy);
  lv_obj_set_user_data(lbl, sh);  // link shadow so set_icon_glyph can sync it
  return lbl;
}

// Swap an icon label's glyph, fading the new glyph in — but only when the glyph
// actually changes (so frequent observer ticks, e.g. a light's brightness, do
// not re-trigger the animation). A no-op when the label or glyph is empty.
inline void set_icon_glyph(lv_obj_t *icon, const std::string &glyph) {
  if (!icon || glyph.empty()) return;
  const char *cur = lv_label_get_text(icon);
  if (cur && glyph == cur) return;
  lv_label_set_text(icon, glyph.c_str());
  lv_obj_t *sh = static_cast<lv_obj_t *>(lv_obj_get_user_data(icon));
  if (sh) lv_label_set_text(sh, glyph.c_str());  // keep the shadow twin in sync
  lv_obj_fade_in(icon, 250, 0);
}

// Wrapping name label anchored bottom-left, full content width. Returns nullptr
// when `hidden` (per-tile hide_label) so callers can skip it — the icon stays.
// Takes the whole CardFonts, not one font, so it can pick the tight rung for a
// narrow tile — see tile_body_font.
inline lv_obj_t *add_name(lv_obj_t *cell, const CardFonts &fonts,
                          const std::string &title, bool hidden = false,
                          lv_align_t align = LV_ALIGN_BOTTOM_LEFT,
                          int dx = 0, int dy = 0) {
  if (hidden || tile_compact()) return nullptr;  // no room for a label on a 1x1
  const lv_font_t *font = tile_body_font(fonts);
  add_text_shadow(cell, font, title, true, align, dx, dy);
  lv_obj_t *lbl = lv_label_create(cell);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
  if (font) lv_obj_set_style_text_font(lbl, font, 0);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl, LV_PCT(100));
  lv_label_set_text(lbl, title.c_str());
  lv_obj_align(lbl, align, dx, dy);
  return lbl;
}

// Selection highlight + press feedback for a button: the background eases
// between off/on colors as LV_STATE_CHECKED is toggled (add/remove the state to
// drive it), and the button dips slightly while pressed. Both transitions share
// one static descriptor.
inline void style_button_feedback(lv_obj_t *btn, uint32_t off_color,
                                  uint32_t on_color) {
  static const lv_style_prop_t props[] = {
      LV_STYLE_BG_COLOR, LV_STYLE_OPA, LV_STYLE_TRANSFORM_SCALE_X,
      LV_STYLE_TRANSFORM_SCALE_Y, LV_STYLE_PROP_INV};
  static lv_style_transition_dsc_t tr;
  static bool inited = false;
  if (!inited) {
    lv_style_transition_dsc_init(&tr, props, lv_anim_path_ease_out, 160, 0,
                                 nullptr);
    inited = true;
  }
  lv_obj_set_style_transition(btn, &tr, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(off_color), LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(on_color),
                            LV_PART_MAIN | LV_STATE_CHECKED);
  lv_obj_set_style_transform_pivot_x(btn, lv_pct(50), LV_PART_MAIN);
  lv_obj_set_style_transform_pivot_y(btn, lv_pct(50), LV_PART_MAIN);
  lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_transform_scale_x(btn, 256, LV_PART_MAIN);  // 256 = 100%
  lv_obj_set_style_transform_scale_y(btn, 256, LV_PART_MAIN);
  lv_obj_set_style_opa(btn, 210, LV_PART_MAIN | LV_STATE_PRESSED);  // ~82%
  lv_obj_set_style_transform_scale_x(btn, 250, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_transform_scale_y(btn, 250, LV_PART_MAIN | LV_STATE_PRESSED);
}

// Soft press feedback: on press the element briefly dims (whole-object opacity,
// which blends toward the near-black page background so it reads as a gentle
// darken) plus a very slight scale-in, easing back on release. Colour-safe — it
// never touches bg_color, so it composes with tiles that recolour by state.
// One shared descriptor. Mirrors the soft feel of the modal dropdown buttons.
inline void style_press_dip(lv_obj_t *obj) {
  static const lv_style_prop_t props[] = {
      LV_STYLE_OPA, LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y,
      LV_STYLE_PROP_INV};
  static lv_style_transition_dsc_t tr;
  static bool inited = false;
  if (!inited) {
    lv_style_transition_dsc_init(&tr, props, lv_anim_path_ease_out, 150, 0,
                                 nullptr);
    inited = true;
  }
  lv_obj_set_style_transition(obj, &tr, LV_PART_MAIN);
  lv_obj_set_style_transform_pivot_x(obj, lv_pct(50), LV_PART_MAIN);
  lv_obj_set_style_transform_pivot_y(obj, lv_pct(50), LV_PART_MAIN);
  lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_transform_scale_x(obj, 256, LV_PART_MAIN);
  lv_obj_set_style_transform_scale_y(obj, 256, LV_PART_MAIN);
  lv_obj_set_style_opa(obj, 210, LV_PART_MAIN | LV_STATE_PRESSED);  // ~82%
  lv_obj_set_style_transform_scale_x(obj, 250, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_transform_scale_y(obj, 250, LV_PART_MAIN | LV_STATE_PRESSED);
}

// Momentary action feedback: one transition covers both the press scale-dip and
// a background "activated" flash. Add LV_STATE_USER_1 to flash to `flash_color`
// and remove it to ease back to `rest_color`. Shares one static descriptor.
inline void style_action_feedback(lv_obj_t *obj, uint32_t rest_color,
                                  uint32_t flash_color) {
  static const lv_style_prop_t props[] = {
      LV_STYLE_BG_COLOR, LV_STYLE_OPA, LV_STYLE_TRANSFORM_SCALE_X,
      LV_STYLE_TRANSFORM_SCALE_Y, LV_STYLE_PROP_INV};
  static lv_style_transition_dsc_t tr;
  static bool inited = false;
  if (!inited) {
    lv_style_transition_dsc_init(&tr, props, lv_anim_path_ease_out, 180, 0,
                                 nullptr);
    inited = true;
  }
  lv_obj_set_style_transition(obj, &tr, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(obj, lv_color_hex(rest_color), LV_PART_MAIN);
  lv_obj_set_style_bg_color(obj, lv_color_hex(flash_color),
                            LV_PART_MAIN | LV_STATE_USER_1);
  lv_obj_set_style_transform_pivot_x(obj, lv_pct(50), LV_PART_MAIN);
  lv_obj_set_style_transform_pivot_y(obj, lv_pct(50), LV_PART_MAIN);
  lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_transform_scale_x(obj, 256, LV_PART_MAIN);
  lv_obj_set_style_transform_scale_y(obj, 256, LV_PART_MAIN);
  lv_obj_set_style_opa(obj, 210, LV_PART_MAIN | LV_STATE_PRESSED);  // ~82%
  lv_obj_set_style_transform_scale_x(obj, 250, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_transform_scale_y(obj, 250, LV_PART_MAIN | LV_STATE_PRESSED);
}

// A top-right "more-info" chevron overlaid on a tile. The tap target is a large
// transparent button filling the top-right corner (easy to hit over a slider),
// with the chevron glyph drawn in its corner. `inset` is the glyph's offset from
// the content area (0 when the cell keeps its padding, kTileInset when the cell
// zeroed its padding and positions its labels manually). Sits above the primary
// control; events don't bubble to the cell. Opt-in.
//
// Top-right, not bottom-right: icons anchor top-LEFT and name labels bottom-LEFT
// at full width, so the bottom-right corner is exactly where a long name lands.
// Reserving width in the label can't fix that — on a 2-col tile a single-word
// title has nowhere to wrap to — so the chevron gets the one corner nothing else
// claims. Matches the intent documented on CardConfig::detail. It sits on the
// icon's centre line, reading as a pair with it across the top of the tile.
inline lv_obj_t *add_detail_chevron(lv_obj_t *cell, const lv_font_t *icon_font,
                                    int inset, lv_event_cb_t cb,
                                    void *user_data) {
  lv_obj_t *b = lv_button_create(cell);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, 72, 56);  // generous corner hit surface
  lv_obj_align(b, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  style_press_dip(b);  // soft dim of the whole corner (and its glyph) on press
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);

  // The glyph, plus the same dark twin behind it that add_icon/add_name use:
  // the chevron sits on follow-colour tiles that can go light, where a bare grey
  // glyph washes out. Twin is created first so it renders underneath.
  auto chevron_glyph = [&](uint32_t color, lv_opa_t opa, int nudge) {
    lv_obj_t *l = lv_label_create(b);
    if (icon_font) lv_obj_set_style_text_font(l, icon_font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_opa(l, opa, 0);
    lv_label_set_text(l, "\U000F0142");  // mdi-chevron-right
    // Shrink the 46px glyph so it reads near the name-label size. Pivot right
    // keeps it flush to the corner; pivot mid keeps its vertical centre put
    // while it shrinks. The glyph reuses the icon font and is aligned to the
    // same top edge as the tile's icon, so identical box heights put both
    // centres on one line — no offset arithmetic, and it survives a font
    // size change.
    lv_obj_set_style_transform_pivot_x(l, lv_pct(100), 0);
    lv_obj_set_style_transform_pivot_y(l, lv_pct(50), 0);
    lv_obj_set_style_transform_scale(l, 165, 0);
    lv_obj_align(l, LV_ALIGN_TOP_RIGHT, -inset + nudge, inset + nudge);
    return l;
  };
  chevron_glyph(0x000000, LV_OPA_60, 1);            // shadow twin
  chevron_glyph(0xC8C8C8, LV_OPA_COVER, 0);         // the chevron itself
  return b;
}

// Whole-tile vertical slider styled as a fill: empty track == cell bg so the
// inset reads seamless, orange indicator, no visible knob.
inline void style_fill_slider(lv_obj_t *slider, int radius) {
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x313131), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, radius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(slider, 0, LV_PART_MAIN);

  lv_obj_set_style_bg_color(slider, lv_color_hex(tile_on_color()), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, radius, LV_PART_INDICATOR);

  lv_obj_set_style_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
}

}  // namespace tilehaus
