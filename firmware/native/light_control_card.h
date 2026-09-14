#pragma once
#include "lvgl.h"
#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>
#include "card.h"
#include "card_style.h"
#include "light_modal.h"
#include "ha.h"
#include "toggle_state.h"

namespace tilehaus {

// A light tile that opens the all-controls modal on tap. The tile itself looks
// like the other cards (icon + name) and tints on/off with the light state.
struct LightControlCard : Card {
  lv_subject_t on_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_;
  std::string icon_on_, icon_off_;
  uint32_t on_color_ = kTileOnBuiltin;
  uint32_t off_color_ = 0x313131;
  int32_t cfg_active_color_ = -1;
  std::unique_ptr<LightModal> modal_;
  bool follow_color_ = false;   // tile tint tracks the light's own colour
  int fr_ = -1, fg_ = -1, fb_ = -1;  // last rgb_color (-1 = unknown)
  int fbri_ = 255;              // last brightness 0..255

  TileSpan default_size() const override { return {2, 2}; }

  // Parse up to three integers out of an HA colour attribute, tolerating either
  // "[r, g, b]" or "(r, g, b)" (list vs tuple stringification). Returns count.
  static int parse_ints(const std::string &s, int *out, int max) {
    int n = 0;
    size_t i = 0;
    while (i < s.size() && n < max) {
      if (isdigit(static_cast<unsigned char>(s[i]))) {
        out[n++] = std::atoi(s.c_str() + i);
        while (i < s.size() && isdigit(static_cast<unsigned char>(s[i]))) ++i;
      } else {
        ++i;
      }
    }
    return n;
  }

  // The tile's "on" tint: the light's colour scaled by brightness (with a floor
  // so a dim light stays visible) when follow_color and a colour is known;
  // otherwise the fixed on_color_.
  uint32_t on_tint() const {
    if (!follow_color_ || fr_ < 0) return on_color_;
    int scale = fbri_ < 60 ? 60 : fbri_;  // floor keeps low-brightness visible
    int r = fr_ * scale / 255, g = fg_ * scale / 255, b = fb_ * scale / 255;
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
  }

  void apply_color() {
    bool on = lv_subject_get_int(&on_) != 0;
    // The white icon/name stay legible on a light follow-colour tint via the
    // soft drop shadow baked into add_icon/add_name (uniform across all tiles).
    lv_obj_set_style_bg_color(cell_, lv_color_hex(on ? on_tint() : off_color_), 0);
    set_icon_glyph(icon_lbl_, on ? icon_on_ : icon_off_);
  }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    static_cast<LightControlCard *>(lv_observer_get_user_data(o))->apply_color();
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    icon_on_ = cfg.icon;
    icon_off_ = cfg.icon_alt;
    cfg_active_color_ = cfg.active_color;
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    off_color_ = resolve_color(cfg.inactive_color, 0x313131);
    follow_color_ = cfg.follow_color;
    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);  // tactile feedback on tap, right before the modal opens
    lv_subject_init_int(&on_, 0);
    lv_subject_add_observer(&on_, recolor_cb, this);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    lv_subject_t *subj = &on_;
    ha_subscribe(cfg.entity, nullptr, [subj](const std::string &s) {
      lv_subject_set_int(subj, (s == "on") ? 1 : 0);
    });
    if (follow_color_) {
      ha_subscribe(cfg.entity, "rgb_color", [this](const std::string &s) {
        int c[3];
        if (parse_ints(s, c, 3) == 3) { fr_ = c[0]; fg_ = c[1]; fb_ = c[2]; }
        else fr_ = fg_ = fb_ = -1;  // no colour (e.g. light off) -> default tint
        apply_color();
        // Mirror the tile's follow-colour onto the modal's brightness slider fill.
        if (modal_) {
          uint32_t fill = fr_ >= 0
              ? ((static_cast<uint32_t>(fr_) << 16) |
                 (static_cast<uint32_t>(fg_) << 8) | static_cast<uint32_t>(fb_))
              : tile_on_color();
          modal_->set_brightness_fill(fill);
        }
      });
      ha_subscribe(cfg.entity, "brightness", [this](const std::string &s) {
        fbri_ = s.empty() ? 255 : std::atoi(s.c_str());
        apply_color();
      });
    }
    // Build the modal now (hidden), not lazily on first tap, so its Home
    // Assistant subscriptions are registered before HA's one-time initial-state
    // push at connect. ESPHome walks the subscription list for HA only once, at
    // the connect handshake; a subscription added later (on first tap) misses
    // that push, so the controls would show defaults until the next change — and
    // attributes no other card watches (color temp, color) would never update at
    // all. The heavy color-wheel image stays lazily generated on first use.
    modal_.reset(new LightModal(entity_, title_, fonts_, &on_));
    modal_->build();
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<LightControlCard *>(lv_event_get_user_data(e));
      if (self->modal_) self->modal_->show();
    }, LV_EVENT_CLICKED, this);
  }

  // apply_color() reads on_/on_tint()/off_color_ directly and repaints, so
  // this works regardless of whether lv_subject_set_int would re-notify (it
  // would not, for an unchanged value). Also refreshes the modal's brightness
  // fill fallback (tile_on_color()) when no explicit light colour is known
  // yet — LightModal exposes set_brightness_fill() for exactly this, already
  // used by the rgb_color subscription above.
  void restyle() override {
    on_color_ = resolve_color(cfg_active_color_, tile_on_color());
    apply_color();
    if (follow_color_ && fr_ < 0 && modal_) modal_->set_brightness_fill(tile_on_color());
  }
};

}  // namespace tilehaus
