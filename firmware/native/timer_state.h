#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

namespace tilehaus {

// Home Assistant timer entities, reduced to what the Timer card draws.
//
// HA pushes a timer's `remaining` only when its state changes, so a running
// timer has to count down on the panel: from `finishes_at` (ISO 8601) against
// the panel's clock. Pure, so the parsing and the countdown are host-tested.

enum class TimerPhase : uint8_t { Idle, Active, Paused, Unknown };

inline TimerPhase timer_phase(const std::string &state) {
  if (state == "idle") return TimerPhase::Idle;
  if (state == "active") return TimerPhase::Active;
  if (state == "paused") return TimerPhase::Paused;
  return TimerPhase::Unknown;  // unavailable, unknown, not yet received
}

// "0:10:00", "12:00:00", or Python's "1 day, 2:03:04" -> seconds; -1 if unparsable.
inline int64_t parse_hms(const std::string &s) {
  long days = 0, h = 0, m = 0;
  double sec = 0;
  const char *p = s.c_str();
  // Python writes "1 day, " and "2 days, ": take the count, then skip past ", ".
  const size_t day = s.find(" day");
  if (day != std::string::npos) {
    if (std::sscanf(p, "%ld", &days) != 1 || days < 0) return -1;
    const size_t comma = s.find(", ", day);
    if (comma == std::string::npos) return -1;
    p += comma + 2;
  }
  if (std::sscanf(p, "%ld:%ld:%lf", &h, &m, &sec) != 3) return -1;
  if (h < 0 || m < 0 || m > 59 || sec < 0 || sec >= 60) return -1;
  return ((static_cast<int64_t>(days) * 24 + h) * 60 + m) * 60 + static_cast<int64_t>(sec);
}

// Days from 1970-01-01 to a civil date (Howard Hinnant's algorithm).
inline int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// "2026-10-23T23:59:59+00:00" (fractional seconds, "Z", or any offset) ->
// Unix seconds; -1 if unparsable.
inline int64_t parse_iso8601(const std::string &s) {
  int y, mo, d, h, mi, n = 0;
  double sec;
  char sep = 0;
  // A 'T' between date and time, or a space: an attribute holding a datetime
  // reaches the panel as Python's str() of it ("2026-10-26 14:10:26.55+00:00").
  if (std::sscanf(s.c_str(), "%4d-%2d-%2d%c%2d:%2d:%lf%n", &y, &mo, &d, &sep, &h, &mi, &sec, &n) != 7 ||
      (sep != 'T' && sep != ' '))
    return -1;
  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec < 0 || sec >= 61) return -1;
  int64_t t = days_from_civil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400 +
              h * 3600 + mi * 60 + static_cast<int64_t>(sec);
  const char *z = s.c_str() + n;
  if (*z == 'Z' || *z == '\0') return t;  // no offset: treat as UTC
  int oh = 0, om = 0;
  const char sign = *z;
  if ((sign != '+' && sign != '-') || std::sscanf(z + 1, "%2d:%2d", &oh, &om) != 2) return -1;
  const int64_t off = oh * 3600 + om * 60;
  return sign == '+' ? t - off : t + off;
}

struct TimerView {
  int64_t remaining_s = -1;  // -1 = unknown
  int64_t duration_s = -1;
  TimerPhase phase = TimerPhase::Unknown;

  // 0..1000 of the duration still left, for the ring; 0 when unknown.
  int ring_permille() const {
    if (remaining_s < 0 || duration_s <= 0) return 0;
    const int64_t v = remaining_s * 1000 / duration_s;
    return static_cast<int>(v < 0 ? 0 : (v > 1000 ? 1000 : v));
  }
};

// What to show now. `now_s` is the panel's Unix time, or -1 before its clock
// has synced, in which case a running timer falls back to HA's last `remaining`.
inline TimerView timer_view(const std::string &state, const std::string &duration,
                            const std::string &remaining, const std::string &finishes_at,
                            int64_t now_s) {
  TimerView v;
  v.phase = timer_phase(state);
  v.duration_s = parse_hms(duration);
  switch (v.phase) {
    case TimerPhase::Idle:
      v.remaining_s = v.duration_s;
      break;
    case TimerPhase::Paused:
      v.remaining_s = parse_hms(remaining);
      break;
    case TimerPhase::Active: {
      const int64_t end = parse_iso8601(finishes_at);
      if (end >= 0 && now_s >= 0) {
        v.remaining_s = end > now_s ? end - now_s : 0;
      } else {
        v.remaining_s = parse_hms(remaining);
      }
      break;
    }
    case TimerPhase::Unknown:
      break;
  }
  return v;
}

// "7:12", "59:59", "1:02:03"; "–" when unknown.
inline std::string format_countdown(int64_t s) {
  if (s < 0) return "–";
  char buf[24];
  const int64_t h = s / 3600, m = (s / 60) % 60, sec = s % 60;
  if (h > 0) std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", static_cast<long long>(h),
                           static_cast<long long>(m), static_cast<long long>(sec));
  else std::snprintf(buf, sizeof(buf), "%lld:%02lld", static_cast<long long>(m), static_cast<long long>(sec));
  return buf;
}

}  // namespace tilehaus
