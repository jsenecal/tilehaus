#pragma once
#include <vector>

namespace tilehaus {

inline constexpr int kMinCellPx = 78;  // shared with the TS editor (MIN_CELL_PX): icon glyph + 2× tile inset
inline int grid_max_cells(int px) { int n = px / kMinCellPx; return n < 2 ? 2 : n; }

struct TileSpan { int w; int h; };       // units, each >= 1
struct Placement { int col; int row; };  // 0-indexed top-left cell

// Columns that best fill the width for the target unit. Rounds to nearest so a
// ~200px unit on 1024px yields 5 columns (a strict floor would give 4). >= 1.
inline int compute_columns(int width_px, int unit_px, int pad_px, int gap_px) {
  int usable = width_px - 2 * pad_px + gap_px;   // + one gap: N cells have N-1 gaps
  int denom = unit_px + gap_px;
  if (denom <= 0) return 1;
  int c = (usable + denom / 2) / denom;          // round to nearest
  return c < 1 ? 1 : c;
}

inline std::vector<Placement> place_tiles(const std::vector<TileSpan>& tiles,
                                          int columns, int& out_rows) {
  if (columns < 1) columns = 1;
  std::vector<Placement> out;
  out.reserve(tiles.size());
  std::vector<unsigned char> occ;  // occupancy grid, row-major, grows by row
  int rows = 0;

  auto ensure_rows = [&](int need) {
    if (need > rows) { occ.resize(static_cast<size_t>(need) * columns, 0); rows = need; }
  };
  auto fits = [&](int c, int r, int w, int h) {
    for (int rr = r; rr < r + h; ++rr)
      for (int cc = c; cc < c + w; ++cc)
        if (occ[static_cast<size_t>(rr) * columns + cc]) return false;
    return true;
  };
  auto mark = [&](int c, int r, int w, int h) {
    for (int rr = r; rr < r + h; ++rr)
      for (int cc = c; cc < c + w; ++cc)
        occ[static_cast<size_t>(rr) * columns + cc] = 1;
  };

  for (const auto& t : tiles) {
    int w = t.w < 1 ? 1 : (t.w > columns ? columns : t.w);
    int h = t.h < 1 ? 1 : t.h;
    int pr = 0, pc = 0;
    bool placed = false;
    for (int r = 0; !placed; ++r) {
      ensure_rows(r + h);
      for (int c = 0; c + w <= columns; ++c) {
        if (fits(c, r, w, h)) { pr = r; pc = c; placed = true; break; }
      }
    }
    mark(pc, pr, w, h);
    out.push_back(Placement{pc, pr});
  }

  out_rows = rows;
  return out;
}

}  // namespace tilehaus
