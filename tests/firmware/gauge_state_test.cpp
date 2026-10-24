#include <cassert>
#include <cmath>

#include "gauge_state.h"

using namespace tilehaus;

int main() {
  // Unit defaults.
  {
    GaugeScale s = parse_gauge_scale("", "%");
    assert(s.min == 0 && s.max == 100 && s.warn == GaugeScale::Warn::None);
    assert(s.permille(74) == 740);
    GaugeScale co2 = parse_gauge_scale("", "ppm");
    assert(co2.min == 400 && co2.max == 2000 && co2.warns(1200) && !co2.warns(800));
    assert(parse_gauge_scale("", "lux").max == 100);  // unknown unit: 0..100
  }
  // A range, with and without a warning.
  {
    GaugeScale s = parse_gauge_scale("0..100 <20", "%");
    assert(s.warn == GaugeScale::Warn::Below && s.threshold == 20);
    assert(s.warns(19.9f) && !s.warns(20) && !s.warns(80));
    GaugeScale t = parse_gauge_scale("400..2000 >1000", "ppm");
    assert(t.min == 400 && t.max == 2000 && t.warns(1001) && !t.warns(1000));
    GaugeScale n = parse_gauge_scale("-20..40", "°C");
    assert(n.min == -20 && n.max == 40 && n.permille(10) == 500);
    GaugeScale d = parse_gauge_scale(" 0.5..1.5 ", "");
    assert(d.min == 0.5f && d.max == 1.5f && d.permille(1.0f) == 500);
  }
  // A warning alone keeps the unit's range.
  {
    GaugeScale s = parse_gauge_scale("<15", "%");
    assert(s.min == 0 && s.max == 100 && s.warns(10));
  }
  // Typos fall back to the unit default, whole.
  {
    assert(parse_gauge_scale("0-100", "%").max == 100 && parse_gauge_scale("0-100", "%").warn == GaugeScale::Warn::None);
    assert(parse_gauge_scale("100..0", "%").min == 0);       // reversed
    assert(parse_gauge_scale("0..100 <", "%").warn == GaugeScale::Warn::None);
    assert(parse_gauge_scale("0..100 junk", "%").warn == GaugeScale::Warn::None);
    assert(parse_gauge_scale("abc", "ppm").warn == GaugeScale::Warn::Above);  // ppm default kept
  }
  // Clamping and bad values.
  {
    GaugeScale s = parse_gauge_scale("0..100", "%");
    assert(s.permille(-5) == 0 && s.permille(150) == 1000);
    assert(s.permille(NAN) == 0 && !s.warns(NAN));
  }
  return 0;
}
