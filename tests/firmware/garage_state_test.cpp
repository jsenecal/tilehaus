#include <cassert>
#include <string>

#include "garage_state.h"

using namespace tilehaus;

int main() {
  const int64_t kMin = 60000;

  // Watched it open: minutes count from then.
  {
    DoorTracker d;
    d.on_state("closed", 0);
    assert(d.caption(0) == "CLOSED");
    d.on_state("opening", 1000);
    assert(d.caption(1000) == "OPENING");
    d.on_state("open", 5000);
    assert(d.caption(5000) == "OPEN · 0 MIN");
    assert(d.caption(5000 + 14 * kMin) == "OPEN · 14 MIN");
    assert(!d.overdue(5000 + 9 * kMin, 10));
    assert(d.overdue(5000 + 10 * kMin, 10));
    assert(d.caption(5000 + 135 * kMin) == "OPEN · 2 H");
    // A repeated "open" push doesn't restart the count.
    d.on_state("open", 5000 + 20 * kMin);
    assert(d.open_minutes(5000 + 20 * kMin) == 20);
    // Closing ends it.
    d.on_state("closing", 5000 + 21 * kMin);
    assert(d.caption(0) == "CLOSING" && d.open_minutes(5000 + 21 * kMin) == -1);
    assert(!d.overdue(5000 + 99 * kMin, 10));
  }

  // Already open when the panel booted: no minutes, never overdue (unknown start).
  {
    DoorTracker d;
    d.on_state("open", 0);
    assert(d.caption(50 * kMin) == "OPEN");
    assert(!d.overdue(50 * kMin, 10));
  }

  // Unavailable in between also loses the start time.
  {
    DoorTracker d;
    d.on_state("closed", 0);
    d.on_state("unavailable", 1000);
    assert(d.caption(1000) == "");
    d.on_state("open", 2000);
    assert(d.caption(2000 + 5 * kMin) == "OPEN");
  }

  // States HA uses.
  assert(door_phase("closed") == DoorPhase::Closed);
  assert(door_phase("open") == DoorPhase::Open);
  assert(door_phase("opening") == DoorPhase::Opening);
  assert(door_phase("closing") == DoorPhase::Closing);
  assert(door_phase("unknown") == DoorPhase::Unknown);
  return 0;
}
