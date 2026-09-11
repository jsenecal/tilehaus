#pragma once
#include <string>

namespace tilehaus {

// Map slider percent (0..100) to Kelvin across [min_k, max_k] and back, round
// half to nearest so the endpoints and midpoints round-trip. constexpr so the
// static_asserts below verify it at compile time (the POC has no test harness).
constexpr int slider_to_kelvin(int slider, int min_k, int max_k) {
  if (slider < 0) slider = 0;
  if (slider > 100) slider = 100;
  return min_k + (slider * (max_k - min_k) + 50) / 100;
}

constexpr int kelvin_to_slider(int kelvin, int min_k, int max_k) {
  if (max_k <= min_k) return 0;
  if (kelvin < min_k) kelvin = min_k;
  if (kelvin > max_k) kelvin = max_k;
  return ((kelvin - min_k) * 100 + (max_k - min_k) / 2) / (max_k - min_k);
}

inline std::string format_kelvin(int kelvin) {
  return std::to_string(kelvin) + " K";
}

// Endpoints and a midpoint round-trip; clamping holds.
static_assert(slider_to_kelvin(0, 2202, 4000) == 2202, "min endpoint");
static_assert(slider_to_kelvin(100, 2202, 4000) == 4000, "max endpoint");
static_assert(kelvin_to_slider(2202, 2202, 4000) == 0, "min -> 0");
static_assert(kelvin_to_slider(4000, 2202, 4000) == 100, "max -> 100");
static_assert(kelvin_to_slider(slider_to_kelvin(50, 2202, 4000), 2202, 4000) == 50,
              "midpoint round-trip");
static_assert(slider_to_kelvin(-10, 2202, 4000) == 2202, "clamp low");
static_assert(kelvin_to_slider(9999, 2202, 4000) == 100, "clamp high");

}  // namespace tilehaus
