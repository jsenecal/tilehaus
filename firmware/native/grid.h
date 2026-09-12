#pragma once
#include "lvgl.h"
#include <vector>
#include "grid_layout.h"

namespace tilehaus {

struct GridTile { lv_obj_t *obj; int w; int h; int col; int row; };

// Place static tile widgets into an adaptive grid on `root`, sized from the
// real display. `subdivisions` refines the grid: normal columns are computed
// from `unit_px`, then multiplied by `subdivisions` (2 = half-units), and each
// row track is `unit_px / subdivisions` tall. Tile w/h are in sub-units (a
// normal tile is `subdivisions × subdivisions`). Enables vertical scrolling
// when the grid is taller than the view.
// `base_cols`/`base_rows` set the normal (pre-subdivision) grid explicitly; pass
// 0 for either to auto-derive it from `unit_px` and the display size.
// `override_cols`/`override_rows` (0 = derive as above) set the FINAL sub-unit
// grid directly — already including any `subdivisions` factor — for callers
// (e.g. a deck-wide stored grid size) that computed the final grid themselves.
inline void poc_build_grid(lv_obj_t *root, const std::vector<GridTile>& tiles,
                           int unit_px, int pad_px, int gap_px, int subdivisions,
                           int base_cols, int base_rows,
                           int override_cols = 0, int override_rows = 0) {
  if (subdivisions < 1) subdivisions = 1;
  lv_display_t *disp = lv_obj_get_display(root);
  const int width = lv_display_get_horizontal_resolution(disp);
  const int height = lv_display_get_vertical_resolution(disp);

  // Geometry comes from grid_layout.h so build_page can compute the identical
  // numbers ahead of this call. Row tracks are sized so the rows that fit the
  // display fill its height exactly (symmetric with columns filling the width);
  // rows beyond that keep the same height and overflow into a vertical scroll.
  const int columns = grid_final_columns(width, unit_px, pad_px, gap_px,
                                         subdivisions, base_cols, override_cols);
  const GridMetrics gm = compute_grid_metrics(width, height, unit_px, pad_px,
                                              gap_px, subdivisions, base_cols,
                                              base_rows, override_cols, override_rows);
  const int row_h = gm.cell_h;

  int rows = 0;
  for (const auto& t : tiles) { int bottom = t.row + (t.h > 0 ? t.h : 1); if (bottom > rows) rows = bottom; }
  if (rows < 1) rows = 1;

  // Descriptor arrays must outlive the call; LVGL keeps the pointer. Multiple
  // containers (one per deck page) can each call this during the same boot, so
  // a single shared static buffer would alias across them — each call gets its
  // own heap-allocated, intentionally-never-freed array (process-lifetime UI,
  // same pattern as live_cards()).
  // LVGL 9.5's grid descriptors are int32_t (not the v8 lv_coord_t alias).
  auto *col_dsc = new std::vector<int32_t>();
  auto *row_dsc = new std::vector<int32_t>();
  col_dsc->assign(columns, LV_GRID_FR(1));
  col_dsc->push_back(LV_GRID_TEMPLATE_LAST);
  row_dsc->assign(rows, row_h);
  row_dsc->push_back(LV_GRID_TEMPLATE_LAST);

  lv_obj_set_style_pad_all(root, pad_px, 0);
  lv_obj_set_style_pad_row(root, gap_px, 0);
  lv_obj_set_style_pad_column(root, gap_px, 0);
  lv_obj_set_grid_dsc_array(root, col_dsc->data(), row_dsc->data());

  for (size_t i = 0; i < tiles.size(); ++i) {
    int w = tiles[i].w > 0 ? tiles[i].w : 1;
    int h = tiles[i].h > 0 ? tiles[i].h : 1;
    int col = tiles[i].col; if (col < 0) col = 0;
    if (col + w > columns) col = columns - w > 0 ? columns - w : 0;
    int row = tiles[i].row < 0 ? 0 : tiles[i].row;
    lv_obj_set_grid_cell(tiles[i].obj,
                         LV_GRID_ALIGN_STRETCH, col, w,
                         LV_GRID_ALIGN_STRETCH, row, h);
  }

  lv_obj_set_scroll_dir(root, LV_DIR_VER);
  lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace tilehaus
