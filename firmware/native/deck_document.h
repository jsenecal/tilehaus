#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "card_config.h"

namespace tilehaus {

inline constexpr uint16_t kDeckDocumentVersion = 6;
inline constexpr size_t kDeckHeaderSize = 16;     // v1/v2 header
inline constexpr size_t kDeckHeaderSizeV3 = 20;   // v3 appends a 4-byte accent; also v5
inline constexpr size_t kDeckHeaderSizeV6 = 22;   // v6 appends gridCols/gridRows
inline constexpr size_t kDeckMaxCardCount = 128;
// Capacity of the persisted deck blob (see DeckStore). Lives here so the codec
// and its host tests can assert a max-size deck still fits without dragging in
// the esphome preferences header.
inline constexpr size_t kDeckMaxDocumentBytes = 16384;
inline constexpr uint8_t kDeckMaxType = 22;
inline constexpr int32_t kDeckAccentDefault = -1;  // -1 = use built-in card colours
inline constexpr int kDeckGridColsDefault = 10;
inline constexpr int kDeckGridRowsDefault = 6;

namespace deck_detail {

inline uint16_t read_u16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline uint32_t read_u32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline int32_t read_i32(const uint8_t *p) { return static_cast<int32_t>(read_u32(p)); }

// Reads a u8-length-prefixed string starting at `offset`. Returns false on any
// overflow; advances `offset` past the field on success.
inline bool read_string(const uint8_t *data, size_t len, size_t &offset, std::string &out) {
  if (offset >= len) return false;
  const size_t str_len = data[offset];
  const size_t start = offset + 1;
  if (start + str_len > len) return false;
  out.assign(reinterpret_cast<const char *>(data + start), str_len);
  offset = start + str_len;
  return true;
}

}  // namespace deck_detail

// Decodes a DECK document into `out`. On any malformed input returns false and
// leaves `out` untouched, so callers can keep the previously applied deck.
inline bool decode_deck(const uint8_t *data, size_t len, std::vector<CardConfig> &out,
                        int32_t *out_accent = nullptr,
                        std::vector<std::string> *out_pages = nullptr,
                        int *out_grid_cols = nullptr, int *out_grid_rows = nullptr) {
  using namespace deck_detail;
  if (data == nullptr || len < kDeckHeaderSize) return false;
  if (data[0] != 0x44 || data[1] != 0x45 || data[2] != 0x43 || data[3] != 0x4b) return false;  // DECK
  const uint16_t version = read_u16(data + 4);
  if (version != 5 && version != 6) return false;
  const size_t header_size = version == 6 ? kDeckHeaderSizeV6 : kDeckHeaderSizeV3;  // v5 == 20
  if (len < header_size) return false;
  if (read_u16(data + 6) != header_size) return false;
  if (data[15] != 0) return false;
  if (read_u32(data + 8) != len - header_size) return false;
  const int32_t accent = read_i32(data + 16);
  const size_t page_count = data[14];
  if (page_count < 1) return false;
  const int grid_cols = version == 6 ? data[20] : kDeckGridColsDefault;
  const int grid_rows = version == 6 ? data[21] : kDeckGridRowsDefault;
  const size_t card_count = read_u16(data + 12);
  if (card_count > kDeckMaxCardCount) return false;
  size_t offset = header_size;
  std::vector<std::string> pages;
  for (size_t i = 0; i < page_count; ++i) {
    std::string name;
    if (!read_string(data, len, offset, name)) return false;
    pages.push_back(std::move(name));
  }
  std::vector<CardConfig> parsed;
  parsed.reserve(card_count);
  for (size_t index = 0; index < card_count; ++index) {
    if (offset + 16 > len) return false;
    const uint8_t type = data[offset];
    if (type > kDeckMaxType) return false;
    CardConfig card;
    card.type = static_cast<CardType>(type);
    card.w = data[offset + 1];
    card.h = data[offset + 2];
    const uint8_t flags = data[offset + 3];
    card.hide_label = (flags & 0x01) != 0;
    card.detail = (flags & 0x02) != 0;
    card.follow_color = (flags & 0x04) != 0;
    card.show_hilo = (flags & 0x08) != 0;
    card.transparent = (flags & 0x10) != 0;
    card.show_weather = (flags & 0x20) == 0;  // stored inverted (set bit = hidden)
    card.show_clock = (flags & 0x40) == 0;
    card.tight_margins = (flags & 0x80) != 0;
    card.active_color = read_i32(data + offset + 4);
    card.inactive_color = read_i32(data + offset + 8);
    card.col = data[offset + 12];
    card.row = data[offset + 13];
    card.align = data[offset + 14];
    card.page = data[offset + 15];
    offset += 16;
    if (!read_string(data, len, offset, card.entity)) return false;
    if (!read_string(data, len, offset, card.title)) return false;
    if (!read_string(data, len, offset, card.entity2)) return false;
    if (!read_string(data, len, offset, card.icon)) return false;
    if (!read_string(data, len, offset, card.icon_alt)) return false;
    parsed.push_back(std::move(card));
  }
  if (offset != len) return false;
  out = std::move(parsed);
  if (out_accent) *out_accent = accent;
  if (out_pages) *out_pages = std::move(pages);
  if (out_grid_cols) *out_grid_cols = grid_cols;
  if (out_grid_rows) *out_grid_rows = grid_rows;
  return true;
}

}  // namespace tilehaus
