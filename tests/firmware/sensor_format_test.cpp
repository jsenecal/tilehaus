#include <cassert>
#include <string>

#include "sensor_format.h"

int main() {
  using tilehaus::format_sensor_value;

  const std::string kDegC = "\xC2\xB0""C";
  const std::string kEmDash = "\xE2\x80\x94";

  // Temperature keeps one decimal and hugs the degree sign.
  assert(format_sensor_value("21.4", kDegC, true) == "21.4" + kDegC);
  assert(format_sensor_value("20.46", kDegC, true) == "20.5" + kDegC);
  assert(format_sensor_value("-3.25", kDegC, true) == "-3.2" + kDegC);

  // Whole numbers lose the phantom ".0" — the old formatter printed "0.0".
  assert(format_sensor_value("0", kDegC, true) == "0" + kDegC);
  assert(format_sensor_value("81", "%", true) == "81%");
  assert(format_sensor_value("139", "", true) == "139");

  // Other units are separated by a space.
  assert(format_sensor_value("0", "\xC2\xB5g/m\xC2\xB3", true) == "0 \xC2\xB5g/m\xC2\xB3");
  assert(format_sensor_value("1013.2", "hPa", true) == "1013.2 hPa");

  // A sensor whose state is not a number passes through verbatim.
  assert(format_sensor_value("asleep", "", true) == "asleep");
  assert(format_sensor_value("12 minutes", "", true) == "12 minutes");

  // Invalid beats everything, unit included.
  assert(format_sensor_value("21.4", kDegC, false) == kEmDash);
  assert(format_sensor_value("", "", false) == kEmDash);

  // A trailing space in the state still reads as numeric.
  assert(format_sensor_value("21.4 ", kDegC, true) == "21.4" + kDegC);

  // Unit spacing rule itself.
  using tilehaus::unit_hugs_value;
  assert(unit_hugs_value("%"));
  assert(unit_hugs_value(kDegC));
  assert(unit_hugs_value("\xC2\xB0"));
  assert(!unit_hugs_value("W"));
  assert(!unit_hugs_value(""));
  return 0;
}
