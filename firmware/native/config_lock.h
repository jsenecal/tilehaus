#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tilehaus {

// Who may write the deck. Home Assistant is the gatekeeper: its "Configurator"
// switch opens the panel, the first client to claim a session gets a random
// key, and every write must carry that key until the switch goes off.
//
//   Locked  --switch on-->  Open  --first claim-->  Claimed
//      ^                                              |
//      +---- switch off, or idle for timeout_ms ------+
//
// The key survives reboots (the caller persists it), because a save reboots the
// panel and the session should carry on across it. What ends a session is the
// switch going off, by hand or by the idle timeout. The timer restarts at boot,
// since millis() does.
//
// Pure on purpose: no LVGL, no ESP-IDF, no globals, no clock. Randomness and
// time come in as arguments, so every rule here is host-tested.

inline constexpr size_t kConfigKeyBytes = 16;                  // 128 bits
inline constexpr size_t kConfigKeyHexLen = kConfigKeyBytes * 2;
inline constexpr size_t kClaimantLen = 48;                     // fits an IPv6 address

enum class LockState : uint8_t { Locked, Open, Claimed };
enum class ClaimResult : uint8_t { Granted, Locked, AlreadyClaimed };
enum class AuthResult : uint8_t { Ok, Locked, Unauthorized };

struct ConfigLock {
  bool open = false;
  bool claimed = false;
  uint8_t key[kConfigKeyBytes] = {};
  char claimant[kClaimantLen] = {};
  uint32_t last_use_ms = 0;
  uint32_t timeout_ms = 0;  // 0 = never expires

  LockState state() const {
    if (!open) return LockState::Locked;
    return claimed ? LockState::Claimed : LockState::Open;
  }

  // Opening an already-open lock keeps its session: Home Assistant re-sending
  // "on" must not hand the panel to whoever asks next.
  void set_open(bool on, uint32_t now_ms) {
    if (on && open) {
      last_use_ms = now_ms;
      return;
    }
    // Locking, or opening fresh: either way no session survives.
    const uint32_t t = timeout_ms;
    *this = ConfigLock{};
    timeout_ms = t;
    open = on;
    last_use_ms = now_ms;
  }

  ClaimResult claim(const uint8_t (&random)[kConfigKeyBytes], const char *ip,
                    uint32_t now_ms) {
    if (!open) return ClaimResult::Locked;
    if (claimed) return ClaimResult::AlreadyClaimed;
    std::memcpy(key, random, kConfigKeyBytes);
    std::memset(claimant, 0, sizeof(claimant));
    if (ip) std::strncpy(claimant, ip, sizeof(claimant) - 1);
    claimed = true;
    last_use_ms = now_ms;
    return ClaimResult::Granted;
  }

  // `header` is the raw Authorization value ("Bearer <32 hex>"), or null when
  // the request had none. A match counts as use and restarts the idle timer.
  AuthResult authorize(const char *header, uint32_t now_ms) {
    if (!open) return AuthResult::Locked;
    if (!claimed || !matches(header)) return AuthResult::Unauthorized;
    last_use_ms = now_ms;
    return AuthResult::Ok;
  }

  // Whether `header` carries this session's key. No side effects.
  bool matches(const char *header) const {
    uint8_t given[kConfigKeyBytes];
    if (!claimed || !parse_bearer(header, given)) return false;
    // Constant time: the comparison must not reveal how many bytes matched.
    uint8_t diff = 0;
    for (size_t i = 0; i < kConfigKeyBytes; ++i) diff |= given[i] ^ key[i];
    return diff == 0;
  }

  // Closes the lock once it has sat unused for timeout_ms. Unsigned
  // subtraction keeps this right across the 49-day millis() wrap. Returns true
  // on the tick that closes it, so the caller knows to persist.
  bool expire(uint32_t now_ms) {
    if (!open || timeout_ms == 0) return false;
    if (now_ms - last_use_ms < timeout_ms) return false;
    set_open(false, now_ms);
    return true;
  }

  void key_hex(char (&out)[kConfigKeyHexLen + 1]) const {
    static const char kHex[] = "0123456789abcdef";
    for (size_t i = 0; i < kConfigKeyBytes; ++i) {
      out[i * 2] = kHex[key[i] >> 4];
      out[i * 2 + 1] = kHex[key[i] & 0x0F];
    }
    out[kConfigKeyHexLen] = '\0';
  }

  static bool parse_bearer(const char *header, uint8_t (&out)[kConfigKeyBytes]) {
    static const char kPrefix[] = "Bearer ";
    if (header == nullptr) return false;
    if (std::strncmp(header, kPrefix, sizeof(kPrefix) - 1) != 0) return false;
    const char *hex = header + sizeof(kPrefix) - 1;
    if (std::strlen(hex) != kConfigKeyHexLen) return false;
    for (size_t i = 0; i < kConfigKeyBytes; ++i) {
      const int hi = nibble(hex[i * 2]), lo = nibble(hex[i * 2 + 1]);
      if (hi < 0 || lo < 0) return false;
      out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
  }

 private:
  static int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
};

}  // namespace tilehaus
