#pragma once
#include "lvgl.h"
#include <string>

#include "card.h"  // CardFonts
#include "splash.h"  // splash_hide
#include "esphome/components/network/util.h"

namespace tilehaus {

// The "Awaiting Configuration" placeholder shown when no (valid) deck config is
// stored. A centered cog icon, a title, and the device's URL so the user knows
// where to point the configurator. An LVGL timer refreshes the IP line once the
// network comes up — this runs on the main loop, so no ESPHome yaml hook needed.

inline lv_obj_t *&awaiting_ip_label() {
  static lv_obj_t *label = nullptr;
  return label;
}

// Refreshes the URL line from the current network address. Cheap no-op when the
// text is unchanged. Safe to call before the network is up (shows a wait line).
inline void awaiting_update() {
  lv_obj_t *label = awaiting_ip_label();
  if (label == nullptr) return;
  std::string ip;
  for (const auto &addr : esphome::network::get_ip_addresses()) {
    if (addr.is_set() && addr.is_ip4()) {
      char buf[esphome::network::IP_ADDRESS_BUFFER_SIZE];
      addr.str_to(buf);
      ip = buf;
      break;
    }
  }
  static std::string shown;
  const std::string text =
      ip.empty() ? std::string("Connecting…") : (std::string("http://") + ip);
  if (text != shown) {
    shown = text;
    lv_label_set_text(label, text.c_str());
  }
  // The boot splash normally hides on the first Home Assistant state, delivered
  // via a card's HA subscription — but the awaiting screen has no cards and no
  // subscriptions, so that never fires. This screen needs only the network, so
  // dismiss the splash once we have an address and reveal the screen underneath.
  static bool splash_dismissed = false;
  if (!ip.empty() && !splash_dismissed) {
    splash_dismissed = true;
    splash_hide();
  }
}

inline void awaiting_build(lv_obj_t *root, const CardFonts &fonts) {
  lv_obj_clean(root);
  lv_obj_set_style_pad_all(root, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *col = lv_obj_create(root);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 14, 0);

  lv_obj_t *icon = lv_label_create(col);
  lv_label_set_text(icon, "\U000F0493");  // mdi-cog
  lv_obj_set_style_text_font(icon, fonts.icon, 0);
  lv_obj_set_style_text_color(icon, lv_color_hex(0x9AA0A6), 0);

  lv_obj_t *title = lv_label_create(col);
  lv_label_set_text(title, "Awaiting Configuration");
  lv_obj_set_style_text_font(title, fonts.medium, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

  lv_obj_t *url = lv_label_create(col);
  lv_obj_set_style_text_font(url, fonts.body, 0);
  lv_obj_set_style_text_color(url, lv_color_hex(0x9AA0A6), 0);
  awaiting_ip_label() = url;

  awaiting_update();  // fill immediately if the network is already up
  lv_timer_create([](lv_timer_t *) { awaiting_update(); }, 1000, nullptr);
}

}  // namespace tilehaus
