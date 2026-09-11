#pragma once
#include "lvgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "card.h"          // CardFonts
#include "card_style.h"    // kTileInset
#include "overlay.h"
#include "climate_arc.h"
#include "climate_dropdown.h"

namespace tilehaus {

// The thermostat modal, laid out to match HA's climate more-info card: a large
// arc dial with action / target / current stacked in its center and round -/+
// steppers below, over Mode + Preset dropdown buttons at the bottom. Built once
// (hidden) at boot so its subscriptions register before HA's initial-state push,
// then shown on tap.
struct ClimateModal {
  Overlay overlay_;
  std::string entity_, title_;
  CardFonts fonts_;
  bool built_ = false;
  ClimateArc arc_;
  ClimateDropdown mode_;
  ClimateDropdown preset_;

  ClimateModal(const std::string &entity, const std::string &title,
               const CardFonts &fonts)
      : entity_(entity), title_(title), fonts_(fonts) {}

  lv_obj_t *step_btn(lv_obj_t *parent, const char *sym, int dx) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 64, 64);
    lv_obj_set_style_radius(b, 32, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0x808080), 0);
    lv_obj_align(b, LV_ALIGN_CENTER, dx, 120);
    lv_obj_t *l = lv_label_create(b);
    if (fonts_.medium) lv_obj_set_style_text_font(l, fonts_.medium, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(l, sym);
    lv_obj_center(l);
    return b;
  }

  void build() {
    overlay_.build(title_, fonts_.icon, fonts_.body);
    overlay_.on_close_ = [this]() { hide(); };
    lv_obj_t *c = overlay_.content();
    int cw = overlay_.content_w();
    int ch = overlay_.content_h();

    int bot_h = 116;
    int arc_area_h = ch - bot_h;

    // Arc area fills everything above the dropdowns: dial + center labels
    // (action / target / current) + -/+ steppers.
    lv_obj_t *top = lv_obj_create(c);
    lv_obj_remove_style_all(top);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(top, cw, arc_area_h);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);

    int arc_size = std::min(cw, arc_area_h) - 12;
    arc_.build(top, entity_, fonts_, arc_size);

    lv_obj_t *minus = step_btn(top, "-", -40);
    lv_obj_t *plus = step_btn(top, "+", 40);
    lv_obj_add_event_cb(minus, [](lv_event_t *e) {
      static_cast<ClimateModal *>(lv_event_get_user_data(e))->arc_.step(-1);
    }, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(plus, [](lv_event_t *e) {
      static_cast<ClimateModal *>(lv_event_get_user_data(e))->arc_.step(1);
    }, LV_EVENT_CLICKED, this);

    // Bottom: Mode + Preset dropdown buttons.
    lv_obj_t *bot = lv_obj_create(c);
    lv_obj_remove_style_all(bot);
    lv_obj_set_style_bg_opa(bot, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(bot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bot, cw, bot_h);
    lv_obj_align(bot, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(bot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bot, 24, 0);

    int dw = std::min((cw - 2 * kTileInset - 24) / 2, 320);
    // Icons parallel the values: off=power/heat=fire; home/away=account/boost=bolt.
    mode_.build(bot, fonts_, {"\U000F0425", "\U000F0238"}, "Mode", entity_,
                "climate.set_hvac_mode", "hvac_mode", nullptr,
                {"Off", "Heat"}, {"off", "heat"}, dw);
    preset_.build(bot, fonts_, {"\U000F02DC", "\U000F0004", "\U000F140B"},
                  "Preset", entity_, "climate.set_preset_mode", "preset_mode",
                  "preset_mode", {"Home", "Away", "Boost"},
                  {"home", "away", "boost"}, dw);

    arc_.bind();
    mode_.bind();
    preset_.bind();
    built_ = true;
  }

  void show() {
    if (!built_) build();
    overlay_.show();
  }
  void hide() { overlay_.hide(); }
};

}  // namespace tilehaus
