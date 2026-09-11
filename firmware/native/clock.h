#pragma once
#include "esphome/components/time/real_time_clock.h"

namespace tilehaus {

// Wall-clock source for the Header tile. Set once at boot from bindings.yaml
// (`tilehaus::app_clock() = id(poc_time);`) so the pure-LVGL card headers can read
// time without ESPHome ids. Null until set / before the first HA time sync.
inline esphome::time::RealTimeClock *&app_clock() {
  static esphome::time::RealTimeClock *clk = nullptr;
  return clk;
}

}  // namespace tilehaus
