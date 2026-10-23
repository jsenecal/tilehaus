#pragma once
#include <cstdint>

namespace tilehaus {

// The colour palette, as plain constants with no LVGL dependency so pure
// helpers (toggle_state.h, host-tested) can share it with the UI headers.
// Day/night page colours and the theme logic live in theme.h.

inline constexpr uint32_t kTileBg = 0x313131;     // tile chrome, and a tile's "off"
inline constexpr uint32_t kControlBg = 0x2A2A2A;  // raised controls in modals
inline constexpr uint32_t kPressedBg = 0x484848;  // a control under the finger
inline constexpr uint32_t kModalWell = 0x1E1E1E;  // a recessed panel inside a modal
inline constexpr uint32_t kButtonNeutral = 0x4A4A4A;  // a secondary button (Cancel)
inline constexpr uint32_t kArcTrack = 0x3A3A3A;   // the unfilled part of a dial
inline constexpr uint32_t kHairline = 0x444444;   // borders on dropdowns and lists
inline constexpr uint32_t kStroke = 0x808080;     // outlined buttons, and "off" text

// Text and glyphs on charcoal. Night mode recognises tile text by kTileInk's
// exact value and legibility twins by kBlack's (theme.h ink_page_text), so
// these two must stay pure white and pure black.
inline constexpr uint32_t kTileInk = 0xFFFFFF;       // primary text, glyphs, knobs
inline constexpr uint32_t kTileInkSoft = 0xC8C8C8;   // secondary values (hi/lo, weekday)
inline constexpr uint32_t kTileInkDim = 0xB0B0B0;    // captions in modals and overlays
inline constexpr uint32_t kTileInkFaint = 0x909090;  // tertiary (a forecast low)
inline constexpr uint32_t kBlack = 0x000000;         // overlays, scrims, text twins

// State tints: each card's built-in active/inactive colour.
inline constexpr uint32_t kStateGood = 0x2E7D32;   // locked, disarmed
inline constexpr uint32_t kStateAlert = 0xC62828;  // unlocked, a destructive confirm
inline constexpr uint32_t kStateWarm = 0xC24E1E;   // heating, armed home/away/night
inline constexpr uint32_t kStateArmed = 0xB23B3B;  // the alarm tile while armed
inline constexpr uint32_t kFanOn = 0x2C6E8A;       // cool teal while a fan runs
inline constexpr uint32_t kWledOn = 0x5A3E8A;      // WLED violet while on

// Climate.
inline constexpr uint32_t kHeatAccent = 0xB5451B;  // the dial's fill, a chosen mode
inline constexpr uint32_t kHeatingInk = 0xE1662E;  // "Heating"
inline constexpr uint32_t kCoolingInk = 0x3B9BE3;  // "Cooling"

// The colour-temperature slider runs warm to cool.
inline constexpr uint32_t kKelvinWarmEnd = 0xFF8C2A;
inline constexpr uint32_t kKelvinCoolEnd = 0xCFE0FF;

}  // namespace tilehaus
