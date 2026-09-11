#pragma once
#include <vector>
#include "card.h"

namespace tilehaus {

// Fallback default deck, used when no configuration is stored. Fills the
// 10x6 half-unit grid exactly (5x3 base x2 subdivisions), no scroll. Header
// spans the full top; the two rows below are five 2x2 tiles each — new
// status/action tiles on top, existing control demos beneath.
inline std::vector<CardConfig> default_deck() {
  return {
    // Header: full-width transparent greeting + clock + weather.
    // entity = greeting-line template sensor; title = subtitle template sensor
    // (both verbatim from HA); entity2 = weather source for the glyph + temp.
    {CardType::Header, "sensor.panel_greeting", "sensor.panel_subtitle", 10, 2,
     "weather.forecast_home", "", ""},

    // Row A — status + action tiles.
    // Presence demos a per-tile active color: green when occupied (else default).
    {CardType::Presence, "binary_sensor.office_presence_sensor_occupancy",
     "Presence", 2, 2, "",
     "\U000F0D91", "\U000F1435",   // mdi-motion-sensor / motion-sensor-off
     0x2E7D32, -1, false},         // active=green, inactive=default, label shown
    {CardType::Weather, "weather.forecast_home", "Weather", 2, 2,
     "input_number.forecast_home_temp_high", ""},  // entity2 = forecast high helper
    // [test slot] LightSlider on the desk lamp to demo follow_color on a
    // brightness slider (the fill tracks the lamp's colour).
    {CardType::LightSlider, "light.office_desk_lamp", "Lamp Dim", 2, 2, "",
     "\U000F095F", "\U000F1B1F",   // mdi-desk-lamp / mdi-desk-lamp-off
     -1, -1, false, false, true},  // follow_color: fill tints to the lamp's colour
    {CardType::Wled, "light.office_corner", "Office Corner", 2, 2, "",
     "\U000F1051", "\U000F1A4B"},  // mdi-led-strip-variant / -off (modal on tap)
    {CardType::Lock, "lock.front_door_lock", "Front Door", 2, 2, "",
     "\U000F033E", "\U000F033F",   // mdi-lock / mdi-lock-open
     -1, -1, true},                // default colors, label hidden

    // Row B — existing control demos, resized to 2x2 to match the showcase.
    {CardType::LightControl, "light.office_desk_lamp", "Desk Lamp", 2, 2, "",
     "\U000F095F", "\U000F1B1F",   // mdi-desk-lamp / mdi-desk-lamp-off (modal on tap)
     -1, -1, false, false, true},  // follow_color: tile tints to the lamp's colour
    {CardType::Toggle, "switch.office_air_filter_s31_relay", "Air Filter", 2, 2, "",
     "\U000F0D44", "\U000F1B57"},  // mdi-air-purifier / mdi-air-purifier-off
    // Sensor shows its value in the icon slot, so no icon glyph.
    {CardType::Sensor, "sensor.office_average_temperature", "Temp", 2, 2, "", ""},
    {CardType::Cover, "cover.office_blinds", "Office Blinds", 2, 2, "",
     "\U000F00AC", "\U000F1011",   // mdi-blinds (closed) / mdi-blinds-open
     -1, -1, false, true},         // detail chevron → cover modal
    // Climate: tap opens the HA-style thermostat modal (arc + modes + presets).
    {CardType::Climate, "climate.hydronics_office_climate_controller",
     "Office Heat", 2, 2, "",
     "\U000F0393", ""},            // mdi-thermostat
  };
}

}  // namespace tilehaus
