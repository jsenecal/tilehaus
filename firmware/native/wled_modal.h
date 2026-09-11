#pragma once
#include "lvgl.h"
#include <string>
#include "card.h"
#include "tabbed_modal.h"
#include "power_tab.h"
#include "color_tab.h"
#include "option_list_tab.h"
#include "levels_tab.h"

namespace tilehaus {

// The WLED modal: seven tabs over the shared TabbedModal, reusing the light
// modal's Power / Brightness / Color tab components and adding WLED-specific
// Presets / Effects / Palette (each a scrollable OptionListTab) and Dynamics
// (Speed + Intensity). Companion entities are derived from the light slug.
// Built once (hidden) at boot; shown on tap. PowerTab shares the tile's on/off
// subject (owned by the card).
struct WledModal {
  TabbedModal tabs_;
  std::string entity_;  // light.<base>
  CardFonts fonts_;
  lv_subject_t *power_ = nullptr;
  bool built_ = false;

  PowerTab power_tab_;
  LevelsTab levels_tab_;
  ColorTab color_tab_;
  OptionListTab presets_, effects_, palette_;
  std::string preset_e_, palette_e_, speed_e_, intensity_e_;

  WledModal(const std::string &entity, const std::string &title,
            const CardFonts &fonts, lv_subject_t *power)
      : entity_(entity), fonts_(fonts), power_(power) {
    tabs_.title_ = title;
    tabs_.fonts_ = fonts;
  }

  static std::string derive(const std::string &e, const char *dom,
                            const char *suf) {
    auto dot = e.find('.');
    std::string base = dot == std::string::npos ? e : e.substr(dot + 1);
    return std::string(dom) + "." + base + suf;
  }

  void build() {
    preset_e_ = derive(entity_, "select", "_preset");
    palette_e_ = derive(entity_, "select", "_color_palette");
    speed_e_ = derive(entity_, "number", "_speed");
    intensity_e_ = derive(entity_, "number", "_intensity");

    tabs_.add_tab("\U000F0425", [this](lv_obj_t *p, int, int) {  // power
      power_tab_.build(p, entity_, fonts_, power_);
    });
    tabs_.add_tab("\U000F029A", [this](lv_obj_t *p, int, int) {  // levels
      levels_tab_.build(p, fonts_, entity_, speed_e_, intensity_e_);
    });
    tabs_.add_tab("\U000F06E9", [this](lv_obj_t *p, int, int) {  // color swatches
      color_tab_.build(p, entity_);
    });
    tabs_.add_tab("\U000F0241", [this](lv_obj_t *p, int, int) {  // presets
      presets_.build(p, fonts_, preset_e_, "options", preset_e_, nullptr,
                     "select.select_option", preset_e_, "option");
    });
    tabs_.add_tab("\U000F07DE", [this](lv_obj_t *p, int, int) {  // effects
      effects_.build(p, fonts_, entity_, "effect_list", entity_, "effect",
                     "light.turn_on", entity_, "effect");
    });
    tabs_.add_tab("\U000F03D8", [this](lv_obj_t *p, int, int) {  // palette
      palette_.build(p, fonts_, palette_e_, "options", palette_e_, nullptr,
                     "select.select_option", palette_e_, "option");
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
