#pragma once
#include "lvgl.h"
#include <cstdint>
#include <cstdlib>
#include <string>
#include "card.h"         // CardFonts
#include "card_style.h"   // kTileInset, style_press_dip
#include "ha.h"

namespace tilehaus {

// Color tab: a scrollable palette of preset colour swatches — light on memory
// (no wheel image / PSRAM buffer). Tapping a dot sends rgb_color; the swatch
// nearest the light's current colour glows. Owns its rgb_color subscription.
struct ColorTab {
  lv_obj_t *panel_ = nullptr;
  std::string entity_;

  // Glow the swatch nearest the light's current colour (squared RGB distance).
  void highlight(int r, int g, int b) {
    if (!panel_) return;
    uint32_t n = lv_obj_get_child_count(panel_);
    long best = -1;
    uint32_t best_i = 0;
    for (uint32_t i = 0; i < n; ++i) {
      lv_obj_t *dot = lv_obj_get_child(panel_, i);
      uint32_t c = static_cast<uint32_t>(
          reinterpret_cast<intptr_t>(lv_obj_get_user_data(dot)));
      long dr = static_cast<int>((c >> 16) & 0xFF) - r;
      long dg = static_cast<int>((c >> 8) & 0xFF) - g;
      long db = static_cast<int>(c & 0xFF) - b;
      long dist = dr * dr + dg * dg + db * db;
      if (best < 0 || dist < best) { best = dist; best_i = i; }
    }
    for (uint32_t i = 0; i < n; ++i)
      lv_obj_set_style_shadow_width(lv_obj_get_child(panel_, i),
                                    i == best_i ? 44 : 0, 0);
  }

  void build(lv_obj_t *panel, const std::string &entity) {
    entity_ = entity;
    panel_ = panel;
    static const uint32_t kSwatches[] = {
        // bright hues
        0xFF3B30, 0xFF7A1A, 0xFFB300, 0xFFE000, 0xB4EC1E, 0x4CD964, 0x00D3A7,
        0x00C2D6, 0x2A9BFF, 0x3D5AFE, 0x8E4CFF, 0xE040FB, 0xFF4FA3, 0xFF6F5A,
        // lighter / pastel tones
        0xFFB3AE, 0xFFD1A6, 0xFFE8A8, 0xF3F0A8, 0xD8F0A8, 0xB8ECC4, 0xAEE9DC,
        0xB3E5EA, 0xBBD9FF, 0xC7CCFF, 0xDCC6FF, 0xF3C6FF, 0xFFC9E0, 0xF0D9C0,
        // whites, warm -> cool
        0xFFCE96, 0xFFF0D8, 0xFFFFFF, 0xE6EEFF,
    };
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel, kTileInset, 0);
    lv_obj_set_style_pad_row(panel, 18, 0);
    lv_obj_set_style_pad_column(panel, 18, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    for (uint32_t c : kSwatches) {
      lv_obj_t *dot = lv_obj_create(panel);
      lv_obj_remove_style_all(dot);
      lv_obj_set_size(dot, 88, 88);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, lv_color_hex(c), 0);
      lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
      lv_obj_set_style_shadow_color(dot, lv_color_hex(c), 0);  // glow in own hue
      lv_obj_set_style_shadow_spread(dot, 6, 0);
      lv_obj_set_style_shadow_opa(dot, LV_OPA_COVER, 0);
      lv_obj_set_style_shadow_width(dot, 0, 0);  // glows only when active
      lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(dot, LV_OBJ_FLAG_CLICKABLE);
      style_press_dip(dot);
      lv_obj_set_user_data(dot,
                           reinterpret_cast<void *>(static_cast<intptr_t>(c)));
      lv_obj_add_event_cb(dot, [](lv_event_t *e) {
        auto *self = static_cast<ColorTab *>(lv_event_get_user_data(e));
        lv_obj_t *d = static_cast<lv_obj_t *>(lv_event_get_target(e));
        uint32_t col = static_cast<uint32_t>(
            reinterpret_cast<intptr_t>(lv_obj_get_user_data(d)));
        ha_call_rgb(self->entity_, (col >> 16) & 0xFF, (col >> 8) & 0xFF,
                    col & 0xFF);
      }, LV_EVENT_CLICKED, this);
    }

    ha_subscribe(entity_, "rgb_color", [this](const std::string &s) {
      // HA sends "[r, g, b]"; pull the three integers and glow the nearest dot.
      int rgb[3] = {0, 0, 0};
      int idx = 0;
      const char *p = s.c_str();
      while (*p && idx < 3) {
        while (*p && *p != '-' && (*p < '0' || *p > '9')) ++p;
        if (!*p) break;
        rgb[idx++] = static_cast<int>(strtol(p, const_cast<char **>(&p), 10));
      }
      if (idx == 3) highlight(rgb[0], rgb[1], rgb[2]);
    });
  }
};

}  // namespace tilehaus
