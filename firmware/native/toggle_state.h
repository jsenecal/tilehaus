#pragma once
#include <cstdint>
#include "card_config.h"  // accent_or
#include "palette.h"      // kTileBg

namespace tilehaus {

inline uint32_t toggle_color(bool on) {
  return on ? tile_on_color() : kTileBg;
}

inline bool next_toggle_on(bool current) {
  return !current;
}

}  // namespace tilehaus
