#include <cassert>
#include <string>

#include "timer_state.h"

using namespace tilehaus;

int main() {
  // Durations as HA formats them.
  assert(parse_hms("0:10:00") == 600);
  assert(parse_hms("12:00:00") == 43200);
  assert(parse_hms("0:00:05.5") == 5);
  assert(parse_hms("1 day, 2:03:04") == 93784);
  assert(parse_hms("2 days, 0:00:00") == 172800);
  assert(parse_hms("") == -1);
  assert(parse_hms("ten minutes") == -1);
  assert(parse_hms("0:61:00") == -1);

  // Timestamps, with each offset form.
  assert(parse_iso8601("1970-01-01T00:00:00+00:00") == 0);
  assert(parse_iso8601("2026-10-23T23:59:59+00:00") == 1792799999);
  assert(parse_iso8601("2026-10-23T23:59:59Z") == 1792799999);
  assert(parse_iso8601("2026-10-23T19:59:59-04:00") == 1792799999);
  assert(parse_iso8601("2026-10-24T01:29:59.123456+01:30") == 1792799999);
  assert(parse_iso8601("2024-02-29T12:00:00+00:00") == 1709208000);  // leap day
  assert(parse_iso8601("") == -1);
  assert(parse_iso8601("yesterday") == -1);
  assert(parse_iso8601("2026-13-01T00:00:00+00:00") == -1);

  // Idle shows the full duration; the ring is full.
  {
    TimerView v = timer_view("idle", "0:10:00", "0:10:00", "", 1000);
    assert(v.phase == TimerPhase::Idle && v.remaining_s == 600 && v.ring_permille() == 1000);
  }
  // Active counts down from finishes_at against the panel clock.
  {
    const int64_t end = parse_iso8601("2026-10-23T23:59:59+00:00");
    TimerView v = timer_view("active", "0:10:00", "0:10:00", "2026-10-23T23:59:59+00:00", end - 432);
    assert(v.remaining_s == 432 && v.ring_permille() == 720);
    assert(format_countdown(v.remaining_s) == "7:12");
    // Past the end: zero, not negative.
    assert(timer_view("active", "0:10:00", "", "2026-10-23T23:59:59+00:00", end + 5).remaining_s == 0);
  }
  // Active before the panel clock has synced: HA's last remaining.
  assert(timer_view("active", "0:10:00", "0:04:00", "2026-10-23T23:59:59+00:00", -1).remaining_s == 240);
  // Paused holds HA's remaining.
  assert(timer_view("paused", "0:10:00", "0:03:30", "", 1000).remaining_s == 210);
  // Unavailable: nothing to show.
  {
    TimerView v = timer_view("unavailable", "", "", "", 1000);
    assert(v.phase == TimerPhase::Unknown && v.remaining_s == -1 && v.ring_permille() == 0);
    assert(format_countdown(v.remaining_s) == "–");
  }

  assert(format_countdown(0) == "0:00");
  assert(format_countdown(3599) == "59:59");
  assert(format_countdown(3723) == "1:02:03");
  return 0;
}
