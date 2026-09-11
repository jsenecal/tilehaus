#pragma once
#include <cstdint>
#include <string>

namespace tilehaus {

// Round-half-to-nearest conversions between HA brightness (0..255) and
// slider percent (0..100). Designed so brightness_to_slider(slider_to_brightness(p)) == p.
inline int brightness_to_slider(uint8_t brightness) {
  return (static_cast<int>(brightness) * 100 + 127) / 255;
}

inline uint8_t slider_to_brightness(int slider) {
  if (slider <= 0) return 0;
  if (slider >= 100) return 255;
  return static_cast<uint8_t>((slider * 255 + 50) / 100);
}

inline std::string format_brightness_pct(int slider) {
  return std::to_string(slider) + "%";
}

}  // namespace tilehaus
