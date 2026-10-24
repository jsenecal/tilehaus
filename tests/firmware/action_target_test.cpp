#include <cassert>
#include <cstring>

#include "action_target.h"

int main() {
  using tilehaus::action_service;
  using tilehaus::action_tracks_running;

  assert(std::strcmp(action_service("script.movie_night"), "script.turn_on") == 0);
  assert(std::strcmp(action_service("automation.porch_lights"), "automation.trigger") == 0);
  assert(std::strcmp(action_service("scene.evening"), "scene.turn_on") == 0);
  assert(std::strcmp(action_service("button.restart_router"), "button.press") == 0);
  assert(std::strcmp(action_service("input_button.doorbell"), "input_button.press") == 0);
  assert(std::strcmp(action_service("light.desk"), "homeassistant.turn_on") == 0);
  // Only the domain decides: a name containing another domain doesn't confuse it.
  assert(std::strcmp(action_service("switch.script_runner"), "homeassistant.turn_on") == 0);
  // No entity: fire the event instead.
  assert(action_service("") == nullptr);
  assert(std::strncmp(tilehaus::kActionEvent, "esphome.", 8) == 0);

  assert(action_tracks_running("script.movie_night"));
  assert(!action_tracks_running("automation.porch_lights"));
  assert(!action_tracks_running("switch.script_runner"));
  assert(!action_tracks_running(""));
  return 0;
}
