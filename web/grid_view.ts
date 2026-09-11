import { cardTypeLabel } from "./card_types";
import { canPlace, moveCard, removeCard, updateCard, type EditorState } from "./editor_state";

export function createGridPane(state: EditorState, rerender: () => void): HTMLElement {
  const pane = document.createElement("div");
  pane.className = "pane grid-pane";
  const title = document.createElement("div");
  title.className = "pane-title";
  title.textContent = `Layout · ${state.cards.length} ${state.cards.length === 1 ? "tile" : "tiles"}`;
  pane.append(title);

  if (state.cards.length === 0) {
    const empty = document.createElement("div");
    empty.className = "empty";
    empty.textContent = "No tiles yet — pick a type in the toolbar and add one.";
    pane.append(empty);
    return pane;
  }

  const stage = document.createElement("div");
  stage.className = "grid-stage";
  stage.style.gridTemplateColumns = `repeat(${state.gridCols}, 1fr)`;
  pane.append(stage);

  let rows = 1;
  for (const c of state.cards) { if (c.page !== state.currentPage) continue; const b = c.row + c.h; if (b > rows) rows = b; }

  state.cards.forEach((card, index) => {
    if (card.page !== state.currentPage) return;
    const tile = document.createElement("div");
    tile.className = "tile"
      + (index === state.selected ? " selected" : "")
      + (card.row >= state.gridRows ? " below-screen" : "");
    tile.style.gridColumn = `${card.col + 1} / span ${card.w}`;
    tile.style.gridRow = `${card.row + 1} / span ${card.h}`;
    tile.draggable = false;

    const badge = document.createElement("span");
    badge.className = "t-badge";
    badge.textContent = cardTypeLabel(card.type);
    const label = document.createElement("span");
    label.className = "t-label";
    label.textContent = card.title || card.entity || "untitled";
    tile.append(badge, label);

    const del = document.createElement("button");
    del.className = "t-del";
    del.title = "Delete tile";
    del.textContent = "✕";
    del.onclick = (event) => { event.stopPropagation(); removeCard(state, index); rerender(); };
    tile.append(del);

    tile.onclick = () => { state.selected = index; rerender(); };

    tile.onpointerdown = (event) => {
      const t = event.target as HTMLElement;
      if (t.classList.contains("resize-handle") || t.classList.contains("t-del")) return;
      event.preventDefault();
      state.selected = index;
      tile.setPointerCapture(event.pointerId);
      tile.classList.add("dragging");
      const stageRect = stage.getBoundingClientRect();
      const cell = stageRect.width / state.gridCols;
      const grabCol = Math.floor((event.clientX - stageRect.left) / cell) - card.col;
      const grabRow = Math.floor((event.clientY - stageRect.top) / cell) - card.row;
      let lastCol = card.col;
      let lastRow = card.row;
      const onMove = (move: PointerEvent): void => {
        const col = Math.max(0, Math.floor((move.clientX - stageRect.left) / cell) - grabCol);
        const row = Math.max(0, Math.floor((move.clientY - stageRect.top) / cell) - grabRow);
        const clampedCol = Math.min(col, state.gridCols - card.w);
        if (clampedCol === lastCol && row === lastRow) return;
        lastCol = clampedCol; lastRow = row;
        const ok = canPlace(state.cards, index, clampedCol, row, card.w, card.h, card.page, state.gridCols);
        tile.classList.toggle("invalid", !ok);
        tile.style.gridColumn = `${clampedCol + 1} / span ${card.w}`;
        tile.style.gridRow = `${row + 1} / span ${card.h}`;
      };
      const onUp = (up: PointerEvent): void => {
        tile.releasePointerCapture(up.pointerId);
        tile.removeEventListener("pointermove", onMove);
        tile.removeEventListener("pointerup", onUp);
        tile.classList.remove("dragging", "invalid");
        moveCard(state, index, lastCol, lastRow); // no-op if invalid → reverts on rerender
        rerender();
      };
      tile.addEventListener("pointermove", onMove);
      tile.addEventListener("pointerup", onUp);
    };

    if (index === state.selected) {
      const handle = document.createElement("div");
      handle.className = "resize-handle";
      handle.title = "Drag to resize";
      handle.onpointerdown = (event) => {
        event.preventDefault();
        event.stopPropagation();
        handle.setPointerCapture(event.pointerId);
        const startX = event.clientX;
        const startY = event.clientY;
        const startW = card.w;
        const startH = card.h;
        const step = stage.clientWidth / state.gridCols;
        let w = startW;
        let h = startH;
        const onMove = (move: PointerEvent): void => {
          const nw = Math.min(state.gridCols - card.col, Math.max(1, startW + Math.round((move.clientX - startX) / step)));
          const nh = Math.max(1, startH + Math.round((move.clientY - startY) / step));
          if ((nw !== w || nh !== h) && canPlace(state.cards, index, card.col, card.row, nw, nh, card.page, state.gridCols)) {
            w = nw; h = nh;
            tile.style.gridColumn = `${card.col + 1} / span ${w}`;
            tile.style.gridRow = `${card.row + 1} / span ${h}`;
          }
        };
        const onUp = (up: PointerEvent): void => {
          handle.releasePointerCapture(up.pointerId);
          handle.removeEventListener("pointermove", onMove);
          handle.removeEventListener("pointerup", onUp);
          if (w !== startW || h !== startH) updateCard(state, index, { w, h });
          rerender();
        };
        handle.addEventListener("pointermove", onMove);
        handle.addEventListener("pointerup", onUp);
      };
      tile.append(handle);
    }

    stage.append(tile);
  });

  // Square cells (row height = column width) + the on-panel screen edge,
  // measured once the stage has a width.
  const applyMetrics = (): void => {
    const width = stage.clientWidth;
    if (width <= 0) return;
    const cell = width / state.gridCols;
    stage.style.gridAutoRows = `${cell}px`;
    let edge = stage.querySelector<HTMLElement>(".screen-edge");
    if (rows > state.gridRows) {
      if (edge === null) {
        edge = document.createElement("div");
        edge.className = "screen-edge";
        edge.textContent = "screen edge · rows below scroll on the panel";
        stage.append(edge);
      }
      edge.style.top = `${state.gridRows * cell}px`;
    } else if (edge !== null) {
      edge.remove();
    }
  };
  requestAnimationFrame(applyMetrics);

  return pane;
}
