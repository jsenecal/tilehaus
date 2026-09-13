#pragma once
#include "lvgl.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "card.h"
#include "card_style.h"
#include "tabbed_modal.h"
#include "ha.h"

namespace tilehaus {

// Cover detail modal, split across the shared tab rail (like the light/WLED
// modals): a Position tab (fill slider with flash/fade % + glide), a Controls
// tab (Open / Stop / Close), and a Presets tab (0/25/50/75/100%). Position is HA
// "open percent" (100 = open). Built hidden at boot.
struct CoverModal {
  TabbedModal tabs_;
  std::string entity_, title_;
  CardFonts fonts_;
  bool built_ = false;
  lv_obj_t *pos_ = nullptr;     // transparent input slider (open %)
  lv_obj_t *fill_ = nullptr;    // top-anchored shade panel (height = closed %)
  lv_obj_t *val_ = nullptr;
  lv_timer_t *hide_ = nullptr;
  lv_subject_t pos_subj_{};     // open % (0..100)
  int touch_ = 0;
  int fill_closed_ = 0;         // current shade height (closed %), anim start

  CoverModal(const std::string &entity, const std::string &title,
             const CardFonts &fonts)
      : entity_(entity), title_(title), fonts_(fonts) {
    tabs_.title_ = title;
    tabs_.fonts_ = fonts;
  }

  static void hide_cb(lv_timer_t *t) {
    auto *self = static_cast<CoverModal *>(lv_timer_get_user_data(t));
    lv_obj_fade_out(self->val_, 400, 0);
    lv_timer_pause(t);
  }
  static void fill_h_cb(void *var, int32_t v) {  // shade height = closed %
    lv_obj_set_height(static_cast<lv_obj_t *>(var), LV_PCT(v));
  }
  // Observer on the open-% subject: drive the top-anchored shade (glide on
  // HA/tap, track the finger on drag) and flash the readout.
  static void pos_change_cb(lv_observer_t *o, lv_subject_t *s) {
    auto *self = static_cast<CoverModal *>(lv_observer_get_user_data(o));
    int open = lv_subject_get_int(s);
    int closed = 100 - open;
    if (self->fill_) {
      if (self->touch_ == 1) {
        lv_anim_del(self->fill_, fill_h_cb);
        lv_obj_set_height(self->fill_, LV_PCT(closed));
      } else {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, self->fill_);
        lv_anim_set_exec_cb(&a, fill_h_cb);
        lv_anim_set_values(&a, self->fill_closed_, closed);
        lv_anim_set_duration(&a, kSliderAnimMs);
        lv_anim_start(&a);
      }
      self->fill_closed_ = closed;
    }
    if (self->val_) {
      set_val(self->val_, open);
      lv_anim_del(self->val_, nullptr);
      lv_obj_set_style_opa(self->val_, LV_OPA_COVER, 0);
      lv_timer_reset(self->hide_);
      lv_timer_resume(self->hide_);
    }
  }
  static void set_val(lv_obj_t *l, int v) {
    char b[8];
    std::snprintf(b, sizeof(b), "%d%%", v);
    lv_label_set_text(l, b);
  }

  static lv_obj_t *centered_col(lv_obj_t *panel, int pad_row) {
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel, kTileInset, 0);
    lv_obj_set_style_pad_row(panel, pad_row, 0);
    return panel;
  }

  // ---- Position tab ------------------------------------------------------
  void build_position(lv_obj_t *panel, int ph) {
    centered_col(panel, 12);
    lv_obj_t *cap = lv_label_create(panel);
    if (fonts_.body) lv_obj_set_style_text_font(cap, fonts_.body, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xB0B0B0), 0);
    lv_label_set_text(cap, "Position");

    // "Window" box with a shade that descends from the TOP (like the tile): a
    // top-anchored orange fill whose height is the closed %, over a transparent
    // vertical slider that captures input (value = open %).
    int h = ph - 110;
    if (h < 150) h = 150;
    lv_obj_t *win = lv_obj_create(panel);
    lv_obj_remove_style_all(win);
    lv_obj_set_size(win, 150, h);
    lv_obj_set_style_bg_color(win, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_bg_opa(win, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(win, 14, 0);
    lv_obj_set_style_clip_corner(win, true, 0);
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLLABLE);

    fill_ = lv_obj_create(win);
    lv_obj_remove_style_all(fill_);
    lv_obj_set_style_bg_color(fill_, lv_color_hex(tile_on_color()), 0);
    lv_obj_set_style_bg_opa(fill_, LV_OPA_COVER, 0);
    lv_obj_set_width(fill_, LV_PCT(100));
    lv_obj_set_height(fill_, LV_PCT(0));
    lv_obj_align(fill_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(fill_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(fill_, LV_OBJ_FLAG_SCROLLABLE);

    val_ = lv_label_create(win);
    if (fonts_.value) lv_obj_set_style_text_font(val_, fonts_.value, 0);
    lv_obj_set_style_text_color(val_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(val_);
    lv_obj_set_style_opa(val_, LV_OPA_TRANSP, 0);
    hide_ = lv_timer_create(hide_cb, 1500, this);
    lv_timer_pause(hide_);

    pos_ = lv_slider_create(win);
    lv_slider_set_orientation(pos_, LV_SLIDER_ORIENTATION_VERTICAL);
    lv_slider_set_range(pos_, 0, 100);
    lv_obj_set_size(pos_, LV_PCT(100), LV_PCT(100));
    lv_obj_center(pos_);
    lv_obj_set_style_opa(pos_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_opa(pos_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_opa(pos_, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_subject_init_int(&pos_subj_, 0);
    lv_slider_bind_value(pos_, &pos_subj_);
    lv_subject_add_observer(&pos_subj_, pos_change_cb, this);

    lv_obj_add_event_cb(pos_, [](lv_event_t *e) {
      static_cast<CoverModal *>(lv_event_get_user_data(e))->touch_ = 1;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSED | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(pos_, [](lv_event_t *e) {
      static_cast<CoverModal *>(lv_event_get_user_data(e))->touch_ = 1;
    }, static_cast<lv_event_code_t>(LV_EVENT_PRESSING | LV_EVENT_PREPROCESS), this);
    lv_obj_add_event_cb(pos_, [](lv_event_t *e) {
      auto *s = static_cast<CoverModal *>(lv_event_get_user_data(e));
      s->touch_ = 0;
      ha_call_kv("cover.set_cover_position", s->entity_, "position",
                 std::to_string(static_cast<int>(lv_slider_get_value(s->pos_))));
    }, LV_EVENT_RELEASED, this);

    ha_subscribe(entity_, "current_position", [this](const std::string &s) {
      if (touch_ == 1) return;
      if (s.empty() || s == "unknown" || s == "unavailable" || s == "None")
        return;
      int open = std::atoi(s.c_str());
      lv_subject_set_int(&pos_subj_, open < 0 ? 0 : (open > 100 ? 100 : open));
    });
  }

  // ---- Controls tab (Open / Stop / Close) --------------------------------
  lv_obj_t *ctl_btn(lv_obj_t *parent, const char *glyph, const char *label,
                    int w, lv_event_cb_t cb) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, 76);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    style_press_dip(b);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, 12, 0);
    lv_obj_t *g = lv_label_create(b);
    if (fonts_.icon) lv_obj_set_style_text_font(g, fonts_.icon, 0);
    lv_obj_set_style_text_color(g, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(g, glyph);
    lv_obj_t *t = lv_label_create(b);
    if (fonts_.body) lv_obj_set_style_text_font(t, fonts_.body, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(t, label);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
    return b;
  }

  void build_controls(lv_obj_t *panel, int pw) {
    centered_col(panel, 14);
    int w = pw * 6 / 10;
    ctl_btn(panel, "\U000F0143", "Open", w, [](lv_event_t *e) {  // chevron-up
      ha_call("cover.open_cover",
              static_cast<CoverModal *>(lv_event_get_user_data(e))->entity_);
    });
    ctl_btn(panel, "\U000F04DB", "Stop", w, [](lv_event_t *e) {  // stop
      ha_call("cover.stop_cover",
              static_cast<CoverModal *>(lv_event_get_user_data(e))->entity_);
    });
    ctl_btn(panel, "\U000F0140", "Close", w, [](lv_event_t *e) {  // chevron-down
      ha_call("cover.close_cover",
              static_cast<CoverModal *>(lv_event_get_user_data(e))->entity_);
    });
  }

  // ---- Presets tab -------------------------------------------------------
  static void preset_cb(lv_event_t *e) {
    auto *self = static_cast<CoverModal *>(lv_event_get_user_data(e));
    lv_obj_t *bt = lv_event_get_target_obj(e);
    int p = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(bt)));
    ha_call_kv("cover.set_cover_position", self->entity_, "position",
               std::to_string(p));
  }
  void preset_btn(lv_obj_t *parent, const char *label, int pos, int w) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, 64);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    style_press_dip(b);
    lv_obj_set_user_data(b, reinterpret_cast<void *>(static_cast<intptr_t>(pos)));
    lv_obj_add_event_cb(b, preset_cb, LV_EVENT_CLICKED, this);
    lv_obj_t *l = lv_label_create(b);
    if (fonts_.medium) lv_obj_set_style_text_font(l, fonts_.medium, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(l, label);
    lv_obj_center(l);
  }
  void build_presets(lv_obj_t *panel, int pw) {
    centered_col(panel, 12);  // one preset tile per row, stacked
    int w = pw * 6 / 10;
    preset_btn(panel, "Open", 100, w);
    preset_btn(panel, "75%", 75, w);
    preset_btn(panel, "50%", 50, w);
    preset_btn(panel, "25%", 25, w);
    preset_btn(panel, "Closed", 0, w);
  }

  void build() {
    tabs_.add_tab("\U000F0E79", [this](lv_obj_t *p, int, int ph) {  // up-down
      build_position(p, ph);
    });
    tabs_.add_tab("\U000F111C", [this](lv_obj_t *p, int pw, int) {  // shutter
      build_controls(p, pw);
    });
    tabs_.add_tab("\U000F0241", [this](lv_obj_t *p, int pw, int) {  // flash
      build_presets(p, pw);
    });
    tabs_.build_chrome();
    built_ = true;
  }

  void show() {
    if (!built_) build();
    tabs_.show();
  }
  void hide() { tabs_.hide(); }
};

}  // namespace tilehaus
