#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "esphome/core/preferences.h"

namespace tilehaus {

// Durable opaque store for the DECK document. Persists the document bytes plus a
// monotonic generation used for the HTTP ETag / If-Match. Knows nothing about
// card fields — it stores bytes, like the shipping configuration_store contract.
class DeckStore {
 public:
  // Fixed capacity persisted blob: 4-byte length + 4-byte generation + payload.
  static constexpr size_t kMaxDocumentBytes = 4096;

  void setup() {
    // Stable hashed key for this panel's deck document.
    pref_ = esphome::global_preferences->make_preference<Blob>(0x6465636bUL /* 'deck' */);
    // Blob is ~4 KB — keep it off the stack (heap) even on the main task.
    auto blob = std::unique_ptr<Blob>(new Blob());
    if (pref_.load(blob.get()) && blob->length <= kMaxDocumentBytes) {
      length_ = blob->length;
      generation_ = blob->generation;
      std::memcpy(bytes_, blob->data, length_);
    }
  }

  bool empty() const { return length_ == 0; }
  uint32_t generation() const { return generation_; }
  const uint8_t *data() const { return bytes_; }
  size_t length() const { return length_; }

  // Persists `len` bytes, bumps the generation, returns the new generation
  // (0 = write failed).
  uint32_t save(const uint8_t *data, size_t len) {
    if (data == nullptr || len == 0 || len > kMaxDocumentBytes) return 0;
    // Blob is ~4 KB; heap-allocate it — this runs on the HTTP server task whose
    // stack is far too small for a 4 KB stack frame (would overflow → crash).
    auto blob = std::unique_ptr<Blob>(new Blob());
    blob->length = static_cast<uint32_t>(len);
    blob->generation = generation_ + 1;
    std::memcpy(blob->data, data, len);
    if (!pref_.save(blob.get())) return 0;
    esphome::global_preferences->sync();
    length_ = len;
    generation_ = blob->generation;
    std::memcpy(bytes_, data, len);
    return generation_;
  }

  // Clears the stored document (length 0) and bumps the generation, so after a
  // reboot the panel comes up unconfigured. Returns the new generation (0 = fail).
  uint32_t clear() {
    auto blob = std::unique_ptr<Blob>(new Blob());
    blob->length = 0;
    blob->generation = generation_ + 1;
    if (!pref_.save(blob.get())) return 0;
    esphome::global_preferences->sync();
    length_ = 0;
    generation_ = blob->generation;
    return generation_;
  }

 private:
  struct Blob {
    uint32_t length{0};
    uint32_t generation{0};
    uint8_t data[kMaxDocumentBytes]{};
  };

  esphome::ESPPreferenceObject pref_{};
  uint8_t bytes_[kMaxDocumentBytes]{};
  size_t length_{0};
  uint32_t generation_{0};
};

// Process-wide store instance (constructed in bindings on_boot).
inline DeckStore &deck_store() {
  static DeckStore store;
  return store;
}

}  // namespace tilehaus
