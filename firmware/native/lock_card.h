#pragma once
#include "lvgl.h"
#include <string>
#include "card.h"
#include "card_style.h"
#include "confirm_dialog.h"
#include "ha.h"

namespace tilehaus {

// Lock tile: icon + name, tinting secure (locked) vs warn (unlocked). Locking is
// immediate; unlocking is guarded by a ConfirmDialog. Owns its state
// subscription.
struct LockCard : Card {
  lv_subject_t locked_{};
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_;
  std::string icon_locked_, icon_unlocked_;
  uint32_t locked_color_ = 0x2E7D32;
  uint32_t unlocked_color_ = 0xC62828;

  TileSpan default_size() const override { return {2, 2}; }

  static void recolor_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<LockCard *>(lv_observer_get_user_data(o));
    bool locked = lv_subject_get_int(s) != 0;
    lv_obj_set_style_bg_color(
        self->cell_,
        lv_color_hex(locked ? self->locked_color_ : self->unlocked_color_), 0);
    set_icon_glyph(self->icon_lbl_,
                   locked ? self->icon_locked_ : self->icon_unlocked_);
  }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    icon_lbl_ = add_icon(cell, fonts.icon, cfg.icon);
    icon_locked_ = cfg.icon;
    icon_unlocked_ = cfg.icon_alt;
    locked_color_ = resolve_color(cfg.active_color, 0x2E7D32);
    unlocked_color_ = resolve_color(cfg.inactive_color, 0xC62828);
    add_name(cell, fonts, cfg.title, cfg.hide_label);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);  // tactile feedback on tap (before lock action / confirm)
    lv_subject_init_int(&locked_, 0);
    lv_subject_add_observer(&locked_, recolor_cb, this);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    lv_subject_t *subj = &locked_;
    ha_subscribe(cfg.entity, nullptr, [subj](const std::string &s) {
      lv_subject_set_int(subj, (s == "locked") ? 1 : 0);
    });
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<LockCard *>(lv_event_get_user_data(e));
      bool locked = lv_subject_get_int(&self->locked_) != 0;
      if (locked) {
        std::string entity = self->entity_;
        ConfirmDialog::show("Unlock " + self->title_ + "?", self->fonts_,
                            "Unlock",
                            [entity]() { ha_call("lock.unlock", entity); });
      } else {
        ha_call("lock.lock", self->entity_);
      }
    }, LV_EVENT_CLICKED, this);
  }
};

}  // namespace tilehaus
