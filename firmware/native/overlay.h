#pragma once
#include "lvgl.h"
#include <functional>
#include <string>
#include "card_style.h"    // kTileInset
#include "surface.h"   // app_grid_root

namespace tilehaus {

struct Overlay;

// The overlay currently showing, or nullptr. Modals are owned by individual
// cards with no registry between them, so this is the only way to ask "is a
// modal open" — which the idle engine needs both to close one and to avoid
// firing while the user is part-way through it.
inline Overlay *&active_overlay() {
  static Overlay *o = nullptr;
  return o;
}

// Full-screen modal chrome on the top layer: opaque background that renders
// above the grid and swallows touches, a back arrow (top-left) + title, and an
// empty content() region the owner fills. Built once, reused via show()/hide().
//
// Lifecycle: built once and retained for the program's lifetime (the POC
// calls poc_build_cards() a single time at boot and never destroys cards
// at runtime). root_ lives on lv_layer_top(); there is intentionally no
// teardown path — do not destroy an Overlay/LightModal while the app runs,
// or its lv_objs would outlive their C++ owner.
struct Overlay {
  lv_obj_t *root_ = nullptr;
  lv_obj_t *content_ = nullptr;
  int content_w_ = 0;
  int content_h_ = 0;
  std::function<void()> on_close_;

  // Editorial chrome: the light page, a square black back button, and the
  // heavy page title with its bar beside it — the same header a page gets. The
  // owner's content sits on the page too; show() re-inks any text the body
  // left white directly on it (see ink_page_text).
  void build(const std::string &title, const CardFonts &fonts) {
    root_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root_, lv_color_hex(kPageBg), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);  // swallow grid touches
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    constexpr int kBack = 60;  // back button side, matched to the title's cap height
    lv_obj_t *back = lv_button_create(root_);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, kBack, kBack);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, kTileInset, kTileInset);
    lv_obj_set_style_bg_color(back, lv_color_hex(kInk), 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
    style_press_dip(back);
    lv_obj_t *bl = lv_label_create(back);
    if (fonts.icon) lv_obj_set_style_text_font(bl, fonts.icon, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(bl, "\U000F004D");  // mdi-arrow-left
    lv_obj_set_style_transform_pivot_x(bl, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(bl, lv_pct(50), 0);
    lv_obj_set_style_transform_scale(bl, 200, 0);  // ~36px arrow in the 60px box
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, [](lv_event_t *e) {
      auto *self = static_cast<Overlay *>(lv_event_get_user_data(e));
      if (self->on_close_) self->on_close_();
    }, LV_EVENT_CLICKED, this);

    // Title top sits a few px above the button so the cap height and the
    // button share a top edge (the font's line box carries internal leading).
    const lv_font_t *tfont = fonts.title ? fonts.title : fonts.body;
    const int title_y = kTileInset - 6;
    lv_obj_t *tt = add_page_title(root_, tfont, title.c_str());
    lv_obj_align(tt, LV_ALIGN_TOP_LEFT, kTileInset + kBack + 20, title_y);

    int w = lv_display_get_horizontal_resolution(lv_display_get_default());
    int h = lv_display_get_vertical_resolution(lv_display_get_default());
    // The body is cut right under the title's bar, and a hairline rule carries
    // the bar's bottom edge across the full width, so lists read as sliding
    // under a ruled header. Both are drawn once — nothing repaints behind the
    // header while a list scrolls. Computed from the font's line height
    // (add_page_title stacks: line, 4px gap, bar) since the overlay is built
    // hidden, before any layout pass could measure it.
    const int bar_bottom =
        title_y + (tfont ? lv_font_get_line_height(tfont) : 56) + 4 + kTitleBarH;
    constexpr int kRuleH = 2;
    lv_obj_t *rule = lv_obj_create(root_);
    lv_obj_remove_style_all(rule);
    lv_obj_clear_flag(rule, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(rule, w - 2 * kTileInset, kRuleH);
    lv_obj_set_style_bg_color(rule, lv_color_hex(kInk), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_align(rule, LV_ALIGN_TOP_LEFT, kTileInset, bar_bottom - kRuleH);
    const int top = bar_bottom;
    content_w_ = w - 2 * kTileInset;
    content_h_ = h - top - kTileInset;
    content_ = lv_obj_create(root_);
    lv_obj_remove_style_all(content_);
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(content_, content_w_, content_h_);
    lv_obj_align(content_, LV_ALIGN_TOP_LEFT, kTileInset, top);
  }

  lv_obj_t *content() { return content_; }
  int content_w() const { return content_w_; }
  int content_h() const { return content_h_; }

  // Instant show/hide. The overlay background is opaque, so simply raising it
  // covers the grid and hiding it reveals the grid again — no animation. A
  // full-screen opacity fade here forced LVGL to render the whole 1024x600
  // screen to a layer and alpha-blend it every frame, which dropped the panel to
  // ~8fps during modal transitions. Covered grid cells aren't redrawn, so this
  // is both instant and cheap. (A cheap slide could be added later if desired.)
  void show() {
    if (!root_) return;
    // Re-ink on every show: bodies set white text as state changes, and this
    // is cheaper than teaching each modal about the light page.
    ink_page_text(content_, true);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root_);
    active_overlay() = this;
    // Hide the grid beneath. LVGL never treats the top layer as covering the
    // screen: every redraw paints the active screen first, then the top layer
    // over it, so with the grid shown each frame of a modal scroll also
    // repainted every tile hidden behind the modal (~35ms of a ~53ms frame).
    if (app_grid_root()) lv_obj_add_flag(app_grid_root(), LV_OBJ_FLAG_HIDDEN);
  }

  void hide() {
    if (!root_) return;
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    // Only clear it if we are the one showing — a modal hiding itself after
    // another has already opened must not blank the pointer.
    if (active_overlay() == this) active_overlay() = nullptr;
    if (!active_overlay() && app_grid_root())
      lv_obj_clear_flag(app_grid_root(), LV_OBJ_FLAG_HIDDEN);
  }
};

}  // namespace tilehaus
