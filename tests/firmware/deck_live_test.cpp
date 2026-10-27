#include <cassert>
#include <string>

#include "deck_live.h"

using namespace tilehaus;

static DeckParts deck() {
  DeckParts d;
  d.pages = {"Home", "Lights"};
  d.page_back = {false, true};
  d.grid_cols = 12;
  d.grid_rows = 7;
  CardConfig a;
  a.type = CardType::Toggle;
  a.entity = "switch.fan";
  a.title = "Fan";
  a.w = 2;
  a.h = 2;
  CardConfig b;
  b.type = CardType::Gauge;
  b.entity = "sensor.humidity";
  b.entity2 = "0..100 <30";
  b.title = "Humidity";
  b.col = 2;
  b.page = 1;
  d.cards = {a, b};
  return d;
}

int main() {
  // Unchanged: live, nothing to rebuild.
  {
    const LiveChange c = deck_live_change(deck(), deck());
    assert(c.live && c.cards.empty() && !c.accent);
  }
  // Looks only: those tiles, live.
  {
    DeckParts to = deck();
    to.cards[1].title = "Damp";
    to.cards[1].icon = "\xF3\xB0\x96\x8E";
    to.cards[1].active_color = 0x112233;
    const LiveChange c = deck_live_change(deck(), to);
    assert(c.live && c.cards.size() == 1 && c.cards[0] == 1);
  }
  {
    DeckParts to = deck();
    to.cards[0].hide_label = true;
    to.cards[0].transparent = true;
    to.cards[0].align = 0;
    to.accent = 0xFF8800;
    const LiveChange c = deck_live_change(deck(), to);
    assert(c.live && c.cards.size() == 1 && c.cards[0] == 0 && c.accent);
  }
  // Anything structural: a reboot.
  auto reboots = [](void (*edit)(DeckParts &)) {
    DeckParts to = deck();
    edit(to);
    return !deck_live_change(deck(), to).live;
  };
  assert(reboots([](DeckParts &d) { d.cards[0].entity = "switch.lamp"; }));
  assert(reboots([](DeckParts &d) { d.cards[1].entity2 = "0..100"; }));
  assert(reboots([](DeckParts &d) { d.cards[0].col = 4; }));
  assert(reboots([](DeckParts &d) { d.cards[0].w = 3; }));
  assert(reboots([](DeckParts &d) { d.cards[0].type = CardType::LightSlider; }));
  assert(reboots([](DeckParts &d) { d.cards[1].page = 0; }));
  assert(reboots([](DeckParts &d) { d.cards[0].detail = true; }));
  assert(reboots([](DeckParts &d) { d.cards.pop_back(); }));
  assert(reboots([](DeckParts &d) { d.pages[1] = "Lamps"; }));
  assert(reboots([](DeckParts &d) { d.page_back[1] = false; }));
  assert(reboots([](DeckParts &d) { d.grid_cols = 10; }));
  return 0;
}
