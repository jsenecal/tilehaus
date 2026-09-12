#include <cassert>

#include "tile_scale.h"

int main() {
  using tilehaus::TextScale;
  using tilehaus::text_scale_for;
  using tilehaus::kTileTightWidth;

  // A 2-column tile (156px on the office panel) is tight.
  assert(text_scale_for(156) == TextScale::Tight);
  // A 4-column tile (322px) and the full-width header (986px) are not.
  assert(text_scale_for(322) == TextScale::Default);
  assert(text_scale_for(986) == TextScale::Default);

  // The boundary is inclusive on the roomy side.
  assert(text_scale_for(kTileTightWidth - 1) == TextScale::Tight);
  assert(text_scale_for(kTileTightWidth) == TextScale::Default);

  // Degenerate widths are tight rather than crashing or reading as roomy.
  assert(text_scale_for(0) == TextScale::Tight);
  assert(text_scale_for(-10) == TextScale::Tight);

  // The header needs two rows' worth of content height to stack its clock.
  using tilehaus::header_clock_stacks;
  assert(!header_clock_stacks(72 - 32));    // 12x1 slot -> 40px content
  assert(header_clock_stacks(154 - 32));    // 12x2 slot -> 122px content
  return 0;
}
