#pragma once
#include "lvgl.h"
#include <functional>
#include <string>
#include <utility>
#include "card.h"         // CardFonts
#include "card_style.h"   // style_button_feedback

namespace tilehaus {

// A lightweight yes/no confirmation on lv_layer_top(): a dimmed backdrop plus a
// centered card with a title and Cancel / Confirm buttons. Built per use and
// fully destroyed on either choice. Reusable by any tile needing a guard.
struct ConfirmDialog {
  // Heap context so the callback + scrim survive past show()'s stack frame.
  struct Ctx {
    lv_obj_t *scrim;
    std::function<void()> on_confirm;
  };

  static lv_obj_t *make_button(lv_obj_t *parent, const CardFonts &fonts,
                               const char *text, uint32_t color) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 210, 84);
    lv_obj_set_style_radius(b, 12, 0);
    style_button_feedback(b, color, color);
    lv_obj_t *l = lv_label_create(b);
    if (fonts.body) lv_obj_set_style_text_font(l, fonts.body, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return b;
  }

  static void show(const std::string &title, const CardFonts &fonts,
                   const char *confirm_text, std::function<void()> on_confirm) {
    lv_obj_t *scrim = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(scrim);
    lv_obj_set_size(scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(scrim, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_60, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);  // swallow taps behind
    lv_obj_clear_flag(scrim, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(scrim);
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x313131), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_size(card, 520, 240);
    lv_obj_center(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(card);
    if (fonts.body) lv_obj_set_style_text_font(t, fonts.body, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, LV_PCT(100));
    lv_label_set_text(t, title.c_str());
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

    auto *ctx = new Ctx{scrim, std::move(on_confirm)};

    lv_obj_t *cancel = make_button(card, fonts, "Cancel", 0x4A4A4A);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(cancel, [](lv_event_t *e) {
      auto *c = static_cast<Ctx *>(lv_event_get_user_data(e));
      lv_obj_delete(c->scrim);
      delete c;
    }, LV_EVENT_CLICKED, ctx);

    lv_obj_t *ok = make_button(card, fonts, confirm_text, 0xC62828);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(ok, [](lv_event_t *e) {
      auto *c = static_cast<Ctx *>(lv_event_get_user_data(e));
      if (c->on_confirm) c->on_confirm();
      lv_obj_delete(c->scrim);
      delete c;
    }, LV_EVENT_CLICKED, ctx);
  }
};

}  // namespace tilehaus
