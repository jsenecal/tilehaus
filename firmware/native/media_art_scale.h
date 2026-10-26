#pragma once
#include <cstdint>
#include <cstring>

namespace tilehaus {

// Cover art comes in any size; the Media card and its full view draw a square
// of a fixed size. This fills that square from a decoded RGB565 picture: the
// centred square crop of the picture, each output pixel the average of the
// source pixels it covers (the nearest one when enlarging). Pure, so it is
// host-tested.
//
// `src` is w x h with `stride` pixels per row (the JPEG decoder pads rows to
// whole blocks); `dst` is px x px.
inline void art_fit_square(const uint16_t *src, int w, int h, int stride, uint16_t *dst, int px) {
  if (!src || !dst || w <= 0 || h <= 0 || px <= 0) return;
  const int side = w < h ? w : h;
  const int x0 = (w - side) / 2, y0 = (h - side) / 2;
  if (side == px) {
    for (int y = 0; y < px; ++y)
      std::memcpy(dst + y * px, src + (y0 + y) * stride + x0, px * sizeof(uint16_t));
    return;
  }
  for (int y = 0; y < px; ++y) {
    const int sy0 = y0 + static_cast<int>(static_cast<int64_t>(y) * side / px);
    int sy1 = y0 + static_cast<int>(static_cast<int64_t>(y + 1) * side / px);
    if (sy1 <= sy0) sy1 = sy0 + 1;
    for (int x = 0; x < px; ++x) {
      const int sx0 = x0 + static_cast<int>(static_cast<int64_t>(x) * side / px);
      int sx1 = x0 + static_cast<int>(static_cast<int64_t>(x + 1) * side / px);
      if (sx1 <= sx0) sx1 = sx0 + 1;
      uint32_t r = 0, g = 0, b = 0, n = 0;
      for (int sy = sy0; sy < sy1; ++sy) {
        const uint16_t *row = src + sy * stride;
        for (int sx = sx0; sx < sx1; ++sx) {
          const uint16_t p = row[sx];
          r += p >> 11;
          g += (p >> 5) & 0x3F;
          b += p & 0x1F;
          ++n;
        }
      }
      dst[y * px + x] = static_cast<uint16_t>(((r / n) << 11) | ((g / n) << 5) | (b / n));
    }
  }
}

}  // namespace tilehaus
