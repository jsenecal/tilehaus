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

// Pixel geometry of the final tile grid. Mirrors what poc_build_grid hands
// LVGL, so a caller can size a tile *before* any layout pass: columns are
// LV_GRID_FR(1) tracks that divide the remaining width evenly, and row tracks
// are set to an explicit height.
struct GridMetrics { int cell_w; int cell_h; int gap; };

// The final sub-unit column count. `base_cols` (0 = derive from unit_px) sets
// the pre-subdivision grid; `override_cols` (0 = derive) sets the final grid
// outright, already including any subdivision factor.
inline int grid_final_columns(int width_px, int unit_px, int pad_px, int gap_px,
                              int subdivisions, int base_cols, int override_cols) {
  if (subdivisions < 1) subdivisions = 1;
  const int normal = base_cols > 0
                         ? base_cols
                         : compute_columns(width_px, unit_px, pad_px, gap_px);
  const int cols = override_cols > 0 ? override_cols : normal * subdivisions;
  return cols < 1 ? 1 : cols;
}

// Row twin of grid_final_columns — compute_columns is dimension-agnostic track
// math, so height reuses it exactly as poc_build_grid does.
inline int grid_final_rows(int height_px, int unit_px, int pad_px, int gap_px,
                           int subdivisions, int base_rows, int override_rows) {
  if (subdivisions < 1) subdivisions = 1;
  const int normal = base_rows > 0
                         ? base_rows
                         : compute_columns(height_px, unit_px, pad_px, gap_px);
  const int rows = override_rows > 0 ? override_rows : normal * subdivisions;
  return rows < 1 ? 1 : rows;
}

inline GridMetrics compute_grid_metrics(int width_px, int height_px, int unit_px,
                                        int pad_px, int gap_px, int subdivisions,
                                        int base_cols, int base_rows,
                                        int override_cols, int override_rows) {
  const int cols = grid_final_columns(width_px, unit_px, pad_px, gap_px,
                                      subdivisions, base_cols, override_cols);
  const int rows = grid_final_rows(height_px, unit_px, pad_px, gap_px,
                                   subdivisions, base_rows, override_rows);
  int cell_w = (width_px - 2 * pad_px - (cols - 1) * gap_px) / cols;
  if (cell_w < 1) cell_w = 1;
  int cell_h = (height_px - 2 * pad_px - (rows - 1) * gap_px) / rows;
  if (cell_h < 1) cell_h = 1;
  return GridMetrics{cell_w, cell_h, gap_px};
}

// Pixel size of a tile spanning `span` tracks: the tracks plus the gaps between
// them.
inline int tile_pixel_width(const GridMetrics &m, int span) {
  if (span < 1) span = 1;
  return span * m.cell_w + (span - 1) * m.gap;
}
inline int tile_pixel_height(const GridMetrics &m, int span) {
  if (span < 1) span = 1;
  return span * m.cell_h + (span - 1) * m.gap;
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
