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

  return 0;
}
