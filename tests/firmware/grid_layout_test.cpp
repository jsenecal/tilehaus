#include <cassert>
#include <vector>

#include "grid_layout.h"

int main() {
  using tilehaus::compute_columns;

  // 1024-wide panel, unit 200, pad 16, gap 10 -> rounds to 5 (not floor 4).
  assert(compute_columns(1024, 200, 16, 10) == 5);
  assert(compute_columns(1920, 200, 16, 10) == 9);
  assert(compute_columns(480, 200, 16, 10) == 2);
  assert(compute_columns(200, 200, 16, 10) == 1);
  assert(compute_columns(100, 200, 16, 10) == 1);   // clamp to >= 1

  using tilehaus::place_tiles;
  using tilehaus::TileSpan;
  using tilehaus::Placement;

  int rows = -1;

  // Three 1x1 in a 5-wide grid: one row, left to right.
  {
    std::vector<TileSpan> t = {{1,1},{1,1},{1,1}};
    auto p = place_tiles(t, 5, rows);
    assert(rows == 1);
    assert(p[0].col == 0 && p[0].row == 0);
    assert(p[1].col == 1 && p[1].row == 0);
    assert(p[2].col == 2 && p[2].row == 0);
  }

  // A 2x1 wide then three 1x1: wide takes cols 0-1, rest fill 2,3,4.
  {
    std::vector<TileSpan> t = {{2,1},{1,1},{1,1},{1,1}};
    auto p = place_tiles(t, 5, rows);
    assert(rows == 1);
    assert(p[0].col == 0 && p[0].row == 0);
    assert(p[1].col == 2 && p[1].row == 0);
    assert(p[3].col == 4 && p[3].row == 0);
  }

  // A 1x2 tall then 1x1s: tall occupies (0,0)-(0,1); next 1x1 goes to (1,0).
  {
    std::vector<TileSpan> t = {{1,2},{1,1}};
    auto p = place_tiles(t, 5, rows);
    assert(rows == 2);
    assert(p[0].col == 0 && p[0].row == 0);
    assert(p[1].col == 1 && p[1].row == 0);
  }

  // Wrap: six 1x1 in a 5-wide grid -> second row starts at (0,1).
  {
    std::vector<TileSpan> t(6, TileSpan{1,1});
    auto p = place_tiles(t, 5, rows);
    assert(rows == 2);
    assert(p[5].col == 0 && p[5].row == 1);
  }

  // Width clamp: a 9-wide tile in a 5-wide grid is clamped to span all 5.
  {
    std::vector<TileSpan> t = {{9,1}};
    auto p = place_tiles(t, 5, rows);
    assert(rows == 1);
    assert(p[0].col == 0 && p[0].row == 0);
  }

  // --- pixel metrics: the office panel's real geometry ---
  // 1024x600, deck grid 12x7 (passed as overrides), pad 16, gap 10.
  {
    using tilehaus::compute_grid_metrics;
    using tilehaus::tile_pixel_width;
    using tilehaus::tile_pixel_height;
    using tilehaus::GridMetrics;

    const GridMetrics m =
        compute_grid_metrics(1024, 600, 200, 16, 10, 2, 0, 0, 12, 7);
    assert(m.cell_w == 73);   // (1024 - 2*16 - 11*10) / 12
    assert(m.cell_h == 72);   // (600  - 2*16 -  6*10) / 7
    assert(m.gap == 10);

    assert(tile_pixel_width(m, 2) == 156);    // 2x2 tile — the tight case
    assert(tile_pixel_width(m, 4) == 322);    // 4x2 All Office Lights
    assert(tile_pixel_width(m, 12) == 986);   // full-width header
    assert(tile_pixel_width(m, 1) == 73);
    assert(tile_pixel_width(m, 0) == 73);     // clamps to >= 1 span

    assert(tile_pixel_height(m, 1) == 72);    // 12x1 header slot
    assert(tile_pixel_height(m, 2) == 154);
    assert(tile_pixel_height(m, 4) == 318);   // 2x4 Downlights
  }

  // Auto-derived grid (no overrides) must match what poc_build_grid would use.
  {
    using tilehaus::compute_grid_metrics;
    using tilehaus::grid_final_columns;
    using tilehaus::grid_final_rows;

    // unit 200 on 1024 -> 5 normal columns, x2 subdivisions = 10.
    assert(grid_final_columns(1024, 200, 16, 10, 2, 0, 0) == 10);
    // base_cols wins over the unit-derived value, still x2.
    assert(grid_final_columns(1024, 200, 16, 10, 2, 5, 0) == 10);
    // override wins outright and is already final (no subdivision factor).
    assert(grid_final_columns(1024, 200, 16, 10, 2, 5, 12) == 12);
    assert(grid_final_rows(600, 200, 16, 10, 2, 3, 0) == 6);
    assert(grid_final_rows(600, 200, 16, 10, 2, 3, 7) == 7);

    const auto m = compute_grid_metrics(1024, 600, 200, 16, 10, 2, 5, 3, 0, 0);
    assert(m.cell_w == (1024 - 32 - 9 * 10) / 10);
    assert(m.cell_h == (600 - 32 - 5 * 10) / 6);
  }

  // Degenerate inputs clamp instead of dividing by zero or going negative.
  {
    using tilehaus::compute_grid_metrics;
    const auto m = compute_grid_metrics(40, 40, 200, 16, 10, 1, 0, 0, 8, 8);
    assert(m.cell_w >= 1);
    assert(m.cell_h >= 1);
  }

  return 0;
}
