#include <cassert>
#include <string>

#include "sensor_format.h"

int main() {
  using tilehaus::format_temperature;

  assert(format_temperature(21.4f, true) == "21.4\xC2\xB0""C");
  assert(format_temperature(0.0f, true) == "0.0\xC2\xB0""C");
  assert(format_temperature(-3.25f, true) == "-3.2\xC2\xB0""C");
  assert(format_temperature(21.4f, false) == "\xE2\x80\x94");  // em dash when invalid
  return 0;
}
