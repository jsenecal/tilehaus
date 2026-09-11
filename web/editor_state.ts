import {
  type DeckCard, DECK_MAX_CARD_COUNT, DECK_ACCENT_DEFAULT, DECK_ALIGN_DEFAULT,
  DECK_GRID_COLS_DEFAULT, DECK_GRID_ROWS_DEFAULT,
} from "./model/deck";
import { DISPLAY_W_DEFAULT, DISPLAY_H_DEFAULT } from "./model/grid_layout";
import { placeTiles } from "./layout";

export interface EditorState {
  cards: DeckCard[];
  accent: number;        // deck accent colour (0xRRGGBB) or -1 for built-in
  pages: string[];
  currentPage: number;
  gridCols: number;
  gridRows: number;
  displayW: number;
  displayH: number;
  etag: string;
  selected: number | null;
}

export function defaultCard(type: number): DeckCard {
  const isBack = type === 22;  // Back tile: small nav tile, chevron icon
  return {
    type, entity: "", title: "",
    w: isBack ? 1 : 2, h: isBack ? 1 : 2,
    entity2: "", icon: isBack ? "\u{F0141}" : "", iconAlt: "",
    activeColor: -1, inactiveColor: -1, hideLabel: false, detail: false, followColor: false,
    showHilo: false, transparent: false, showWeather: true, showClock: true,
    tightMargins: false, align: DECK_ALIGN_DEFAULT, page: 0, col: 0, row: 0,
  };
}

export function createEditorState(): EditorState {
  return {
    cards: [], accent: DECK_ACCENT_DEFAULT, pages: ["Home"], currentPage: 0,
    gridCols: DECK_GRID_COLS_DEFAULT, gridRows: DECK_GRID_ROWS_DEFAULT,
    displayW: DISPLAY_W_DEFAULT, displayH: DISPLAY_H_DEFAULT,
    etag: '"0"', selected: null,
  };
}

export function canPlace(
  cards: readonly DeckCard[], index: number, col: number, row: number, w: number, h: number, page: number,
  cols: number,
): boolean {
  if (col < 0 || row < 0 || w < 1 || h < 1 || col + w > cols) return false;
  for (let i = 0; i < cards.length; i += 1) {
    if (i === index) continue;
    const o = cards[i];
    if (o === undefined || o.page !== page) continue;
    const overlapX = col < o.col + o.w && o.col < col + w;
    const overlapY = row < o.row + o.h && o.row < row + h;
    if (overlapX && overlapY) return false;
  }
  return true;
}

export function addCard(state: EditorState, type: number): void {
  if (state.cards.length >= DECK_MAX_CARD_COUNT) return;
  const card = defaultCard(type);
  card.page = state.currentPage;
  for (let row = 0; ; row += 1) {
    let placed = false;
    for (let col = 0; col + card.w <= state.gridCols; col += 1) {
      if (canPlace(state.cards, -1, col, row, card.w, card.h, state.currentPage, state.gridCols)) {
        card.col = col; card.row = row; placed = true; break;
      }
    }
    if (placed) break;
  }
  state.cards.push(card);
  state.selected = state.cards.length - 1;
}

export function removeCard(state: EditorState, index: number): void {
  if (index < 0 || index >= state.cards.length) return;
  state.cards.splice(index, 1);
  if (state.cards.length === 0) {
    state.selected = null;
  } else if (state.selected !== null && state.selected >= state.cards.length) {
    state.selected = state.cards.length - 1;
  }
}

export function moveCard(state: EditorState, index: number, col: number, row: number): void {
  const current = state.cards[index];
  if (current === undefined) return;
  if (!canPlace(state.cards, index, col, row, current.w, current.h, current.page, state.gridCols)) return;
  state.cards[index] = { ...current, col, row };
  state.selected = index;
}

export function updateCard(state: EditorState, index: number, patch: Partial<DeckCard>): void {
  const current = state.cards[index];
  if (current === undefined) return;
  state.cards[index] = { ...current, ...patch };
}

export function autoArrange(state: EditorState): void {
  const onPage = state.cards.map((c, i) => [c, i] as const).filter(([c]) => c.page === state.currentPage);
  const placements = placeTiles(onPage.map(([c]) => ({ w: c.w, h: c.h })), state.gridCols).placements;
  onPage.forEach(([, i], k) => {
    const p = placements[k];
    if (p !== undefined) state.cards[i] = { ...state.cards[i]!, col: p.col, row: p.row };
  });
}

export function addPage(state: EditorState, name: string): void {
  state.pages.push(name !== "" ? name : `Page ${state.pages.length}`);
  state.currentPage = state.pages.length - 1;
  state.selected = null;
}

export function renamePage(state: EditorState, index: number, name: string): void {
  const old = state.pages[index];
  if (old === undefined || index === 0 || name === "") return;
  state.pages[index] = name;
  for (const c of state.cards) if (c.type === 21 && c.entity2 === old) c.entity2 = name;
}

export function deletePage(state: EditorState, index: number): void {
  if (index <= 0 || index >= state.pages.length) return;
  const name = state.pages[index];
  state.cards = state.cards
    .filter((c) => c.page !== index)
    .map((c) => ({
      ...c,
      page: c.page > index ? c.page - 1 : c.page,
      entity2: c.type === 21 && c.entity2 === name ? "" : c.entity2,
    }));
  state.pages.splice(index, 1);
  state.currentPage = 0;
  state.selected = null;
}
