#include <cassert>
#include <string>

#include "forecast_helpers.h"

int main() {
  using tilehaus::forecast_helper_id;
  using tilehaus::forecast_low_from_high;
  using tilehaus::resolve_forecast_helpers;

  // Derivation from the weather entity — the no-configuration path both the
  // Weather tile and the Header take.
  assert(forecast_helper_id("weather.forecast_home", "high") ==
         "input_number.forecast_home_temp_high");
  assert(forecast_helper_id("weather.forecast_home", "low") ==
         "input_number.forecast_home_temp_low");
  assert(forecast_helper_id("", "high").empty());
  assert(forecast_helper_id("weather.", "high").empty());
  // A bare object id with no domain is taken as-is.
  assert(forecast_helper_id("home", "high") == "input_number.home_temp_high");

  // Low implied by an explicit high.
  assert(forecast_low_from_high("input_number.my_high") == "input_number.my_low");
  assert(forecast_low_from_high("input_number.a_high_b_high") ==
         "input_number.a_high_b_low");  // last occurrence
  assert(forecast_low_from_high("input_number.nothing").empty());

  // Both derived when nothing is overridden.
  {
    auto h = resolve_forecast_helpers("weather.forecast_home", "", "");
    assert(h.high == "input_number.forecast_home_temp_high");
    assert(h.low == "input_number.forecast_home_temp_low");
  }
  // High overridden alone: the low follows it, not the weather entity — this is
  // what keeps a pre-existing single-field deck working.
  {
    auto h = resolve_forecast_helpers("weather.forecast_home",
                                      "input_number.custom_high", "");
    assert(h.high == "input_number.custom_high");
    assert(h.low == "input_number.custom_low");
  }
  // Both overridden.
  {
    auto h = resolve_forecast_helpers("weather.forecast_home",
                                      "input_number.a", "input_number.b");
    assert(h.high == "input_number.a" && h.low == "input_number.b");
  }
  // Low overridden alone still derives the high.
  {
    auto h = resolve_forecast_helpers("weather.forecast_home", "",
                                      "input_number.b");
    assert(h.high == "input_number.forecast_home_temp_high");
    assert(h.low == "input_number.b");
  }
  // No weather entity and no overrides: both empty, caller subscribes to nothing.
  {
    auto h = resolve_forecast_helpers("", "", "");
    assert(h.high.empty() && h.low.empty());
  }
  return 0;
}
