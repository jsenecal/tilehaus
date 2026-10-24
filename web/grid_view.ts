import { cardTypeLabel } from "./card_types";
import { canPlace, moveCard, removeCard, updateCard, type EditorState } from "./editor_state";
import { type HaClient } from "./ha_client";
import { PANEL, hex, renderTileFace } from "./tile_preview";

// The preview: the panel's screen at its real resolution, with tiles placed by
// the firmware's own grid arithmetic (16px padding, 10px gaps), then scaled as
// a whole to fit the page. Sizes inside are panel pixels, so what is drawn here
// is what the glass shows; pointer positions are divided by the scale.

const PAD = 16;
const GAP = 10;

export interface PanelGrid { cellW: number; cellH: number; }

/** The firmware's grid_metrics(): cell size for a deck's columns and visible rows. */
export function panelGrid(displayW: number, displayH: number, cols: number, rows: number): PanelGrid {
  return {
    cellW: Math.max(1, Math.floor((displayW - 2 * PAD - (cols - 1) * GAP) / cols)),
    cellH: Math.max(1, Math.floor((displayH - 2 * PAD - (rows - 1) * GAP) / rows)),
  };
}

export interface PreviewOptions {
  ha: HaClient;
  night: boolean;
}

export function createGridPane(state: EditorState, rerender: () => void, opts: PreviewOptions): HTMLElement {
  const g = panelGrid(state.displayW, state.displayH, state.gridCols, state.gridRows);
  const x = (col: number): number => PAD + col * (g.cellW + GAP);
  const y = (row: number): number => PAD + row * (g.cellH + GAP);
  const span = (n: number, cell: number): number => n * cell + (n - 1) * GAP;

  const onPage = state.cards.map((c, i) => [c, i] as const).filter(([c]) => c.page === state.currentPage);
  const rowsUsed = onPage.reduce((m, [c]) => Math.max(m, c.row + c.h), 0);
  const screenH = Math.max(state.displayH, y(rowsUsed) - GAP + PAD);

  const frame = document.createElement("div");
  frame.className = "pv-frame";
  const viewport = document.createElement("div");
  viewport.className = "pv-viewport";
  const screen = document.createElement("div");
  screen.className = "pv-screen";
  screen.style.width = `${state.displayW}px`;
  screen.style.height = `${screenH}px`;
  screen.style.background = hex(opts.night ? PANEL.nightPage : PANEL.page);
  viewport.append(screen);
  frame.append(viewport);

  let scale = 1;
  const fit = (): void => {
    const w = viewport.clientWidth;
    if (w <= 0) return;
    scale = w / state.displayW;
    screen.style.transform = `scale(${scale})`;
    viewport.style.height = `${screenH * scale}px`;
  };
  new ResizeObserver(fit).observe(viewport);
  requestAnimationFrame(fit);

  // Where the panel's screen ends: rows below it scroll on the panel.
  if (rowsUsed > state.gridRows) {
    const edge = document.createElement("div");
    edge.className = "pv-edge";
    edge.style.top = `${state.displayH}px`;
    const label = document.createElement("span");
    label.textContent = "SCREEN EDGE · ROWS BELOW SCROLL ON THE PANEL";
    edge.append(label);
    screen.append(edge);
  }

  if (onPage.length === 0) {
    const empty = document.createElement("div");
    empty.className = "pv-empty";
    empty.textContent = "No tiles on this page yet. Pick a type above and add one.";
    screen.append(empty);
  }

  const toPanel = (cx: number, cy: number): [number, number] => {
    const r = screen.getBoundingClientRect();
    return [(cx - r.left) / scale, (cy - r.top) / scale];
  };
  const colAt = (px: number): number => Math.floor((px - PAD + GAP / 2) / (g.cellW + GAP));
  const rowAt = (py: number): number => Math.floor((py - PAD + GAP / 2) / (g.cellH + GAP));

  const haUp = opts.ha.status().state === "connected";
  const now = new Date();

  for (const [card, index] of onPage) {
    const tile = document.createElement("div");
    const unknown = haUp && card.entity !== "" && card.entity.includes(".") && opts.ha.meta(card.entity) === undefined;
    tile.className = "pv-tile"
      + (index === state.selected ? " selected" : "")
      + (unknown ? " unknown" : "");
    const place = (col: number, row: number, w: number, h: number): void => {
      tile.style.left = `${x(col)}px`;
      tile.style.top = `${y(row)}px`;
      tile.style.width = `${span(w, g.cellW)}px`;
      tile.style.height = `${span(h, g.cellH)}px`;
    };
    place(card.col, card.row, card.w, card.h);
    const meta = opts.ha.meta(card.entity);
    renderTileFace(tile, card, meta, {
      w: span(card.w, g.cellW), h: span(card.h, g.cellH), accent: state.accent, night: opts.night, now,
    });
    tile.title = `${cardTypeLabel(card.type)}${card.entity ? " · " + card.entity : ""}`;

    if (unknown) {
      const flag = document.createElement("div");
      flag.className = "pv-flag";
      flag.textContent = "UNKNOWN ENTITY";
      tile.append(flag);
    }

    tile.onclick = () => { state.selected = index; rerender(); };

    tile.onpointerdown = (event) => {
      const t = event.target as HTMLElement;
      if (t.closest(".pv-handle, .pv-del")) return;
      event.preventDefault();
      state.selected = index;
      tile.setPointerCapture(event.pointerId);
      tile.classList.add("dragging");
      const [px, py] = toPanel(event.clientX, event.clientY);
      const grabCol = colAt(px) - card.col;
      const grabRow = rowAt(py) - card.row;
      let lastCol = card.col, lastRow = card.row;
      const onMove = (move: PointerEvent): void => {
        const [mx, my] = toPanel(move.clientX, move.clientY);
        const col = Math.max(0, Math.min(state.gridCols - card.w, colAt(mx) - grabCol));
        const row = Math.max(0, rowAt(my) - grabRow);
        if (col === lastCol && row === lastRow) return;
        lastCol = col; lastRow = row;
        tile.classList.toggle("invalid", !canPlace(state.cards, index, col, row, card.w, card.h, card.page, state.gridCols));
        place(col, row, card.w, card.h);
      };
      const onUp = (up: PointerEvent): void => {
        tile.releasePointerCapture(up.pointerId);
        tile.removeEventListener("pointermove", onMove);
        tile.removeEventListener("pointerup", onUp);
        tile.classList.remove("dragging", "invalid");
        moveCard(state, index, lastCol, lastRow);  // refuses an overlap: the rerender puts it back
        rerender();
      };
      tile.addEventListener("pointermove", onMove);
      tile.addEventListener("pointerup", onUp);
    };

    if (index === state.selected) {
      const del = document.createElement("button");
      del.className = "pv-del";
      del.title = "Delete tile";
      del.textContent = "✕";
      del.onclick = (event) => { event.stopPropagation(); removeCard(state, index); rerender(); };
      tile.append(del);

      const handle = document.createElement("div");
      handle.className = "pv-handle";
      handle.title = "Drag to resize";
      handle.onpointerdown = (event) => {
        event.preventDefault();
        event.stopPropagation();
        handle.setPointerCapture(event.pointerId);
        const [sx, sy] = toPanel(event.clientX, event.clientY);
        let w = card.w, h = card.h;
        const onMove = (move: PointerEvent): void => {
          const [mx, my] = toPanel(move.clientX, move.clientY);
          const nw = Math.min(state.gridCols - card.col, Math.max(1, card.w + Math.round((mx - sx) / (g.cellW + GAP))));
          const nh = Math.max(1, card.h + Math.round((my - sy) / (g.cellH + GAP)));
          if ((nw !== w || nh !== h) && canPlace(state.cards, index, card.col, card.row, nw, nh, card.page, state.gridCols)) {
            w = nw; h = nh;
            place(card.col, card.row, w, h);
          }
        };
        const onUp = (up: PointerEvent): void => {
          handle.releasePointerCapture(up.pointerId);
          handle.removeEventListener("pointermove", onMove);
          handle.removeEventListener("pointerup", onUp);
          if (w !== card.w || h !== card.h) updateCard(state, index, { w, h });
          rerender();
        };
        handle.addEventListener("pointermove", onMove);
        handle.addEventListener("pointerup", onUp);
      };
      tile.append(handle);
    }
    screen.append(tile);
  }
  return frame;
}
