#pragma once
#include <cstdint>
#include "card_config.h"  // accent_or

namespace tilehaus {

inline uint32_t toggle_color(bool on) {
  return on ? tile_on_color() : 0x313131u;
}

inline bool next_toggle_on(bool current) {
  return !current;
}

}  // namespace tilehaus
