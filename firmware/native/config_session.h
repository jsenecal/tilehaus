#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>

#include <esp_random.h>

#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"

#include "config_lock.h"

namespace tilehaus {

// The device side of config_lock.h: one lock shared by the HTTP server task
// (claims and writes) and the main loop (the HA switch and the idle timeout),
// persisted so a session carries across the reboot every save causes.

namespace config_session_detail {

// The flash image. Only what must outlive a reboot: the idle timer restarts at
// boot because millis() does, so last_use_ms is not stored.
struct Saved {
  uint32_t version;
  uint8_t open;
  uint8_t claimed;
  uint8_t key[kConfigKeyBytes];
  char claimant[kClaimantLen];
};
inline constexpr uint32_t kSavedVersion = 1;

inline ConfigLock &lock() {
  static ConfigLock l;
  return l;
}
inline std::mutex &mutex() {
  static std::mutex m;
  return m;
}
inline esphome::ESPPreferenceObject &pref() {
  static esphome::ESPPreferenceObject p;
  return p;
}

// Caller holds mutex().
inline void persist() {
  const ConfigLock &l = lock();
  Saved s{};
  s.version = kSavedVersion;
  s.open = l.open;
  s.claimed = l.claimed;
  std::memcpy(s.key, l.key, sizeof(s.key));
  std::memcpy(s.claimant, l.claimant, sizeof(s.claimant));
  pref().save(&s);
  esphome::global_preferences->sync();
}

}  // namespace config_session_detail

// Set by panel_entities.yaml: publishes the HA status text ("Locked", "Open",
// "In use by 192.168.1.42"). Native code cannot reach a text_sensor directly.
inline std::function<void(const std::string &)> &config_status_hook() {
  static std::function<void(const std::string &)> hook;
  return hook;
}

// Boot: restore the session, if the switch was on when the panel went down.
inline void config_session_setup(uint32_t timeout_ms) {
  using namespace config_session_detail;
  std::lock_guard<std::mutex> g(mutex());
  pref() = esphome::global_preferences->make_preference<Saved>(0x6c6f636bUL /* 'lock' */);
  ConfigLock &l = lock();
  l.timeout_ms = timeout_ms;
  Saved s{};
  if (pref().load(&s) && s.version == kSavedVersion && s.open) {
    l.set_open(true, esphome::millis());
    if (s.claimed) {
      std::memcpy(l.key, s.key, sizeof(l.key));
      std::memcpy(l.claimant, s.claimant, sizeof(l.claimant));
      l.claimant[sizeof(l.claimant) - 1] = '\0';
      l.claimed = true;
    }
  }
}

inline bool config_session_is_open() {
  using namespace config_session_detail;
  std::lock_guard<std::mutex> g(mutex());
  return lock().open;
}

// The HA switch.
inline void config_session_set_open(bool on) {
  using namespace config_session_detail;
  std::lock_guard<std::mutex> g(mutex());
  const bool was = lock().open;
  lock().set_open(on, esphome::millis());
  if (was != on) persist();
}

// POST /api/v1/session. On Granted, `key_out` holds the hex key.
inline ClaimResult config_session_claim(const char *ip,
                                        char (&key_out)[kConfigKeyHexLen + 1]) {
  using namespace config_session_detail;
  uint8_t random[kConfigKeyBytes];
  esp_fill_random(random, sizeof(random));  // the chip's hardware RNG
  std::lock_guard<std::mutex> g(mutex());
  const ClaimResult r = lock().claim(random, ip, esphome::millis());
  if (r == ClaimResult::Granted) {
    lock().key_hex(key_out);
    persist();
  }
  return r;
}

// Every write, and the session probe. `header` is the raw Authorization value.
inline AuthResult config_session_authorize(const char *header) {
  using namespace config_session_detail;
  std::lock_guard<std::mutex> g(mutex());
  return lock().authorize(header, esphome::millis());
}

inline LockState config_session_state() {
  using namespace config_session_detail;
  std::lock_guard<std::mutex> g(mutex());
  return lock().state();
}

inline uint32_t config_session_timeout_ms() {
  using namespace config_session_detail;
  std::lock_guard<std::mutex> g(mutex());
  return lock().timeout_ms;
}

// Main loop: apply the idle timeout and keep the HA status text current.
inline void config_session_tick() {
  using namespace config_session_detail;
  std::string status;
  {
    std::lock_guard<std::mutex> g(mutex());
    ConfigLock &l = lock();
    if (l.expire(esphome::millis())) persist();
    switch (l.state()) {
      case LockState::Locked: status = "Locked"; break;
      case LockState::Open: status = "Open"; break;
      case LockState::Claimed:
        status = l.claimant[0] ? std::string("In use by ") + l.claimant : "In use";
        break;
    }
  }
  static std::string last;
  if (status != last && config_status_hook()) {
    last = status;
    config_status_hook()(status);
  }
}

}  // namespace tilehaus
