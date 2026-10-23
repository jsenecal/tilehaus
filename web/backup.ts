import { encodeDeck, DECK_ACCENT_DEFAULT, DECK_GRID_COLS_DEFAULT, DECK_GRID_ROWS_DEFAULT, type DeckCard } from "./model/deck";
import { defaultCard } from "./editor_state";
import { type DeckSnapshot } from "./history";

// Backups are the same JSON spec scripts/deck_cli.js pushes, so a file exported
// here can be pushed from the CLI, and a CLI spec can be imported here.

export function exportSpec(deck: DeckSnapshot): string {
  return JSON.stringify({
    pages: deck.pages,
    gridCols: deck.gridCols,
    gridRows: deck.gridRows,
    accent: deck.accent,
    cards: deck.cards,
  }, null, 2) + "\n";
}

/** "tilehaus-office-2026-10-23.json", from the panel's host name. */
export function backupFileName(host: string, date: Date): string {
  const name = host.replace(/\.local$/, "").replace(/[^a-zA-Z0-9-]+/g, "-").replace(/^-+|-+$/g, "") || "tilehaus";
  const pad = (n: number): string => String(n).padStart(2, "0");
  return `${name}-${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}.json`;
}

export class BackupError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "BackupError";
  }
}

const isObject = (v: unknown): v is Record<string, unknown> => typeof v === "object" && v !== null && !Array.isArray(v);

// Parse and fully validate a spec. Cards may leave out fields, which take the
// same defaults as a freshly added tile; anything the panel would reject fails
// here, with the reason, rather than on Save.
export function parseSpec(text: string): DeckSnapshot {
  let raw: unknown;
  try {
    raw = JSON.parse(text);
  } catch {
    throw new BackupError("This file isn't valid JSON.");
  }
  const obj = isObject(raw) ? raw : null;
  const rawCards = Array.isArray(raw) ? raw : obj?.["cards"];
  if (!Array.isArray(rawCards)) throw new BackupError("This file has no list of tiles (a \"cards\" array).");

  const pages = obj?.["pages"] ?? ["Home"];
  if (!Array.isArray(pages) || pages.length === 0 || !pages.every((p) => typeof p === "string")) {
    throw new BackupError("\"pages\" must be a list of page names.");
  }
  const num = (key: string, fallback: number): number => {
    const v = obj?.[key];
    if (v === undefined) return fallback;
    if (typeof v !== "number") throw new BackupError(`"${key}" must be a number.`);
    return v;
  };
  const accent = num("accent", DECK_ACCENT_DEFAULT);
  const gridCols = num("gridCols", DECK_GRID_COLS_DEFAULT);
  const gridRows = num("gridRows", DECK_GRID_ROWS_DEFAULT);

  const cards: DeckCard[] = rawCards.map((c, i) => {
    if (!isObject(c) || typeof c["type"] !== "number") throw new BackupError(`Tile ${i + 1} has no card type.`);
    // Only the fields a card has: unknown keys would ride along into the next export.
    const card = defaultCard(c["type"]);
    const fields = card as unknown as Record<string, unknown>;
    for (const key of Object.keys(fields)) if (key in c) fields[key] = c[key];
    if (!Number.isInteger(card.page) || card.page < 0 || card.page >= pages.length) {
      throw new BackupError(`Tile ${i + 1} is on page ${card.page}, but the file has ${pages.length} page(s).`);
    }
    return card;
  });

  try {
    encodeDeck(cards, accent, pages as string[], gridCols, gridRows);
  } catch (error) {
    throw new BackupError(`The panel would reject this file: ${error instanceof Error ? error.message : String(error)}.`);
  }
  return { cards, pages: pages as string[], accent, gridCols, gridRows };
}
