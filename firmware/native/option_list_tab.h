#pragma once
#include "lvgl.h"
#include <cstdint>
#include <string>
#include <vector>
#include "card.h"
#include "card_style.h"
#include "ha.h"

namespace tilehaus {

// A scrollable single-select option list, shared by the WLED Presets, Effects,
// and Palette tabs — each is "pick one from a list, apply it, highlight the
// active one". The option list and the current selection come from HA
// attributes/state; applying calls a service with the exact option string.
//
//   build(panel, fonts,
//         list_entity, list_attr,   // where options[] come from
//         cur_entity, cur_attr,     // current selection (nullptr attr = state)
//         service, set_entity, set_key)  // how to apply
//
// The options list is parsed by scanning quoted tokens ('...' / "..."), so it is
// JSON / Python-repr / comma / emoji safe. Row labels strip non-ASCII (the panel
// font can't render WLED emoji); the service call sends the full original value.
struct OptionListTab {
  lv_obj_t *list_ = nullptr;
  CardFonts fonts_{};
  std::vector<std::string> values_;
  std::vector<lv_obj_t *> rows_;
  std::string list_raw_, active_;
  std::string set_service_, set_entity_, set_key_;

  static std::vector<std::string> parse_options(const std::string &s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
      char c = s[i];
      if (c == '\'' || c == '"') {
        char q = c;
        size_t j = i + 1;
        std::string tok;
        while (j < s.size() && s[j] != q) tok.push_back(s[j++]);
        out.push_back(tok);
        i = j + 1;
      } else {
        ++i;
      }
    }
    return out;
  }

  static std::string display_label(const std::string &v) {
    std::string out;
    for (unsigned char ch : v)
      if (ch < 0x80) out.push_back(static_cast<char>(ch));
    while (!out.empty() && (out.back() == ' ' || out.back() == '\t'))
      out.pop_back();
    return out;
  }

  void build(lv_obj_t *panel, const CardFonts &fonts,
             const std::string &list_entity, const char *list_attr,
             const std::string &cur_entity, const char *cur_attr,
             const char *service, const std::string &set_entity,
             const char *set_key) {
    fonts_ = fonts;
    set_service_ = service;
    set_entity_ = set_entity;
    set_key_ = set_key;

    list_ = lv_obj_create(panel);
    lv_obj_remove_style_all(list_);
    // Percentage so it fills the panel at layout time — the panel's pixel size is
    // not yet computed when tabs build (the modal is built hidden at boot).
    lv_obj_set_size(list_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list_, kTileInset, 0);
    lv_obj_set_style_pad_row(list_, 8, 0);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);

    ha_subscribe(list_entity, list_attr, [this](const std::string &s) {
      if (s == list_raw_) return;
      list_raw_ = s;
      rebuild(parse_options(s));
    });
    ha_subscribe(cur_entity, cur_attr, [this](const std::string &s) {
      active_ = s;
      apply_active();
    });
  }

  static void tap_cb(lv_event_t *e) {
    auto *self = static_cast<OptionListTab *>(lv_event_get_user_data(e));
    lv_obj_t *r = lv_event_get_target_obj(e);
    int idx =
        static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(r)));
    if (idx >= 0 && idx < static_cast<int>(self->values_.size()))
      ha_call_kv(self->set_service_.c_str(), self->set_entity_,
                 self->set_key_.c_str(), self->values_[idx]);
  }

  void rebuild(const std::vector<std::string> &opts) {
    lv_obj_clean(list_);
    rows_.clear();
    values_ = opts;
    for (size_t i = 0; i < opts.size(); ++i) {
      lv_obj_t *r = lv_button_create(list_);
      lv_obj_remove_style_all(r);
      lv_obj_set_width(r, LV_PCT(100));
      lv_obj_set_height(r, LV_SIZE_CONTENT);
      lv_obj_set_style_bg_color(r, lv_color_hex(0x2A2A2A), 0);
      lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(r, 10, 0);
      lv_obj_set_style_pad_hor(r, 16, 0);
      lv_obj_set_style_pad_ver(r, 14, 0);
      lv_obj_set_style_bg_color(r, lv_color_hex(0xB5451B), LV_STATE_CHECKED);
      style_press_dip(r);
      lv_obj_set_user_data(r,
                           reinterpret_cast<void *>(static_cast<intptr_t>(i)));
      lv_obj_add_event_cb(r, tap_cb, LV_EVENT_CLICKED, this);
      lv_obj_t *l = lv_label_create(r);
      if (fonts_.body) lv_obj_set_style_text_font(l, fonts_.body, 0);
      lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
      lv_label_set_text(l, display_label(opts[i]).c_str());
      lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
      rows_.push_back(r);
    }
    apply_active();
  }

  void apply_active() {
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (i < values_.size() && values_[i] == active_) {
        lv_obj_add_state(rows_[i], LV_STATE_CHECKED);
        lv_obj_scroll_to_view(rows_[i], LV_ANIM_OFF);
      } else {
        lv_obj_remove_state(rows_[i], LV_STATE_CHECKED);
      }
    }
  }
};

}  // namespace tilehaus
