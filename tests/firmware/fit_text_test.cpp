#include <cassert>
#include <string>

#include "fit_text.h"
#include "sensor_format.h"

using namespace tilehaus;

int main() {
  // Big face (180 tall, "23.4" 300 wide) plus regular 55 / 34 / 22.
  const FitCandidate faces[] = {
      {300, 180, true}, {92, 55, false}, {57, 34, false}, {37, 22, false}};

  // Room for it: the big face scaled to the narrower limit.
  {
    FitChoice f = choose_fit(faces, 4, 150, 200, 60);
    assert(f.index == 0 && f.scale == 128 && f.height == 90);  // width-bound
    FitChoice g = choose_fit(faces, 4, 600, 90, 60);
    assert(g.index == 0 && g.scale == 128 && g.height == 90);  // height-bound
    FitChoice h = choose_fit(faces, 4, 900, 900, 60);
    assert(h.index == 0 && h.scale == 256);  // never upscaled
  }
  // Too tight for a crisp big face: the largest regular face that fits.
  {
    FitChoice f = choose_fit(faces, 4, 100, 60, 60);  // big would be 60 * 100/300... no
    assert(f.index == 1 && f.scale == 256 && f.height == 55);
    FitChoice g = choose_fit(faces, 4, 60, 40, 60);
    assert(g.index == 2 && g.height == 34);
  }
  // Nothing fits: the smallest face rather than nothing.
  {
    FitChoice f = choose_fit(faces, 4, 10, 10, 60);
    assert(f.index == 3);
  }
  // Without a big face (a text state), only the regular faces.
  {
    FitChoice f = choose_fit(faces + 1, 3, 200, 200, 60);
    assert(f.index == 0 && f.height == 55);
  }

  // The unit about 42% of the number's height.
  {
    const int units[] = {34, 22, 17};
    assert(choose_unit_face(units, 3, 120) == 0);  // 34 <= 50.4
    assert(choose_unit_face(units, 3, 60) == 1);   // 22 <= 25.2
    assert(choose_unit_face(units, 3, 30) == 2);   // nothing that small: the smallest
  }

  // Splitting a sensor value into number and unit.
  {
    SensorParts p = split_sensor_value("23.46", "°C", true);
    assert(p.value == "23.5" && p.unit == "°C" && p.numeric);
    SensorParts q = split_sensor_value("139", "", true);
    assert(q.value == "139" && q.unit.empty() && q.numeric);
    SensorParts r = split_sensor_value("-4.0", "kW", true);
    assert(r.value == "-4" && r.unit == "kW" && r.numeric);
    SensorParts t = split_sensor_value("Heating", "", true);
    assert(t.value == "Heating" && !t.numeric);
    SensorParts u = split_sensor_value("", "%", false);
    assert(u.value == "—" && !u.numeric && u.unit.empty());
    SensorParts big = split_sensor_value("1e300", "", true);  // prints as a long digit string
    assert(big.numeric);
  }
  assert(fit_face_can_draw("12:05") && fit_face_can_draw("-3.5") && !fit_face_can_draw("3e5") &&
         !fit_face_can_draw(""));
  return 0;
}
