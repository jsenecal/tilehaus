#include <cassert>
#include <cstdint>
#include <string>

#include "slider_map.h"

int main() {
  using tilehaus::brightness_to_slider;
  using tilehaus::slider_to_brightness;
  using tilehaus::format_brightness_pct;

  // HA brightness is 0..255; slider is 0..100.
  assert(brightness_to_slider(0) == 0);
  assert(brightness_to_slider(255) == 100);
  assert(brightness_to_slider(128) == 50);
  assert(slider_to_brightness(0) == 0);
  assert(slider_to_brightness(100) == 255);
  assert(slider_to_brightness(50) == 128);

  // Round-trip must be stable (no drift) for slider values.
  for (int p = 0; p <= 100; ++p)
    assert(brightness_to_slider(slider_to_brightness(p)) == p);

  assert(format_brightness_pct(0) == "0%");
  assert(format_brightness_pct(50) == "50%");
  assert(format_brightness_pct(100) == "100%");
  return 0;
}
