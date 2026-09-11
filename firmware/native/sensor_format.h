#pragma once
#include <string>
#include <cmath>
#include <cstdio>

namespace tilehaus {

// One decimal place, U+00B0 C suffix; em dash (U+2014) when the value is invalid.
inline std::string format_temperature(float value, bool valid) {
  if (!valid) return "\xE2\x80\x94";
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", value);
  return std::string(buf);
}

}  // namespace tilehaus
