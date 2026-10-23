#pragma once
#include <cstring>
#include <string>

namespace tilehaus {

// What the Action card (type 23) calls for its entity, by domain. Returns null
// for an empty entity: the card then fires kActionEvent instead, so a tile can
// trigger a Home Assistant automation with no helper entity at all.
//
// Pure, so the mapping is host-tested.

// ESPHome may only fire HA events whose name starts with "esphome.".
inline constexpr char kActionEvent[] = "esphome.tilehaus_action";

inline const char *action_service(const std::string &entity) {
  if (entity.empty()) return nullptr;
  const std::string domain = entity.substr(0, entity.find('.'));
  if (domain == "script") return "script.turn_on";
  if (domain == "automation") return "automation.trigger";
  if (domain == "scene") return "scene.turn_on";
  if (domain == "button") return "button.press";
  if (domain == "input_button") return "input_button.press";
  return "homeassistant.turn_on";  // anything else that can be turned on
}

// Scripts report "on" while running, so their tile can tint until they finish.
// No other domain the card calls has a running state worth showing.
inline bool action_tracks_running(const std::string &entity) {
  return entity.rfind("script.", 0) == 0;
}

}  // namespace tilehaus
