import {
  decodeDeckDocument, encodeDeck, DECK_ACCENT_DEFAULT,
  DECK_GRID_COLS_DEFAULT, DECK_GRID_ROWS_DEFAULT, type DeckCard,
} from "./model/deck";
import { DISPLAY_W_DEFAULT, DISPLAY_H_DEFAULT } from "./model/grid_layout";

export const DECK_MEDIA_TYPE = "application/vnd.tilehaus.deck";
const CONFIG_URL = "/api/v1/config";

export interface LoadResult {
  cards: DeckCard[];
  accent: number;
  pages: string[];
  gridCols: number;
  gridRows: number;
  displayW: number;
  displayH: number;
  etag: string;
}

export type SaveResult =
  | { status: "applied"; etag: string; cards: DeckCard[]; accent: number; pages: string[]; gridCols: number; gridRows: number }
  | { status: "conflict"; etag: string }
  | { status: "error"; message: string };

export interface DeckClientOptions {
  fetch?: typeof fetch;
  sleep?: (ms: number) => Promise<void>;
  pollIntervalMs?: number;
  rebootTimeoutMs?: number;
}

export interface DeckClient {
  load(): Promise<LoadResult>;
  save(cards: DeckCard[], etag: string, accent?: number, pages?: string[], gridCols?: number, gridRows?: number): Promise<SaveResult>;
  reset(etag: string): Promise<SaveResult>;
}

export function createDeckClient(options: DeckClientOptions = {}): DeckClient {
  const doFetch = options.fetch ?? fetch;
  const sleep = options.sleep ?? ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  const pollIntervalMs = options.pollIntervalMs ?? 2000;
  const rebootTimeoutMs = options.rebootTimeoutMs ?? 60000;

  async function load(): Promise<LoadResult> {
    const response = await doFetch(CONFIG_URL, { method: "GET" });
    const etag = response.headers.get("ETag") ?? '"0"';
    const displayW = Number(response.headers.get("X-Display-Width")) || DISPLAY_W_DEFAULT;
    const displayH = Number(response.headers.get("X-Display-Height")) || DISPLAY_H_DEFAULT;
    if (response.status === 204) {
      return {
        cards: [], accent: DECK_ACCENT_DEFAULT, pages: ["Home"],
        gridCols: DECK_GRID_COLS_DEFAULT, gridRows: DECK_GRID_ROWS_DEFAULT, displayW, displayH, etag,
      };
    }
    if (!response.ok) throw new Error(`load failed: ${response.status}`);
    const buffer = new Uint8Array(await response.arrayBuffer());
    const doc = decodeDeckDocument(buffer);
    return {
      cards: doc.cards, accent: doc.accent, pages: doc.pages,
      gridCols: doc.gridCols, gridRows: doc.gridRows, displayW, displayH, etag,
    };
  }

  // Poll GET until the panel is back after a config-apply reboot.
  async function waitForReboot(): Promise<LoadResult> {
    const deadline = Date.now() + rebootTimeoutMs;
    await sleep(pollIntervalMs); // let it actually go down first
    while (Date.now() < deadline) {
      try {
        return await load();
      } catch {
        // still down / not ready — keep polling
      }
      await sleep(pollIntervalMs);
    }
    throw new Error("panel did not come back after reboot");
  }

  async function mutate(method: "PUT" | "DELETE", body: Uint8Array | null, etag: string): Promise<SaveResult> {
    let response: Response;
    try {
      const headers: Record<string, string> = { "If-Match": etag };
      if (body !== null) headers["Content-Type"] = DECK_MEDIA_TYPE;
      response = await doFetch(CONFIG_URL, {
        method,
        headers,
        ...(body !== null ? { body: body as BodyInit } : {}),
      });
    } catch (error) {
      return { status: "error", message: String(error) };
    }
    if (response.status === 204) {
      try {
        const reloaded = await waitForReboot();
        return {
          status: "applied", etag: reloaded.etag, cards: reloaded.cards, accent: reloaded.accent,
          pages: reloaded.pages, gridCols: reloaded.gridCols, gridRows: reloaded.gridRows,
        };
      } catch (error) {
        return { status: "error", message: String(error) };
      }
    }
    if (response.status === 409) {
      return { status: "conflict", etag: response.headers.get("ETag") ?? etag };
    }
    return { status: "error", message: `${method} failed: ${response.status}` };
  }

  async function save(
    cards: DeckCard[], etag: string, accent: number = DECK_ACCENT_DEFAULT, pages: string[] = ["Home"],
    gridCols: number = DECK_GRID_COLS_DEFAULT, gridRows: number = DECK_GRID_ROWS_DEFAULT,
  ): Promise<SaveResult> {
    let body: Uint8Array;
    try {
      body = encodeDeck(cards, accent, pages, gridCols, gridRows);
    } catch (error) {
      return { status: "error", message: String(error) };
    }
    return mutate("PUT", body, etag);
  }

  function reset(etag: string): Promise<SaveResult> {
    return mutate("DELETE", null, etag);
  }

  return { load, save, reset };
}
