#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "card_config.h"
#include "deck_document.h"
#include "grid_layout.h"

#ifndef DECK_FIXTURE
#error "DECK_FIXTURE path must be defined by the build"
#endif
#ifndef DECK_V5_FIXTURE
#error "DECK_V5_FIXTURE path must be defined by the build"
#endif
#ifndef DECK_V6_FIXTURE
#error "DECK_V6_FIXTURE path must be defined by the build"
#endif

static std::vector<uint8_t> read_fixture() {
  std::ifstream file(DECK_FIXTURE, std::ios::binary);
  assert(file && "fixture must open");
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
}

int main() {
  using tilehaus::CardType;
  using tilehaus::CardConfig;

  const std::vector<uint8_t> bytes = read_fixture();
  std::vector<CardConfig> cards;
  int32_t golden_accent = 0;
  assert(tilehaus::decode_deck(bytes.data(), bytes.size(), cards, &golden_accent));
  assert(golden_accent == -1);  // v3 golden carries no custom accent
  assert(cards.size() == 2);

  const CardConfig &a = cards[0];
  assert(a.type == CardType::LightControl);
  assert(a.entity == "light.desk");
  assert(a.title == "Lamp");
  assert(a.w == 2 && a.h == 2);
  assert(a.entity2.empty());
  assert(a.icon == std::string("\U000F095F"));
  assert(a.icon_alt == std::string("\U000F1B1F"));
  assert(a.active_color == -1);
  assert(a.inactive_color == -1);
  assert(a.follow_color && !a.hide_label && !a.detail);
  assert(a.col == 0 && a.row == 0);

  const CardConfig &b = cards[1];
  assert(b.type == CardType::Toggle);
  assert(b.entity == "switch.fan");
  assert(b.title == "Fan");
  assert(b.icon.empty() && b.icon_alt.empty());
  assert(b.active_color == 0x2E7D32);
  assert(b.inactive_color == -1);
  assert(b.hide_label && !b.follow_color && !b.detail);
  assert(b.col == 3 && b.row == 0);

  // Rejections: leave `out` untouched on failure.
  std::vector<CardConfig> untouched;
  std::vector<uint8_t> bad_magic = bytes; bad_magic[0] = 0x00;
  assert(!tilehaus::decode_deck(bad_magic.data(), bad_magic.size(), untouched));
  assert(untouched.empty());
  std::vector<uint8_t> bad_version = bytes; bad_version[4] = 0x63;
  assert(!tilehaus::decode_deck(bad_version.data(), bad_version.size(), untouched));
  assert(!tilehaus::decode_deck(bytes.data(), 8, untouched));                 // short header
  assert(!tilehaus::decode_deck(bytes.data(), bytes.size() - 1, untouched));  // truncated body

  // v5 fixture: named pages table + per-card page field + Page card type;
  // no grid bytes present → defaults to 10x6.
  {
    std::ifstream f(DECK_V5_FIXTURE, std::ios::binary);
    assert(f && "v5 fixture opens");
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::vector<CardConfig> pcards;
    std::vector<std::string> pages;
    int gc = 0, gr = 0;
    assert(tilehaus::decode_deck(b.data(), b.size(), pcards, nullptr, &pages, &gc, &gr));
    assert(pages.size() == 2 && pages[0] == "Home" && pages[1] == "Lights");
    assert(pcards.size() == 2);
    assert(pcards[1].type == CardType::Page);
    assert(pcards[1].entity2 == "Lights");
    assert(pcards[1].page == 0);
    assert(gc == 10 && gr == 6);
  }

  { // v6 grid fixture → 8x4
    std::ifstream f(DECK_V6_FIXTURE, std::ios::binary);
    assert(f && "v6 fixture opens");
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::vector<CardConfig> c; int gc = 0, gr = 0;
    assert(tilehaus::decode_deck(b.data(), b.size(), c, nullptr, nullptr, &gc, &gr));
    assert(gc == 8 && gr == 4);
  }

  { // a full kDeckMaxCardCount deck parses; one card over the cap is rejected.
    // Hand-built so the test pins the ceiling rather than a fixture's card count.
    auto build = [](size_t card_count) {
      std::vector<uint8_t> b(tilehaus::kDeckHeaderSizeV6);
      b[0] = 'D'; b[1] = 'E'; b[2] = 'C'; b[3] = 'K';
      b[4] = 6; b[5] = 0;                                  // version
      b[6] = tilehaus::kDeckHeaderSizeV6; b[7] = 0;        // header size
      b[12] = static_cast<uint8_t>(card_count & 0xff);     // card count (u16)
      b[13] = static_cast<uint8_t>(card_count >> 8);
      b[14] = 1;                                           // one page
      b[20] = 12; b[21] = 7;                               // grid
      for (int i = 16; i < 20; ++i) b[i] = 0xff;           // accent -1
      b.push_back(4); for (char c : std::string("Home")) b.push_back(c);
      for (size_t i = 0; i < card_count; ++i) {
        std::vector<uint8_t> card(16, 0);
        card[0] = static_cast<uint8_t>(CardType::Toggle);
        card[1] = 2; card[2] = 2;                          // w/h
        for (int k = 4; k < 12; ++k) card[k] = 0xff;       // both colours -1
        b.insert(b.end(), card.begin(), card.end());
        for (int k = 0; k < 5; ++k) b.push_back(0);        // five empty strings
      }
      const uint32_t payload = static_cast<uint32_t>(b.size() - tilehaus::kDeckHeaderSizeV6);
      b[8] = payload & 0xff; b[9] = (payload >> 8) & 0xff;
      b[10] = (payload >> 16) & 0xff; b[11] = (payload >> 24) & 0xff;
      return b;
    };

    std::vector<uint8_t> full = build(tilehaus::kDeckMaxCardCount);
    std::vector<CardConfig> c;
    assert(tilehaus::decode_deck(full.data(), full.size(), c));
    assert(c.size() == tilehaus::kDeckMaxCardCount);
    // A full deck must still fit the persisted blob.
    assert(full.size() <= tilehaus::kDeckMaxDocumentBytes);

    std::vector<uint8_t> over = build(tilehaus::kDeckMaxCardCount + 1);
    std::vector<CardConfig> c2;
    assert(!tilehaus::decode_deck(over.data(), over.size(), c2));
  }

  { // a v4 buffer is rejected
    std::vector<uint8_t> b = read_fixture(); b[4] = 0x04;
    std::vector<CardConfig> c;
    assert(!tilehaus::decode_deck(b.data(), b.size(), c));
  }

  return 0;
}
