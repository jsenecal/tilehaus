#include <cassert>
#include <cstring>
#include <string>

#include "config_lock.h"

using tilehaus::AuthResult;
using tilehaus::ClaimResult;
using tilehaus::ConfigLock;
using tilehaus::LockState;
using tilehaus::kConfigKeyBytes;
using tilehaus::kConfigKeyHexLen;

static const uint8_t kRandA[kConfigKeyBytes] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
static const uint8_t kRandB[kConfigKeyBytes] = {
    0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};
static const char kBearerA[] = "Bearer 00112233445566778899aabbccddeeff";

static std::string bearer(const ConfigLock &lock) {
  char hex[kConfigKeyHexLen + 1];
  lock.key_hex(hex);
  return std::string("Bearer ") + hex;
}

int main() {
  // Boots locked: nothing can claim or write.
  {
    ConfigLock lock;
    assert(lock.state() == LockState::Locked);
    assert(lock.claim(kRandA, "10.0.0.5", 0) == ClaimResult::Locked);
    assert(lock.authorize(kBearerA, 0) == AuthResult::Locked);
    assert(lock.authorize(nullptr, 0) == AuthResult::Locked);
  }

  // Open: the first claim wins, the second is refused.
  {
    ConfigLock lock;
    lock.set_open(true, 100);
    assert(lock.state() == LockState::Open);
    assert(lock.authorize(nullptr, 100) == AuthResult::Unauthorized);
    assert(lock.claim(kRandA, "10.0.0.5", 200) == ClaimResult::Granted);
    assert(lock.state() == LockState::Claimed);
    assert(std::strcmp(lock.claimant, "10.0.0.5") == 0);
    assert(lock.claim(kRandB, "10.0.0.9", 300) == ClaimResult::AlreadyClaimed);
    assert(std::strcmp(lock.claimant, "10.0.0.5") == 0);  // not overwritten
    assert(bearer(lock) == kBearerA);                     // key unchanged
  }

  // Only the claimed key writes; malformed headers are refused, not crashed on.
  {
    ConfigLock lock;
    lock.set_open(true, 0);
    lock.claim(kRandA, "10.0.0.5", 0);
    assert(lock.authorize(kBearerA, 1) == AuthResult::Ok);
    assert(lock.authorize("Bearer 00112233445566778899AABBCCDDEEFF", 1) == AuthResult::Ok);
    assert(lock.authorize("Bearer 00112233445566778899aabbccddeefe", 1) == AuthResult::Unauthorized);
    assert(lock.authorize("Bearer 00112233445566778899aabbccddeeff00", 1) == AuthResult::Unauthorized);
    assert(lock.authorize("Bearer 0011", 1) == AuthResult::Unauthorized);
    assert(lock.authorize("Bearer zz112233445566778899aabbccddeeff", 1) == AuthResult::Unauthorized);
    assert(lock.authorize("Basic 00112233445566778899aabbccddeeff", 1) == AuthResult::Unauthorized);
    assert(lock.authorize("00112233445566778899aabbccddeeff", 1) == AuthResult::Unauthorized);
    assert(lock.authorize("", 1) == AuthResult::Unauthorized);
    assert(lock.authorize(nullptr, 1) == AuthResult::Unauthorized);
  }

  // Re-sending "on" keeps the session; "off" revokes it, and reopening starts clean.
  {
    ConfigLock lock;
    lock.set_open(true, 0);
    lock.claim(kRandA, "10.0.0.5", 0);
    lock.set_open(true, 50);
    assert(lock.state() == LockState::Claimed);
    assert(lock.authorize(kBearerA, 60) == AuthResult::Ok);
    lock.set_open(false, 70);
    assert(lock.state() == LockState::Locked);
    assert(lock.authorize(kBearerA, 80) == AuthResult::Locked);
    assert(lock.claimant[0] == '\0');
    lock.set_open(true, 90);
    assert(lock.state() == LockState::Open);
    assert(lock.authorize(kBearerA, 95) == AuthResult::Unauthorized);  // old key is dead
    assert(lock.claim(kRandB, "10.0.0.9", 99) == ClaimResult::Granted);
  }

  // Idle timeout: 30 minutes from the last use, not from opening.
  {
    const uint32_t kMin = 60u * 1000u;
    ConfigLock lock;
    lock.timeout_ms = 30 * kMin;
    lock.set_open(true, 0);
    assert(!lock.expire(30 * kMin - 1));
    assert(lock.claim(kRandA, "10.0.0.5", 20 * kMin) == ClaimResult::Granted);
    assert(!lock.expire(30 * kMin));               // the claim was use
    assert(lock.authorize(kBearerA, 45 * kMin) == AuthResult::Ok);
    assert(!lock.expire(75 * kMin - 1));           // the write was use
    assert(lock.expire(75 * kMin));
    assert(lock.state() == LockState::Locked);
    assert(lock.timeout_ms == 30 * kMin);          // the setting survives a close
    assert(!lock.expire(200 * kMin));              // only fires once
  }

  // A rejected request is not use: someone guessing keys can't hold it open.
  {
    const uint32_t kMin = 60u * 1000u;
    ConfigLock lock;
    lock.timeout_ms = 30 * kMin;
    lock.set_open(true, 0);
    lock.claim(kRandA, "10.0.0.5", 0);
    assert(lock.authorize("Bearer ffffffffffffffffffffffffffffffff", 29 * kMin) == AuthResult::Unauthorized);
    assert(lock.expire(30 * kMin));
  }

  // An unclaimed window also closes after the timeout.
  {
    ConfigLock lock;
    lock.timeout_ms = 1000;
    lock.set_open(true, 500);
    assert(!lock.expire(1499));
    assert(lock.expire(1500));
  }

  // timeout 0 never expires.
  {
    ConfigLock lock;
    lock.set_open(true, 0);
    assert(!lock.expire(0xFFFFFFFFu));
    assert(lock.state() == LockState::Open);
  }

  // Wraps cleanly across the 49-day millis() rollover.
  {
    ConfigLock lock;
    lock.timeout_ms = 1000;
    lock.set_open(true, 0xFFFFFF00u);
    assert(!lock.expire(0x000002E7u));  // 999 ms later, after the wrap
    assert(lock.expire(0x000002E8u));   // 1000 ms later
  }

  // A long claimant is truncated and stays terminated.
  {
    ConfigLock lock;
    lock.set_open(true, 0);
    const std::string long_ip(200, '9');
    lock.claim(kRandA, long_ip.c_str(), 0);
    assert(std::strlen(lock.claimant) == sizeof(lock.claimant) - 1);
    ConfigLock other;
    other.set_open(true, 0);
    other.claim(kRandA, nullptr, 0);
    assert(other.claimant[0] == '\0');
  }

  return 0;
}
