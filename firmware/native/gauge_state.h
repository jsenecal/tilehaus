#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace tilehaus {

// The Gauge card's scale: a range and an optional warning threshold, read from
// one short text field ("0..100 <20", "400..2000 >1000"), or guessed from the
// sensor's unit when the field is blank. Pure, so parsing and the dial maths
// are host-tested.

struct GaugeScale {
  float min = 0, max = 100;
  enum class Warn { None, Below, Above } warn = Warn::None;
  float threshold = 0;

  // 0..1000 along the dial, clamped; NaN reads as empty.
  int permille(float v) const {
    if (std::isnan(v) || max <= min) return 0;
    const float t = (v - min) / (max - min);
    return static_cast<int>(std::lround(1000.0f * (t < 0 ? 0 : (t > 1 ? 1 : t))));
  }

  bool warns(float v) const {
    if (std::isnan(v)) return false;
    if (warn == Warn::Below) return v < threshold;
    if (warn == Warn::Above) return v > threshold;
    return false;
  }
};

// A sensible scale for a unit, so most gauges need no configuration.
inline GaugeScale gauge_default_scale(const std::string &unit) {
  GaugeScale s;
  if (unit == "%") { s.min = 0; s.max = 100; }
  else if (unit == "°C") { s.min = 0; s.max = 40; }
  else if (unit == "°F") { s.min = 32; s.max = 104; }
  else if (unit == "ppm") { s.min = 400; s.max = 2000; s.warn = GaugeScale::Warn::Above; s.threshold = 1000; }
  else if (unit == "W") { s.min = 0; s.max = 3000; }
  else if (unit == "kW") { s.min = 0; s.max = 10; }
  return s;
}

// "min..max", optionally followed by "<n" or ">n". Missing parts keep the
// unit's default; anything unreadable leaves the whole default in place, so a
// typo shows a working gauge rather than an empty one.
inline GaugeScale parse_gauge_scale(const std::string &spec, const std::string &unit) {
  const GaugeScale fallback = gauge_default_scale(unit);
  GaugeScale s = fallback;
  const char *p = spec.c_str();
  while (*p == ' ') ++p;
  if (*p == '\0') return s;

  char *end = nullptr;
  if (*p != '<' && *p != '>') {
    // Split on ".." first: strtof would read "0." of "0..100" as a number.
    const char *dots = std::strstr(p, "..");
    if (dots == nullptr || dots == p) return fallback;
    const std::string lo_text(p, dots);
    const float lo = std::strtof(lo_text.c_str(), &end);
    if (end != lo_text.c_str() + lo_text.size()) return fallback;
    p = dots + 2;
    const float hi = std::strtof(p, &end);
    if (end == p || !(hi > lo)) return fallback;
    s.min = lo;
    s.max = hi;
    p = end;
    while (*p == ' ') ++p;
  }
  if (*p == '<' || *p == '>') {
    const char op = *p++;
    const float t = std::strtof(p, &end);
    if (end == p) return fallback;
    s.warn = op == '<' ? GaugeScale::Warn::Below : GaugeScale::Warn::Above;
    s.threshold = t;
    p = end;
    while (*p == ' ') ++p;
  }
  return *p == '\0' ? s : fallback;
}

}  // namespace tilehaus
