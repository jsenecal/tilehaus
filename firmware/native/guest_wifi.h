#pragma once
#include <functional>
#include <string>
#include <vector>

namespace tilehaus {

// The guest network a Wi-Fi share tile shows, set from Home Assistant (the
// "Guest Wi-Fi name" / "Guest Wi-Fi password" text entities in
// panel_entities.yaml) rather than stored in the deck: the deck is readable by
// anyone on the network with a GET. Tiles register to redraw on change, so a
// new password shows without a reboot.
struct GuestWifi {
  std::string ssid;
  std::string password;
  std::vector<std::function<void()>> listeners;

  void changed() {
    for (auto &l : listeners) l();
  }
};

inline GuestWifi &guest_wifi() {
  static GuestWifi g;
  return g;
}

}  // namespace tilehaus
