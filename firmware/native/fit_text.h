#pragma once
#include <cstdint>

namespace tilehaus {

// Choosing how big a number can be drawn in a given space. Pure (sizes in,
// choice out) so the policy is host-tested; fit_number.h measures real text
// and applies the choice.
//
// Candidates are the faces a number could use, largest first. The first is the
// big face (180px), which can be scaled down to any size; the rest are the
// regular faces drawn as they are. Heavy downscaling makes thin strokes ragged,
// so the big face is only used while the result stays at least min_scaled_h
// tall; below that the largest regular face that fits wins.

struct FitCandidate {
  int w;          // the text's width in this face, unscaled
  int h;          // the face's line height
  bool scalable;  // the big face
};

struct FitChoice {
  int index = -1;   // into the candidates; -1 = none fit, use the last
  int scale = 256;  // LVGL transform scale (256 = 1.0); below 256 only for the big face
  int height = 0;   // the drawn height, for sizing the unit
};

inline FitChoice choose_fit(const FitCandidate *c, int n, int avail_w, int avail_h,
                            int min_scaled_h) {
  FitChoice best;
  if (n <= 0 || avail_w <= 0 || avail_h <= 0) return best;
  for (int i = 0; i < n; ++i) {
    const FitCandidate &k = c[i];
    if (k.w <= 0 || k.h <= 0) continue;
    if (k.scalable) {
      int s = avail_w * 256 / k.w;
      const int sh = avail_h * 256 / k.h;
      if (sh < s) s = sh;
      if (s > 256) s = 256;
      const int drawn = k.h * s / 256;
      if (drawn >= min_scaled_h) return FitChoice{i, s, drawn};
      continue;
    }
    if (k.w <= avail_w && k.h <= avail_h) return FitChoice{i, 256, k.h};
  }
  // Nothing fits: the smallest face, clipped, beats an empty tile.
  best.index = n - 1;
  best.height = c[n - 1].h;
  return best;
}

// The unit's face for a number drawn `number_h` tall: the largest whose line
// height is at most ~42% of it, so the unit reads as a label on the number.
// `unit_h` lists the unit faces' heights, largest first; returns an index.
inline int choose_unit_face(const int *unit_h, int n, int number_h) {
  for (int i = 0; i < n; ++i) {
    if (unit_h[i] * 100 <= number_h * 42) return i;
  }
  return n - 1;
}

}  // namespace tilehaus
