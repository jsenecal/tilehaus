// Mirrors poc/native/grid_layout.h::place_tiles. GRID_COLUMNS / VISIBLE_ROWS must
// match the POC YAML (grid_cols 5 × subdivisions 2 = 10; grid_rows 3 × 2 = 6).
export const GRID_COLUMNS = 10;
export const VISIBLE_ROWS = 6;

export const MIN_CELL_PX = 78;          // icon glyph (~46) + 2x tile inset (16)
export const DISPLAY_W_DEFAULT = 1024;
export const DISPLAY_H_DEFAULT = 600;

export function gridLimits(displayW: number, displayH: number): { maxCols: number; maxRows: number } {
  const w = displayW > 0 ? displayW : DISPLAY_W_DEFAULT;
  const h = displayH > 0 ? displayH : DISPLAY_H_DEFAULT;
  return { maxCols: Math.max(2, Math.floor(w / MIN_CELL_PX)), maxRows: Math.max(2, Math.floor(h / MIN_CELL_PX)) };
}

export interface Placement {
  col: number;
  row: number;
}

export interface LayoutResult {
  placements: Placement[];
  rows: number;
}

export function placeTiles(
  sizes: ReadonlyArray<{ w: number; h: number }>,
  columns: number = GRID_COLUMNS,
): LayoutResult {
  const cols = columns < 1 ? 1 : columns;
  const placements: Placement[] = [];
  let occ: boolean[] = [];
  let rows = 0;

  const ensureRows = (need: number): void => {
    if (need > rows) {
      occ = occ.concat(new Array<boolean>((need - rows) * cols).fill(false));
      rows = need;
    }
  };
  const fits = (c: number, r: number, w: number, h: number): boolean => {
    for (let rr = r; rr < r + h; rr += 1) {
      for (let cc = c; cc < c + w; cc += 1) {
        if (occ[rr * cols + cc]) return false;
      }
    }
    return true;
  };
  const mark = (c: number, r: number, w: number, h: number): void => {
    for (let rr = r; rr < r + h; rr += 1) {
      for (let cc = c; cc < c + w; cc += 1) {
        occ[rr * cols + cc] = true;
      }
    }
  };

  for (const size of sizes) {
    const w = size.w < 1 ? 1 : (size.w > cols ? cols : size.w);
    const h = size.h < 1 ? 1 : size.h;
    let pr = 0;
    let pc = 0;
    let placed = false;
    for (let r = 0; !placed; r += 1) {
      ensureRows(r + h);
      for (let c = 0; c + w <= cols; c += 1) {
        if (fits(c, r, w, h)) { pr = r; pc = c; placed = true; break; }
      }
    }
    mark(pc, pr, w, h);
    placements.push({ col: pc, row: pr });
  }
  return { placements, rows };
}
