import { GRID_COLUMNS, placeTiles } from "../../web/layout";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}
function deepEqual(actual: unknown, expected: unknown, message: string): void {
  const a = JSON.stringify(actual);
  const e = JSON.stringify(expected);
  if (a !== e) throw new Error(`${message}: expected ${e}, received ${a}`);
}

export function runLayoutTests(): void {
  assert(GRID_COLUMNS === 10, "10 columns");

  deepEqual(placeTiles([{ w: 2, h: 2 }, { w: 2, h: 2 }]).placements,
    [{ col: 0, row: 0 }, { col: 2, row: 0 }], "two 2x2 side by side");

  {
    const r = placeTiles([{ w: 10, h: 2 }, { w: 2, h: 2 }, { w: 2, h: 2 }]);
    deepEqual(r.placements, [{ col: 0, row: 0 }, { col: 0, row: 2 }, { col: 2, row: 2 }], "header then row2");
    assert(r.rows === 4, "header deck rows");
  }

  deepEqual(placeTiles([{ w: 2, h: 1 }, { w: 2, h: 1 }, { w: 6, h: 1 }]).placements,
    [{ col: 0, row: 0 }, { col: 2, row: 0 }, { col: 4, row: 0 }], "fills to the edge");

  deepEqual(placeTiles([{ w: 2, h: 1 }, { w: 2, h: 1 }, { w: 2, h: 1 }, { w: 6, h: 1 }]).placements,
    [{ col: 0, row: 0 }, { col: 2, row: 0 }, { col: 4, row: 0 }, { col: 0, row: 1 }], "wraps when it does not fit");

  deepEqual(placeTiles([{ w: 99, h: 1 }]).placements, [{ col: 0, row: 0 }], "width clamps to columns");
}
