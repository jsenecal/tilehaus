#pragma once
#include <string>

namespace tilehaus {

// Map a Home Assistant weather condition string to an MDI glyph. Every glyph
// here is already present in common/assets/icon_glyphs.yaml. Default: cloudy.
inline const char *weather_glyph(const std::string &c) {
  if (c == "sunny" || c == "clear") return "\U000F0599";          // weather-sunny
  if (c == "clear-night") return "\U000F0594";                    // weather-night
  if (c == "partlycloudy") return "\U000F0595";                   // partly-cloudy
  if (c == "cloudy") return "\U000F0590";                         // cloudy
  if (c == "rainy") return "\U000F0597";                          // rainy
  if (c == "pouring") return "\U000F0596";                        // pouring
  if (c == "snowy") return "\U000F0598";                          // snowy
  if (c == "snowy-rainy") return "\U000F067F";                    // snowy-rainy
  if (c == "fog") return "\U000F0591";                            // fog
  if (c == "hail") return "\U000F0592";                           // hail
  if (c == "lightning") return "\U000F0593";                      // lightning
  if (c == "lightning-rainy") return "\U000F067E";                // lightning-rainy
  if (c == "windy" || c == "windy-variant") return "\U000F059D";  // windy
  if (c == "exceptional") return "\U000F0F2F";                    // cloudy-alert
  return "\U000F0590";                                            // default cloudy
}

// Human-readable label for a Home Assistant weather condition. Covers the full
// set of documented conditions; falls back to an em dash for anything else.
inline const char *weather_label(const std::string &c) {
  if (c == "sunny") return "Sunny";
  if (c == "clear" || c == "clear-night") return "Clear";
  if (c == "partlycloudy") return "Partly cloudy";
  if (c == "cloudy") return "Cloudy";
  if (c == "rainy") return "Rainy";
  if (c == "pouring") return "Pouring";
  if (c == "snowy") return "Snowy";
  if (c == "snowy-rainy") return "Sleet";
  if (c == "fog") return "Fog";
  if (c == "hail") return "Hail";
  if (c == "lightning") return "Thunder";
  if (c == "lightning-rainy") return "Thunderstorm";
  if (c == "windy" || c == "windy-variant") return "Windy";
  if (c == "exceptional") return "Alert";
  return "\xE2\x80\x94";  // em dash
}

}  // namespace tilehaus
