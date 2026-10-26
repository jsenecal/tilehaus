#include <cassert>
#include <cstdint>
#include <vector>
#include "media_art_scale.h"

namespace {
uint16_t rgb(int r, int g, int b) { return static_cast<uint16_t>((r << 11) | (g << 5) | b); }
}  // namespace

int main() {
  using namespace tilehaus;

  // Same size: a copy, skipping the decoder's row padding.
  {
    const int w = 3, h = 3, stride = 8;
    std::vector<uint16_t> src(stride * h, 0xFFFF);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) src[y * stride + x] = static_cast<uint16_t>(y * 10 + x);
    std::vector<uint16_t> dst(9);
    art_fit_square(src.data(), w, h, stride, dst.data(), 3);
    for (int y = 0; y < 3; ++y)
      for (int x = 0; x < 3; ++x) assert(dst[y * 3 + x] == y * 10 + x);
  }
  // A wide picture is cropped to its centre square.
  {
    const int w = 4, h = 2;
    std::vector<uint16_t> src = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<uint16_t> dst(4);
    art_fit_square(src.data(), w, h, w, dst.data(), 2);
    assert(dst[0] == 2 && dst[1] == 3 && dst[2] == 6 && dst[3] == 7);
  }
  // Halving averages each 2x2 block, per channel.
  {
    std::vector<uint16_t> src = {
        rgb(0, 0, 0), rgb(4, 8, 2), rgb(31, 63, 31), rgb(31, 63, 31),
        rgb(8, 16, 4), rgb(4, 8, 2), rgb(31, 63, 31), rgb(31, 63, 31),
        rgb(2, 2, 2), rgb(2, 2, 2), rgb(0, 0, 0), rgb(0, 0, 0),
        rgb(2, 2, 2), rgb(2, 2, 2), rgb(0, 0, 0), rgb(0, 0, 0)};
    std::vector<uint16_t> dst(4);
    art_fit_square(src.data(), 4, 4, 4, dst.data(), 2);
    assert(dst[0] == rgb(4, 8, 2));
    assert(dst[1] == rgb(31, 63, 31));
    assert(dst[2] == rgb(2, 2, 2));
    assert(dst[3] == rgb(0, 0, 0));
  }
  // Enlarging repeats the nearest pixel.
  {
    std::vector<uint16_t> src = {1, 2, 3, 4};
    std::vector<uint16_t> dst(16);
    art_fit_square(src.data(), 2, 2, 2, dst.data(), 4);
    assert(dst[0] == 1 && dst[1] == 1 && dst[2] == 2 && dst[3] == 2);
    assert(dst[12] == 3 && dst[15] == 4);
  }
  // Nothing to draw from: the output is left alone.
  {
    std::vector<uint16_t> dst(4, 9);
    art_fit_square(nullptr, 2, 2, 2, dst.data(), 2);
    assert(dst[0] == 9);
  }
  return 0;
}
