#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <vector>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <utility>
#include <string>
#include <cstdio>
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/api_pb2.h"

namespace tilehaus {

// An HA state/attribute string as a float, or `dflt` when it is empty or not
// a number (atof yields 0 for junk; NaN is caught explicitly).
inline float ha_float_or(const std::string &s, float dflt) {
  if (s.empty()) return dflt;
  const float v = static_cast<float>(std::atof(s.c_str()));
  return std::isnan(v) ? dflt : v;
}

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

// Home Assistant state, fanned out to the panel's listeners.
//
// ESPHome sends HA its list of subscriptions once, when HA connects, and has
// no way to drop one. So each entity/attribute pair is subscribed with ESPHome
// once, and its updates go to a list of listeners kept here, along with the
// last value. A listener is tagged with the card that registered it (see
// ha_owner), so a card can be torn down and rebuilt while the panel runs
// (a cosmetic deck change, card_host.h): its listeners are dropped, and the new
// card's get the current value at once instead of waiting for the next change.
//
// A pair first subscribed after HA has connected gets no updates until the
// next connect. Rebuilt cards only ever ask for pairs already subscribed.
struct HaListener {
  const void *owner;
  std::function<void(const std::string &)> cb;
};
struct HaFeed {
  std::string value;
  bool has_value = false;
  std::vector<HaListener> listeners;
};

inline std::map<std::string, std::unique_ptr<HaFeed>> &ha_feeds() {
  static std::map<std::string, std::unique_ptr<HaFeed>> feeds;
  return feeds;
}

// The card registering listeners right now; nullptr = permanent (the panel's
// own subscriptions). Set by card_host.h around a card's build and bind.
inline const void *&ha_owner() {
  static const void *owner = nullptr;
  return owner;
}

// Current values for listeners registered while ha_owner() is set: held until
// ha_flush_replays(), so no callback runs on a card that is half built.
inline std::vector<std::function<void()>> &ha_pending_replays() {
  static std::vector<std::function<void()>> v;
  return v;
}

inline void ha_flush_replays() {
  std::vector<std::function<void()>> run = std::move(ha_pending_replays());
  ha_pending_replays().clear();
  for (auto &f : run) f();
}

// Drop every listener `owner` registered.
inline void ha_release(const void *owner) {
  if (owner == nullptr) return;
  for (auto &kv : ha_feeds()) {
    auto &ls = kv.second->listeners;
    ls.erase(std::remove_if(ls.begin(), ls.end(),
                            [owner](const HaListener &l) { return l.owner == owner; }),
             ls.end());
  }
}

// Subscribe to a HA entity's state (attr = nullptr for the state itself).
inline void ha_subscribe(const std::string &entity, const char *attr,
                         std::function<void(const std::string &)> cb) {
  std::string key = entity;
  key += '\x1f';
  if (attr) key += attr;
  std::unique_ptr<HaFeed> &slot = ha_feeds()[key];
  if (!slot) {
    slot.reset(new HaFeed());
    HaFeed *feed = slot.get();  // owned by the map for the program's lifetime
    // Explicitly typed so it matches the const std::string& overload of
    // subscribe_home_assistant_state (a raw lambda is ambiguous with the
    // StringRef overload).
    std::function<void(const std::string &)> fan_out = [feed](const std::string &s) {
      if (!ha_first_state_seen()) {
        ha_first_state_seen() = true;
        if (ha_first_state_hook()) ha_first_state_hook()();
      }
      feed->value = s;
      feed->has_value = true;
      // By index, on a copy: a callback may add a listener to this same feed.
      for (size_t i = 0; i < feed->listeners.size(); ++i) {
        auto f = feed->listeners[i].cb;
        f(s);
      }
    };
    esphome::api::global_api_server->subscribe_home_assistant_state(
        entity,
        attr ? esphome::optional<std::string>(std::string(attr))
             : esphome::optional<std::string>(),
        std::move(fan_out));
  }
  HaFeed *feed = slot.get();
  feed->listeners.push_back(HaListener{ha_owner(), cb});
  if (feed->has_value) {
    if (ha_owner()) ha_pending_replays().push_back([cb, v = feed->value]() { cb(v); });
    else cb(feed->value);
  }
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

// Fire a Home Assistant event (the name must start with "esphome.") with
// string data. HA automations trigger on it with an `event` trigger.
inline void ha_fire_event(const char *event,
                          std::initializer_list<std::pair<const char *, std::string>> data) {
  esphome::api::HomeassistantActionRequest req;
  req.service = decltype(req.service)(event);
  req.is_event = true;
  req.data.init(data.size());
  for (const auto &d : data) {
    auto &kv = req.data.emplace_back();
    kv.key = decltype(kv.key)(d.first);
    kv.value = decltype(kv.value)(d.second.c_str());
  }
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
// the list from scalar variables.
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
