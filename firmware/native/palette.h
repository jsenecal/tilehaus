#pragma once
#include <cstdint>

namespace tilehaus {

// The colour palette, as plain constants with no LVGL dependency so pure
// helpers (toggle_state.h, host-tested) can share it with the UI headers.
// Day/night page colours and the theme logic live in theme.h.

inline constexpr uint32_t kTileBg = 0x313131;     // tile chrome, and a tile's "off"
inline constexpr uint32_t kControlBg = 0x2A2A2A;  // raised controls in modals
inline constexpr uint32_t kPressedBg = 0x484848;  // a control under the finger

}  // namespace tilehaus
