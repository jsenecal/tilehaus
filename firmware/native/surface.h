#pragma once
#include "lvgl.h"

namespace tilehaus {

// The app's grid container (set once when the deck is built). The light modal
// fades this out as it appears and back in as it closes, so opening reads as the
// grid disappearing and the modal taking its place. A plain accessor keeps the
// modal decoupled from card_host (no include cycle).
inline lv_obj_t *&app_grid_root() {
  static lv_obj_t *root = nullptr;
  return root;
}

}  // namespace tilehaus
