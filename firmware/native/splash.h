#pragma once
#include "lvgl.h"
#include "card.h"  // CardFonts

namespace tilehaus {

// Boot / connection splash: a full-screen black overlay on the top layer (above
// the grid and any modal) with a centered icon + title + grey subtitle, mirrored
// on espcontrol's loading screen. Driven through connection phases from the
// ESPHome wifi/api triggers, then faded out once Home Assistant is connected.
struct Splash {
  lv_obj_t *overlay = nullptr;
  lv_obj_t *icon = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *sub = nullptr;
};

inline Splash &splash() {
  static Splash s;
  return s;
}

inline lv_obj_t *splash_label(lv_obj_t *parent, const lv_font_t *font,
                              uint32_t color) {
  lv_obj_t *l = lv_label_create(parent);
  if (font) lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_text(l, "");
  return l;
}

inline void splash_build(const CardFonts &fonts) {
  Splash &s = splash();
  if (s.overlay) return;

  s.overlay = lv_obj_create(lv_layer_top());  // above the page and any modal
  lv_obj_remove_style_all(s.overlay);
  lv_obj_set_size(s.overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(s.overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s.overlay, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s.overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s.overlay, LV_OBJ_FLAG_CLICKABLE);  // swallow taps while up

  lv_obj_t *box = lv_obj_create(s.overlay);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, LV_PCT(70), LV_SIZE_CONTENT);
  lv_obj_center(box);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(box, 16, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  s.icon = splash_label(box, fonts.icon, 0xFFFFFF);
  s.title = splash_label(box, fonts.medium, 0xFFFFFF);
  s.sub = splash_label(box, fonts.body, 0xB0B0B0);
}

// Update any of the three lines; pass nullptr to leave a line unchanged.
inline void splash_set(const char *icon_glyph, const char *title,
                       const char *sub) {
  Splash &s = splash();
  if (!s.overlay) return;
  if (icon_glyph && s.icon) lv_label_set_text(s.icon, icon_glyph);
  if (title && s.title) lv_label_set_text(s.title, title);
  if (sub && s.sub) lv_label_set_text(s.sub, sub);
}

inline void splash_show() {
  Splash &s = splash();
  if (!s.overlay) return;
  lv_anim_del(s.overlay, nullptr);  // cancel any in-flight fade-out
  lv_obj_clear_flag(s.overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(s.overlay, LV_OPA_COVER, 0);
  lv_obj_move_foreground(s.overlay);
}

inline void splash_opa_exec(void *o, int32_t v) {
  lv_obj_set_style_opa(static_cast<lv_obj_t *>(o), static_cast<lv_opa_t>(v), 0);
}
inline void splash_hidden_cb(lv_anim_t *a) {
  lv_obj_t *o = static_cast<lv_obj_t *>(a->var);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);      // stop swallowing touches
  lv_obj_set_style_opa(o, LV_OPA_COVER, 0);    // reset for the next show
}

// Fade out over 400ms, then add LV_OBJ_FLAG_HIDDEN. LVGL 9's lv_obj_fade_out
// only animates opacity to transparent — it does NOT hide the object — so a
// full-screen CLICKABLE overlay left at opa 0 keeps intercepting every touch
// (the grid renders through it but nothing is tappable = "frozen"). We drive the
// fade manually with a completed_cb that hides it, mirroring Overlay::hide().
inline void splash_hide() {
  Splash &s = splash();
  if (!s.overlay) return;
  lv_anim_del(s.overlay, splash_opa_exec);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s.overlay);
  lv_anim_set_exec_cb(&a, splash_opa_exec);
  lv_anim_set_values(&a, lv_obj_get_style_opa(s.overlay, LV_PART_MAIN),
                     LV_OPA_TRANSP);
  lv_anim_set_duration(&a, 400);
  lv_anim_set_completed_cb(&a, splash_hidden_cb);
  lv_anim_start(&a);
}

}  // namespace tilehaus
