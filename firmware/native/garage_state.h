#pragma once
#include <cstdint>
#include <string>

namespace tilehaus {

// A garage door or gate (a `cover` entity), reduced to what its tile shows:
// the caption ("OPEN · 14 MIN", "CLOSING") and whether it has been open long
// enough to warn. Pure, so the state handling and the clock are host-tested.

enum class DoorPhase : uint8_t { Closed, Open, Opening, Closing, Unknown };

inline DoorPhase door_phase(const std::string &state) {
  if (state == "closed") return DoorPhase::Closed;
  if (state == "open") return DoorPhase::Open;
  if (state == "opening") return DoorPhase::Opening;
  if (state == "closing") return DoorPhase::Closing;
  return DoorPhase::Unknown;  // unavailable, unknown, not yet received
}

// How long it has been open is measured on the panel, from the moment it saw
// the door open. A door already open when the panel booted has an unknown
// start, so its caption leaves the minutes out rather than guess.
struct DoorTracker {
  DoorPhase phase = DoorPhase::Unknown;
  int64_t opened_at_ms = -1;  // -1 = not seen opening

  void on_state(const std::string &state, int64_t now_ms) {
    const DoorPhase next = door_phase(state);
    if (next == DoorPhase::Open && phase != DoorPhase::Open) {
      // Only a transition the panel watched gives a real start time.
      opened_at_ms = phase == DoorPhase::Unknown ? -1 : now_ms;
    } else if (next != DoorPhase::Open) {
      opened_at_ms = -1;
    }
    phase = next;
  }

  int64_t open_minutes(int64_t now_ms) const {
    if (phase != DoorPhase::Open || opened_at_ms < 0 || now_ms < opened_at_ms) return -1;
    return (now_ms - opened_at_ms) / 60000;
  }

  bool overdue(int64_t now_ms, int threshold_min) const {
    const int64_t m = open_minutes(now_ms);
    return m >= 0 && m >= threshold_min;
  }

  std::string caption(int64_t now_ms) const {
    switch (phase) {
      case DoorPhase::Closed: return "CLOSED";
      case DoorPhase::Opening: return "OPENING";
      case DoorPhase::Closing: return "CLOSING";
      case DoorPhase::Unknown: return "";
      case DoorPhase::Open: break;
    }
    const int64_t m = open_minutes(now_ms);
    if (m < 0) return "OPEN";
    if (m < 60) return "OPEN · " + std::to_string(m) + " MIN";
    return "OPEN · " + std::to_string(m / 60) + " H";
  }
};

}  // namespace tilehaus
