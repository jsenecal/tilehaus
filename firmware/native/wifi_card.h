#pragma once
#include "lvgl.h"
#include <algorithm>
#include <string>
#include "card.h"
#include "card_style.h"
#include "guest_wifi.h"
#include "overlay.h"
#include "theme.h"
#include "wifi_qr.h"

namespace tilehaus {

// Guest Wi-Fi as a QR code (type 28). A phone camera pointed at it offers to
// join. The network comes from Home Assistant (guest_wifi.h), never the deck.
// Tap for a full-screen page with a large code and the name and password in
// text, for anyone who would rather type.
struct WifiCard : Card {
  lv_obj_t *cell_ = nullptr;
  lv_obj_t *qr_ = nullptr;
  lv_obj_t *hint_lbl_ = nullptr;
  CardFonts fonts_{};
  Overlay modal_;
  bool modal_built_ = false;
  lv_obj_t *modal_qr_ = nullptr;
  lv_obj_t *ssid_lbl_ = nullptr;
  lv_obj_t *pass_lbl_ = nullptr;

  TileSpan default_size() const override { return {2, 2}; }

  void build(lv_obj_t *cell, const CardConfig &cfg, const CardFonts &fonts) override {
    cell_ = cell;
    fonts_ = fonts;
    const TileMetrics m = tile_metrics();
    const int content_w = m.px_w > 0 ? m.px_w - 2 * kTileInset : 120;
    const int content_h = m.px_h > 0 ? m.px_h - 2 * kTileInset : 120;
    const bool named = !cfg.hide_label && !cfg.title.empty();
    constexpr int kCaptionH = 22, kNameH = 34, kGap = 6;

    lv_obj_t *cap = lv_label_create(cell);
    if (fonts.caption) lv_obj_set_style_text_font(cap, fonts.caption, 0);
    lv_obj_set_style_text_letter_space(cap, 2, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(kTileInkDim), 0);
    lv_label_set_text(cap, "GUEST WI-FI");
    lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 0, 0);

    // The code as large as fits under the caption (and above the name).
    const int size = std::max(48, std::min(content_w, content_h - kCaptionH - kGap -
                                                          (named ? kNameH + kGap : 0)));
    qr_ = lv_qrcode_create(cell);
    lv_qrcode_set_size(qr_, size);
    lv_qrcode_set_dark_color(qr_, lv_color_hex(kBlack));
    lv_qrcode_set_light_color(qr_, lv_color_hex(kTileInk));
    lv_qrcode_set_quiet_zone(qr_, true);  // the white margin scanners need
    lv_obj_align(qr_, LV_ALIGN_TOP_MID, 0, kCaptionH + kGap);

    hint_lbl_ = lv_label_create(cell);
    if (fonts.small) lv_obj_set_style_text_font(hint_lbl_, fonts.small, 0);
    lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(kTileInkDim), 0);
    lv_label_set_long_mode(hint_lbl_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint_lbl_, content_w);
    lv_label_set_text(hint_lbl_, "Set the guest network in Home Assistant");
    lv_obj_align(hint_lbl_, LV_ALIGN_LEFT_MID, 0, 0);

    add_name(cell, fonts, cfg.title, cfg.hide_label);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    style_press_dip(cell);
    paint_cell(cell, resolve_color(cfg.inactive_color, kTileBg));
  }

  void bind(const CardConfig &) override {
    guest_wifi().listeners.push_back([this]() { refresh(); });
    lv_obj_add_event_cb(cell_, [](lv_event_t *e) {
      static_cast<WifiCard *>(lv_event_get_user_data(e))->open();
    }, LV_EVENT_CLICKED, this);
    refresh();
  }

 private:
  void refresh() {
    const GuestWifi &g = guest_wifi();
    const std::string payload = wifi_qr_payload(g.ssid, g.password);
    const bool set = !payload.empty();
    if (set) lv_qrcode_update(qr_, payload.data(), payload.size());
    if (set) lv_obj_remove_flag(qr_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
    if (set) lv_obj_add_flag(hint_lbl_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(hint_lbl_, LV_OBJ_FLAG_HIDDEN);
    if (modal_built_) refresh_modal(payload);
  }

  void open() {
    if (wifi_qr_payload(guest_wifi().ssid, guest_wifi().password).empty()) return;
    if (!modal_built_) build_modal();
    refresh_modal(wifi_qr_payload(guest_wifi().ssid, guest_wifi().password));
    modal_.show();
  }

  static lv_obj_t *caption(lv_obj_t *parent, const CardFonts &f, const char *text) {
    lv_obj_t *l = lv_label_create(parent);
    if (f.caption) lv_obj_set_style_text_font(l, f.caption, 0);
    lv_obj_set_style_text_letter_space(l, 2, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(kInkMuted), 0);
    lv_label_set_text(l, text);
    return l;
  }

  lv_obj_t *value_label(lv_obj_t *parent) {
    lv_obj_t *l = lv_label_create(parent);
    if (fonts_.medium) lv_obj_set_style_text_font(l, fonts_.medium, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(kInk), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
    return l;
  }

  // A large code on the left, the network and password in text on the right.
  void build_modal() {
    modal_built_ = true;
    modal_.build("Guest Wi-Fi", fonts_);
    modal_.on_close_ = [this]() { modal_.hide(); };
    lv_obj_t *body = modal_.content();
    const int w = modal_.content_w(), h = modal_.content_h();
    const int size = std::min(h - 2 * kTileInset, w / 2);
    modal_qr_ = lv_qrcode_create(body);
    lv_qrcode_set_size(modal_qr_, size);
    lv_qrcode_set_dark_color(modal_qr_, lv_color_hex(kBlack));
    lv_qrcode_set_light_color(modal_qr_, lv_color_hex(kTileInk));
    lv_qrcode_set_quiet_zone(modal_qr_, true);
    lv_obj_align(modal_qr_, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *col = lv_obj_create(body);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, w - size - 48, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 10, 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(col, LV_ALIGN_LEFT_MID, size + 48, 0);
    caption(col, fonts_, "NETWORK");
    ssid_lbl_ = value_label(col);
    lv_obj_t *spacer = lv_obj_create(col);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 14);
    caption(col, fonts_, "PASSWORD");
    pass_lbl_ = value_label(col);
    lv_obj_t *spacer2 = lv_obj_create(col);
    lv_obj_remove_style_all(spacer2);
    lv_obj_set_size(spacer2, 1, 24);
    lv_obj_t *how = lv_label_create(col);
    if (fonts_.body) lv_obj_set_style_text_font(how, fonts_.body, 0);
    lv_obj_set_style_text_color(how, lv_color_hex(kInkMuted), 0);
    lv_label_set_long_mode(how, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(how, LV_PCT(100));
    lv_label_set_text(how, "Point your phone's camera at the code to join.");
  }

  void refresh_modal(const std::string &payload) {
    if (!modal_built_ || payload.empty()) return;
    lv_qrcode_update(modal_qr_, payload.data(), payload.size());
    const GuestWifi &g = guest_wifi();
    lv_label_set_text(ssid_lbl_, g.ssid.c_str());
    lv_label_set_text(pass_lbl_, g.password.empty() ? "No password" : g.password.c_str());
  }
};

}  // namespace tilehaus
