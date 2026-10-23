#include <cassert>
#include <cstdint>

#include "toggle_state.h"

int main() {
  using tilehaus::toggle_color;
  using tilehaus::next_toggle_on;

  assert(toggle_color(true) == 0xB5E853u);   // built-in on-colour (lime)
  assert(toggle_color(false) == 0x313131u);  // tile chrome neutral off
  assert(next_toggle_on(false) == true);
  assert(next_toggle_on(true) == false);
  return 0;
}
