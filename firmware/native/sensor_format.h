#pragma once
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace tilehaus {

// True when `unit` butts straight against the number with no separating space —
// the degree family and percent, matching how HA's own frontend renders them.
inline bool unit_hugs_value(const std::string &unit) {
  if (unit == "%") return true;
  // U+00B0 DEGREE SIGN, i.e. "°C" / "°F" / a bare "°".
  return unit.size() >= 2 && static_cast<unsigned char>(unit[0]) == 0xC2 &&
         static_cast<unsigned char>(unit[1]) == 0xB0;
}

// Formats a Home Assistant sensor state for a tile: whole numbers stay bare,
// other numbers get one decimal, and the entity's own unit_of_measurement is
// appended — so a humidity sensor reads "81%" and a VOC index reads "139",
// rather than every sensor being stamped "°C". Non-numeric states pass through
// verbatim (plenty of sensors report text). Em dash (U+2014) when the state is
// missing or unavailable.
inline std::string format_sensor_value(const std::string &state,
                                       const std::string &unit, bool valid) {
  if (!valid) return "\xE2\x80\x94";

  const char *begin = state.c_str();
  char *end = nullptr;
  const double value = std::strtod(begin, &end);
  while (end && *end == ' ') ++end;  // tolerate a trailing space in the state

  std::string text;
  if (end != begin && end && *end == '\0' && std::isfinite(value)) {
    char buf[32];
    // A whole number reads better without a phantom ".0" — a VOC index of 139
    // is not 139.0 — while 20.46 still wants rounding to 20.5.
    std::snprintf(buf, sizeof(buf),
                  value == std::floor(value) ? "%.0f" : "%.1f", value);
    text = buf;
  } else {
    text = state;
  }

  if (!unit.empty()) {
    if (!unit_hugs_value(unit)) text += ' ';
    text += unit;
  }
  return text;
}

// The same value as format_sensor_value, in two parts: the number (or the
// text of a non-numeric state) and the unit, so a tile can set the number large
// and the unit smaller beside it. `numeric` is false for text states, the em
// dash, and anything else the digits-only big face can't draw.
struct SensorParts {
  std::string value;
  std::string unit;
  bool numeric = false;
};

inline bool fit_face_can_draw(const std::string &s) {
  if (s.empty()) return false;
  for (char ch : s) {
    if (!((ch >= '0' && ch <= '9') || ch == '.' || ch == ',' || ch == '-' || ch == '+' || ch == ':'))
      return false;
  }
  return true;
}

inline SensorParts split_sensor_value(const std::string &state, const std::string &unit,
                                      bool valid) {
  SensorParts p;
  if (!valid) {
    p.value = "\xE2\x80\x94";
    return p;
  }
  const char *begin = state.c_str();
  char *end = nullptr;
  const double value = std::strtod(begin, &end);
  while (end && *end == ' ') ++end;
  if (end != begin && end && *end == '\0' && std::isfinite(value)) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), value == std::floor(value) ? "%.0f" : "%.1f", value);
    p.value = buf;
    p.unit = unit;
    p.numeric = fit_face_can_draw(p.value);
    return p;
  }
  p.value = format_sensor_value(state, unit, true);  // text: keep it whole
  return p;
}

}  // namespace tilehaus
