#pragma once
#include "lvgl.h"
#include <cstdint>
#include <string>
#include <vector>
#include "card.h"        // CardFonts
#include "ha.h"

namespace tilehaus {

// A labeled dropdown bound to one HA attribute, matching HA's climate Mode /
// Preset buttons: a small caption ("Mode") over a button that carries an icon on
// its left (inside the button) and the selected value's text. The icon reflects
// the current value (one glyph per option). Picking one calls `service` with
// {key: value}; an HA change re-selects the matching option and swaps the icon
// (guarded so reflecting a push does not echo a command back).
struct ClimateDropdown {
  lv_obj_t *dd_ = nullptr;
  lv_obj_t *icon_lbl_ = nullptr;
  std::vector<std::string> values_;
  std::vector<std::string> icons_;  // one glyph per value (HA-style)
  std::string entity_, service_, key_;
  const char *attr_ = nullptr;
  bool applying_ = false;

  static void changed_cb(lv_event_t *e) {
    auto *self = static_cast<ClimateDropdown *>(lv_event_get_user_data(e));
    if (self->applying_) return;
    int i = static_cast<int>(lv_dropdown_get_selected(self->dd_));
    if (i >= 0 && i < static_cast<int>(self->values_.size()))
      ha_call_kv(self->service_.c_str(), self->entity_, self->key_.c_str(),
                 self->values_[i]);
  }

  void build(lv_obj_t *parent, const CardFonts &fonts,
             const std::vector<std::string> &icons, const char *caption,
             const std::string &entity, const char *service, const char *key,
             const char *attr, const std::vector<std::string> &labels,
             const std::vector<std::string> &values, int w) {
    entity_ = entity;
    service_ = service;
    key_ = key;
    attr_ = attr;
    values_ = values;
    icons_ = icons;

    // Column: caption over the button.
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, w, LV_SIZE_CONTENT);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(box, 4, 0);

    lv_obj_t *cap = lv_label_create(box);
    if (fonts.body) lv_obj_set_style_text_font(cap, fonts.body, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xB0B0B0), 0);
    lv_label_set_text(cap, caption);

    dd_ = lv_dropdown_create(box);
    lv_obj_set_width(dd_, w);
    std::string opts;
    for (size_t i = 0; i < labels.size(); ++i) {
      if (i) opts += "\n";
      opts += labels[i];
    }
    lv_dropdown_set_options(dd_, opts.c_str());
    // The default arrow is LV_SYMBOL_DOWN (a Font Awesome glyph absent from our
    // Roboto subset) — it renders as a tofu box, so drop it.
    lv_dropdown_set_symbol(dd_, nullptr);
    if (fonts.body) lv_obj_set_style_text_font(dd_, fonts.body, 0);
    lv_obj_set_style_bg_color(dd_, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(dd_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(dd_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(dd_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_border_width(dd_, 1, 0);
    lv_obj_set_style_border_color(dd_, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(dd_, 12, 0);
    lv_obj_set_style_pad_ver(dd_, 16, 0);
    lv_obj_set_style_pad_right(dd_, 16, 0);
    // Leave room on the left for the icon that sits inside the button.
    lv_obj_set_style_pad_left(dd_, icons_.empty() ? 16 : 76, 0);
    lv_obj_add_event_cb(dd_, changed_cb, LV_EVENT_VALUE_CHANGED, this);

    // Icon inside the button, on the left. lv_obj_align references the content
    // area (inside pad_left=76), so a negative x pushes it back into the
    // reserved left padding (~16px from the box edge), clear of the text.
    if (!icons_.empty()) {
      icon_lbl_ = lv_label_create(dd_);
      if (fonts.icon) lv_obj_set_style_text_font(icon_lbl_, fonts.icon, 0);
      lv_obj_set_style_text_color(icon_lbl_, lv_color_hex(0x808080), 0);
      lv_label_set_text(icon_lbl_, icons_[0].c_str());
      lv_obj_align(icon_lbl_, LV_ALIGN_LEFT_MID, -60, 0);
    }

    // Style the opened option list for the dark theme.
    lv_obj_t *list = lv_dropdown_get_list(dd_);
    if (list) {
      lv_obj_set_style_bg_color(list, lv_color_hex(0x2A2A2A), 0);
      lv_obj_set_style_text_color(list, lv_color_hex(0xFFFFFF), 0);
      lv_obj_set_style_border_color(list, lv_color_hex(0x444444), 0);
      lv_obj_set_style_radius(list, 12, 0);
      // Clip children to the rounded rect so the option highlight doesn't paint
      // a square corner over the list's rounded top-left.
      lv_obj_set_style_clip_corner(list, true, 0);
      if (fonts.body) lv_obj_set_style_text_font(list, fonts.body, 0);
      lv_obj_set_style_pad_all(list, 6, 0);
      // Highlight the pressed/selected option in the HA rust tone.
      lv_obj_set_style_bg_opa(list, LV_OPA_COVER,
                              LV_PART_SELECTED | LV_STATE_CHECKED);
      lv_obj_set_style_bg_color(list, lv_color_hex(0xB5451B),
                                LV_PART_SELECTED | LV_STATE_CHECKED);
    }
  }

  void select_value(const std::string &v) {
    for (size_t i = 0; i < values_.size(); ++i) {
      if (values_[i] == v) {
        applying_ = true;
        lv_dropdown_set_selected(dd_, static_cast<uint16_t>(i));
        applying_ = false;
        if (icon_lbl_ && i < icons_.size())
          lv_label_set_text(icon_lbl_, icons_[i].c_str());
        return;
      }
    }
  }

  void bind() {
    ha_subscribe(entity_, attr_,
                 [this](const std::string &s) { select_value(s); });
  }
};

}  // namespace tilehaus
