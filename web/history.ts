import { type DeckCard } from "./model/deck";

/** The part of the editor state that undo restores: the deck itself, not the view. */
export interface DeckSnapshot {
  cards: DeckCard[];
  pages: string[];
  pageBack: boolean[];  // per page: the panel draws a Back button
  accent: number;
  gridCols: number;
  gridRows: number;
}

export interface History {
  /** Record the deck if it changed since the last call. Cheap; call it often. */
  checkpoint(current: DeckSnapshot): void;
  /** The deck to restore, or null when there is nothing to undo. */
  undo(): DeckSnapshot | null;
  redo(): DeckSnapshot | null;
  canUndo(): boolean;
  canRedo(): boolean;
}

export interface HistoryOptions {
  now?: () => number;
  /** Changes closer together than this are one step (typing, dragging a colour). */
  coalesceMs?: number;
  limit?: number;
}

const serialize = (s: DeckSnapshot): string =>
  JSON.stringify([s.cards, s.pages, s.pageBack, s.accent, s.gridCols, s.gridRows]);

const parse = (text: string): DeckSnapshot => {
  const [cards, pages, pageBack, accent, gridCols, gridRows] =
    JSON.parse(text) as [DeckCard[], string[], boolean[], number, number, number];
  return { cards, pages, pageBack, accent, gridCols, gridRows };
};

// Snapshot-based: rather than every mutation recording an inverse, the editor
// checkpoints the whole deck after each event and history keeps the decks that
// differ. A 128-card deck serializes in well under a millisecond, and nothing
// that edits the state can forget to tell history about it.
export function createHistory(options: HistoryOptions = {}): History {
  const now = options.now ?? (() => Date.now());
  const coalesceMs = options.coalesceMs ?? 800;
  const limit = options.limit ?? 100;
  const undoStack: string[] = [];
  const redoStack: string[] = [];
  let baseline: string | null = null;  // the deck as last recorded
  let lastChangeAt = Number.NEGATIVE_INFINITY;

  function checkpoint(current: DeckSnapshot): void {
    const next = serialize(current);
    if (baseline === null) { baseline = next; return; }  // the loaded deck is the floor
    if (next === baseline) return;
    const t = now();
    // The first change of a burst saves the deck as it was before the burst.
    if (t - lastChangeAt > coalesceMs) {
      undoStack.push(baseline);
      if (undoStack.length > limit) undoStack.shift();
    }
    baseline = next;
    lastChangeAt = t;
    redoStack.length = 0;
  }

  function step(from: string[], to: string[]): DeckSnapshot | null {
    const target = from.pop();
    if (target === undefined || baseline === null) return null;
    to.push(baseline);
    baseline = target;
    lastChangeAt = Number.NEGATIVE_INFINITY;  // the next edit starts a new step
    return parse(target);
  }

  return {
    checkpoint,
    undo: () => step(undoStack, redoStack),
    redo: () => step(redoStack, undoStack),
    canUndo: () => undoStack.length > 0,
    canRedo: () => redoStack.length > 0,
  };
}
