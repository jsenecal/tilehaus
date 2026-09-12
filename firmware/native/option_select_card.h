#pragma once
#include "lvgl.h"
#include <memory>
#include <string>
#include "card.h"
#include "card_style.h"
#include "overlay.h"
#include "option_list_tab.h"
#include "ha.h"

namespace tilehaus {

// Modal wrapper around OptionListTab for a standalone `select` entity: a
// full-screen scrollable pick-list. Built once (hidden) at boot so its
// subscriptions register before HA's initial-state push.
struct OptionSelectModal {
  Overlay overlay_;
  std::string entity_, title_;
  CardFonts fonts_;
  OptionListTab list_;
  bool built_ = false;

  OptionSelectModal(const std::string &entity, const std::string &title,
                    const CardFonts &fonts)
      : entity_(entity), title_(title), fonts_(fonts) {}

  void build() {
    overlay_.build(title_, fonts_.icon, fonts_.body);
    overlay_.on_close_ = [this]() { hide(); };
    list_.build(overlay_.content(), fonts_, entity_, "options", entity_, nullptr,
                "select.select_option", entity_, "option");
    built_ = true;
  }
  void show() {
    if (!built_) build();
    overlay_.show();
  }
  void hide() { overlay_.hide(); }
};

// Select tile: icon + name + the current option (centered). Tap opens the
// pick-list modal.
struct OptionSelectCard : Card {
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *val_lbl_ = nullptr;
  CardFonts fonts_{};
  std::string entity_, title_;
  std::unique_ptr<OptionSelectModal> modal_;

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg,
             const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    title_ = cfg.title;
    add_icon(cell, fonts.icon, cfg.icon);
    add_name(cell, fonts, cfg.title, cfg.hide_label);

    val_lbl_ = lv_label_create(cell);
    if (fonts.body) lv_obj_set_style_text_font(val_lbl_, fonts.body, 0);
    lv_obj_set_style_text_color(val_lbl_, lv_color_hex(0xC8C8C8), 0);
    lv_label_set_long_mode(val_lbl_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(val_lbl_, LV_PCT(80));
    lv_obj_set_style_text_align(val_lbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(val_lbl_);
    lv_label_set_text(val_lbl_, "");

    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
  }

  void bind(const CardConfig &cfg) override {
    entity_ = cfg.entity;
    ha_subscribe(entity_, nullptr, [this](const std::string &s) {
      bool bad = s.empty() || s == "unknown" || s == "unavailable";
      lv_label_set_text(val_lbl_,
                        bad ? "" : OptionListTab::display_label(s).c_str());
    });
    modal_.reset(new OptionSelectModal(entity_, title_, fonts_));
    modal_->build();
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      auto *self = static_cast<OptionSelectCard *>(lv_event_get_user_data(e));
      if (self->modal_) self->modal_->show();
    }, LV_EVENT_CLICKED, this);
  }
};

}  // namespace tilehaus
