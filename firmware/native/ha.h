#pragma once
#include <functional>
#include <string>
#include <cstdio>
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/api_pb2.h"

namespace tilehaus {

// One-shot "first HA state has arrived" signal. HA pushes initial states right
// after it connects, so the first time any subscription fires is the moment the
// grid stops showing placeholders — the cue to fade the boot splash out. The
// hook is re-armed on disconnect (ha_arm_first_state) so a reconnect fires it
// again. Runs on the ESPHome main loop, same thread as LVGL — safe to touch UI.
inline std::function<void()> &ha_first_state_hook() {
  static std::function<void()> hook;
  return hook;
}
inline bool &ha_first_state_seen() {
  static bool seen = false;
  return seen;
}
inline void ha_arm_first_state() { ha_first_state_seen() = false; }

// Subscribe to a HA entity's state (attr = nullptr for the state itself).
inline void ha_subscribe(const std::string &entity, const char *attr,
                         std::function<void(const std::string &)> cb) {
  // Explicitly typed so it matches the const std::string& overload of
  // subscribe_home_assistant_state (a raw lambda is ambiguous with the StringRef
  // overload).
  std::function<void(const std::string &)> wrapped =
      [cb = std::move(cb)](const std::string &s) {
        if (!ha_first_state_seen()) {
          ha_first_state_seen() = true;
          if (ha_first_state_hook()) ha_first_state_hook()();
        }
        cb(s);
      };
  esphome::api::global_api_server->subscribe_home_assistant_state(
      entity,
      attr ? esphome::optional<std::string>(std::string(attr))
           : esphome::optional<std::string>(),
      std::move(wrapped));
}

// Call a HA service on an entity (no extra data).
inline void ha_call(const char *service, const std::string &entity) {
  esphome::api::HomeassistantActionRequest req;
  req.service = decltype(req.service)(service);
  req.data.init(1);
  auto &kv = req.data.emplace_back();
  kv.key = decltype(kv.key)("entity_id");
  kv.value = decltype(kv.value)(entity.c_str());
  esphome::api::global_api_server->send_homeassistant_action(req);
}

// Call a HA service with one extra data field.
inline void ha_call_kv(const char *service, const std::string &entity,
                       const char *key, const std::string &value) {
  esphome::api::HomeassistantActionRequest req;
  req.service = decltype(req.service)(service);
  req.data.init(2);
  { auto &kv = req.data.emplace_back();
    kv.key = decltype(kv.key)("entity_id");
    kv.value = decltype(kv.value)(entity.c_str()); }
  { auto &kv = req.data.emplace_back();
    kv.key = decltype(kv.key)(key);
    kv.value = decltype(kv.value)(value.c_str()); }
  esphome::api::global_api_server->send_homeassistant_action(req);
}

// Call light.turn_on with rgb_color (a HA list). A plain string data value
// cannot carry a list, so we send a data_template Jinja expression that builds
// the list from scalar variables — the same approach espcontrol uses.
inline void ha_call_rgb(const std::string &entity, int r, int g, int b) {
  auto clamp = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
  r = clamp(r); g = clamp(g); b = clamp(b);
  esphome::api::HomeassistantActionRequest req;
  req.service = decltype(req.service)("light.turn_on");
  req.data.init(1);
  req.data_template.init(1);
  req.variables.init(3);
  { auto &kv = req.data.emplace_back();
    kv.key = decltype(kv.key)("entity_id");
    kv.value = decltype(kv.value)(entity.c_str()); }
  { auto &kv = req.data_template.emplace_back();
    kv.key = decltype(kv.key)("rgb_color");
    kv.value = decltype(kv.value)("{{ [red | int, green | int, blue | int] }}"); }
  char rb[4], gb[4], bb[4];
  snprintf(rb, sizeof(rb), "%d", r);
  snprintf(gb, sizeof(gb), "%d", g);
  snprintf(bb, sizeof(bb), "%d", b);
  { auto &kv = req.variables.emplace_back();
    kv.key = decltype(kv.key)("red");   kv.value = decltype(kv.value)(rb); }
  { auto &kv = req.variables.emplace_back();
    kv.key = decltype(kv.key)("green"); kv.value = decltype(kv.value)(gb); }
  { auto &kv = req.variables.emplace_back();
    kv.key = decltype(kv.key)("blue");  kv.value = decltype(kv.value)(bb); }
  esphome::api::global_api_server->send_homeassistant_action(req);
}

}  // namespace tilehaus
