#pragma once
#include <string>

namespace tilehaus {

// The panel cannot call weather.get_forecasts, so today's high/low arrive as
// input_number helpers an HA automation fills. Both the Weather tile and the
// Header resolve those ids the same way, so a deck normally configures nothing:
//
//   weather.forecast_home -> input_number.forecast_home_temp_high
//                            input_number.forecast_home_temp_low
//
// Pure (no LVGL) so the naming rules are host-tested.
inline std::string forecast_helper_id(const std::string &weather_entity,
                                      const char *suffix) {
  if (weather_entity.empty()) return "";
  std::string obj = weather_entity;
  const auto dot = obj.find('.');
  if (dot != std::string::npos) obj = obj.substr(dot + 1);
  if (obj.empty()) return "";
  return "input_number." + obj + "_temp_" + suffix;
}

// Low id implied by an explicitly configured high id: swap the last "high" for
// "low". Kept so a deck that overrides only the high keeps working — that was
// the Weather tile's original single-field behaviour. Empty when the id has no
// "high" to swap.
inline std::string forecast_low_from_high(const std::string &high_id) {
  const auto p = high_id.rfind("high");
  if (p == std::string::npos) return "";
  std::string lo = high_id;
  lo.replace(p, 4, "low");
  return lo;
}

// Resolve both helper ids. `high_override`/`low_override` are the tile's own
// fields and always win; otherwise the high is derived from the weather entity
// and the low follows the high. Either result may be empty, in which case the
// caller simply does not subscribe and the line stays blank.
struct ForecastHelpers { std::string high; std::string low; };

inline ForecastHelpers resolve_forecast_helpers(const std::string &weather_entity,
                                                const std::string &high_override,
                                                const std::string &low_override) {
  ForecastHelpers out;
  out.high = !high_override.empty() ? high_override
                                    : forecast_helper_id(weather_entity, "high");
  if (!low_override.empty()) out.low = low_override;
  else if (!high_override.empty()) out.low = forecast_low_from_high(high_override);
  else out.low = forecast_helper_id(weather_entity, "low");
  return out;
}

}  // namespace tilehaus
