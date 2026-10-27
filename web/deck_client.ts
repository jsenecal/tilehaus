import {
  decodeDeckDocument, encodeDeck, DECK_ACCENT_DEFAULT,
  DECK_GRID_COLS_DEFAULT, DECK_GRID_ROWS_DEFAULT, type DeckCard,
} from "./model/deck";
import { DISPLAY_W_DEFAULT, DISPLAY_H_DEFAULT } from "./model/grid_layout";
import { cardTypeLabel } from "./card_types";

export const DECK_MEDIA_TYPE = "application/vnd.tilehaus.deck";
const CONFIG_URL = "/api/v1/config";
const SESSION_URL = "/api/v1/session";
// The session key is per panel, and the configurator is served by the panel, so
// this origin-scoped key never mixes two panels up.
export const SESSION_KEY_STORAGE = "tilehaus.config.key";

/** Who may write: "locked" until the HA Configurator switch is on. */
export interface SessionInfo {
  state: "locked" | "open" | "claimed";
  mine: boolean;           // this browser holds the claimed key
  timeoutMinutes: number;  // idle minutes before the switch turns itself off
}

// Firmware older than the X-Deck-Max-Type header knows card types up to Back (22).
export const PANEL_MAX_TYPE_DEFAULT = 22;

export interface LoadResult {
  cards: DeckCard[];
  accent: number;
  pages: string[];
  pageBack: boolean[];
  gridCols: number;
  gridRows: number;
  displayW: number;
  displayH: number;
  maxType: number;  // newest card type the panel's firmware renders
  etag: string;
}

export type SaveResult =
  // live: the panel applied it in place (only looks changed), no reboot.
  | { status: "applied"; live: boolean; etag: string; cards: DeckCard[]; accent: number; pages: string[]; pageBack: boolean[]; gridCols: number; gridRows: number }
  | { status: "conflict"; etag: string }
  | { status: "locked" }   // the HA Configurator switch is off
  | { status: "taken" }    // another client holds the session
  | { status: "error"; message: string };

export interface DeckClientOptions {
  fetch?: typeof fetch;
  sleep?: (ms: number) => Promise<void>;
  pollIntervalMs?: number;
  rebootTimeoutMs?: number;
  storage?: Pick<Storage, "getItem" | "setItem" | "removeItem">;
}

export interface DeckClient {
  load(): Promise<LoadResult>;
  save(cards: DeckCard[], etag: string, accent?: number, pages?: string[], gridCols?: number, gridRows?: number, pageBack?: readonly boolean[]): Promise<SaveResult>;
  reset(etag: string): Promise<SaveResult>;
  session(): Promise<SessionInfo>;
  // Called when a write turns out to need a reboot, before the wait for it.
  onRebooting?: (() => void) | undefined;
}

export function createDeckClient(options: DeckClientOptions = {}): DeckClient {
  const doFetch = options.fetch ?? fetch;
  const sleep = options.sleep ?? ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  const pollIntervalMs = options.pollIntervalMs ?? 2000;
  const rebootTimeoutMs = options.rebootTimeoutMs ?? 60000;
  let panelMaxType = PANEL_MAX_TYPE_DEFAULT;
  const storage = options.storage ?? (typeof localStorage !== "undefined" ? localStorage : undefined);

  const readKey = (): string => {
    try { return storage?.getItem(SESSION_KEY_STORAGE) ?? ""; } catch { return ""; }
  };
  const writeKey = (key: string): void => {
    try {
      if (key === "") storage?.removeItem(SESSION_KEY_STORAGE);
      else storage?.setItem(SESSION_KEY_STORAGE, key);
    } catch { /* storage unavailable: the key lives for this page only */ }
  };
  let key = readKey();
  const setKey = (next: string): void => { key = next; writeKey(next); };
  const authHeader = (): Record<string, string> => (key !== "" ? { Authorization: `Bearer ${key}` } : {});

  async function session(): Promise<SessionInfo> {
    const response = await doFetch(SESSION_URL, { method: "GET", headers: authHeader() });
    if (!response.ok) throw new Error(`session check failed: ${response.status}`);
    const body = await response.json() as Partial<SessionInfo>;
    const state = body.state === "open" || body.state === "claimed" ? body.state : "locked";
    // A key the panel no longer honours is dead: the switch was cycled.
    if (key !== "" && body.mine !== true) setKey("");
    return { state, mine: body.mine === true, timeoutMinutes: Number(body.timeoutMinutes) || 0 };
  }

  // Claim the open panel. Only called on a write, so opening the page to look
  // never takes the session from whoever is about to save.
  async function claim(): Promise<"ok" | "locked" | "taken" | string> {
    const response = await doFetch(SESSION_URL, { method: "POST" });
    if (response.status === 201) {
      const body = await response.json() as { key?: unknown };
      if (typeof body.key !== "string" || body.key === "") return "claim returned no key";
      setKey(body.key);
      return "ok";
    }
    if (response.status === 423) return "locked";
    if (response.status === 409) return "taken";
    return `claim failed: ${response.status}`;
  }

  async function load(): Promise<LoadResult> {
    const response = await doFetch(CONFIG_URL, { method: "GET" });
    const etag = response.headers.get("ETag") ?? '"0"';
    const displayW = Number(response.headers.get("X-Display-Width")) || DISPLAY_W_DEFAULT;
    const displayH = Number(response.headers.get("X-Display-Height")) || DISPLAY_H_DEFAULT;
    const maxType = Number(response.headers.get("X-Deck-Max-Type")) || PANEL_MAX_TYPE_DEFAULT;
    panelMaxType = maxType;
    if (response.status === 204) {
      return {
        cards: [], accent: DECK_ACCENT_DEFAULT, pages: ["Home"], pageBack: [false],
        gridCols: DECK_GRID_COLS_DEFAULT, gridRows: DECK_GRID_ROWS_DEFAULT, displayW, displayH, maxType, etag,
      };
    }
    if (!response.ok) throw new Error(`load failed: ${response.status}`);
    const buffer = new Uint8Array(await response.arrayBuffer());
    const doc = decodeDeckDocument(buffer);
    return {
      cards: doc.cards, accent: doc.accent, pages: doc.pages, pageBack: doc.pageBack,
      gridCols: doc.gridCols, gridRows: doc.gridRows, displayW, displayH, maxType, etag,
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

  function send(method: "PUT" | "DELETE", body: Uint8Array | null, etag: string): Promise<Response> {
    const headers: Record<string, string> = { "If-Match": etag, ...authHeader() };
    if (body !== null) headers["Content-Type"] = DECK_MEDIA_TYPE;
    return doFetch(CONFIG_URL, {
      method,
      headers,
      ...(body !== null ? { body: body as BodyInit } : {}),
    });
  }

  async function mutate(method: "PUT" | "DELETE", body: Uint8Array | null, etag: string): Promise<SaveResult> {
    let response: Response;
    try {
      if (key === "") {
        const claimed = await claim();
        if (claimed === "locked" || claimed === "taken") return { status: claimed };
        if (claimed !== "ok") return { status: "error", message: claimed };
      }
      response = await send(method, body, etag);
      // A stored key from an earlier session that has since been revoked: the
      // panel may have been reopened, so claim afresh once and retry.
      if (response.status === 401) {
        setKey("");
        const claimed = await claim();
        if (claimed === "locked" || claimed === "taken") return { status: claimed };
        if (claimed !== "ok") return { status: "error", message: claimed };
        response = await send(method, body, etag);
      }
    } catch (error) {
      return { status: "error", message: String(error) };
    }
    if (response.status === 423) {
      setKey("");
      return { status: "locked" };
    }
    if (response.status === 401) {
      setKey("");
      return { status: "taken" };
    }
    if (response.status === 204) {
      // A deck that only changes how tiles look is applied in place.
      const live = response.headers.get("X-Deck-Applied") === "live";
      if (!live) client.onRebooting?.();
      try {
        const reloaded = live ? await load() : await waitForReboot();
        return {
          status: "applied", live, etag: reloaded.etag, cards: reloaded.cards, accent: reloaded.accent,
          pages: reloaded.pages, pageBack: reloaded.pageBack,
          gridCols: reloaded.gridCols, gridRows: reloaded.gridRows,
        };
      } catch (error) {
        return { status: "error", message: String(error) };
      }
    }
    if (response.status === 409) {
      return { status: "conflict", etag: response.headers.get("ETag") ?? etag };
    }
    // The panel's 400 says why ("Deck not saved: tile 3 is card type 24, …").
    const detail = await response.text().catch(() => "");
    return { status: "error", message: detail !== "" ? detail : `${method} failed: ${response.status}` };
  }

  async function save(
    cards: DeckCard[], etag: string, accent: number = DECK_ACCENT_DEFAULT, pages: string[] = ["Home"],
    gridCols: number = DECK_GRID_COLS_DEFAULT, gridRows: number = DECK_GRID_ROWS_DEFAULT,
    pageBack: readonly boolean[] = [],
  ): Promise<SaveResult> {
    // Refuse here what the panel would refuse, with the tile named.
    const tooNew = cards.findIndex((c) => c.type > panelMaxType);
    if (tooNew >= 0) {
      const c = cards[tooNew]!;
      return {
        status: "error",
        message: `“${c.title || cardTypeLabel(c.type)}” is a ${cardTypeLabel(c.type)} tile, which this panel's firmware doesn't have yet. Update the panel's firmware, or remove that tile, then save.`,
      };
    }
    let body: Uint8Array;
    try {
      body = encodeDeck(cards, accent, pages, gridCols, gridRows, pageBack);
    } catch (error) {
      return { status: "error", message: String(error) };
    }
    return mutate("PUT", body, etag);
  }

  function reset(etag: string): Promise<SaveResult> {
    return mutate("DELETE", null, etag);
  }

  const client: DeckClient = { load, save, reset, session };
  return client;
}
