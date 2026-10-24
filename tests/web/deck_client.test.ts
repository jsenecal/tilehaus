import { createDeckClient, DECK_MEDIA_TYPE, SESSION_KEY_STORAGE } from "../../web/deck_client";
import { encodeDeck, type DeckCard } from "../../web/model/deck";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}

const CARD: DeckCard = {
  type: 2, entity: "switch.fan", title: "Fan", w: 2, h: 2, entity2: "",
  icon: "", iconAlt: "", activeColor: -1, inactiveColor: -1,
  hideLabel: false, detail: false, followColor: false, showHilo: false,
  transparent: false, showWeather: true, showClock: true, tightMargins: false, align: 5, page: 0, col: 0, row: 0,
};

// A fetch stub that returns queued responses and records requests.
function stub(responses: Response[]): { fetch: typeof fetch; calls: Array<{ url: string; init: RequestInit }> } {
  const calls: Array<{ url: string; init: RequestInit }> = [];
  let i = 0;
  const fetchImpl = (async (url: string | URL | Request, init?: RequestInit) => {
    calls.push({ url: String(url), init: init ?? {} });
    const next = responses[i++];
    if (next === undefined) throw new TypeError("network down");
    return next;
  }) as unknown as typeof fetch;
  return { fetch: fetchImpl, calls };
}

const noSleep = async (): Promise<void> => {};

// In-memory Storage, optionally holding a session key already.
function memStorage(key?: string): Pick<Storage, "getItem" | "setItem" | "removeItem"> & { map: Map<string, string> } {
  const map = new Map<string, string>();
  if (key !== undefined) map.set(SESSION_KEY_STORAGE, key);
  return {
    map,
    getItem: (k) => map.get(k) ?? null,
    setItem: (k, v) => { map.set(k, v); },
    removeItem: (k) => { map.delete(k); },
  };
}

const json = (status: number, body: unknown): Response =>
  new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });

const KEY = "00112233445566778899aabbccddeeff";

export async function runDeckClientTests(): Promise<void> {
  // load: 200 with bytes + ETag
  {
    const bytes = encodeDeck([CARD]);
    const res = new Response(bytes as BodyInit, { status: 200, headers: { ETag: '"7"' } });
    const client = createDeckClient({ fetch: stub([res]).fetch, sleep: noSleep });
    const loaded = await client.load();
    assert(loaded.etag === '"7"', "load etag");
    assert(loaded.cards.length === 1 && loaded.cards[0]!.entity === "switch.fan", "load decoded");
    assert(loaded.gridCols === 10 && loaded.gridRows === 6, "load default grid size");
    assert(loaded.displayW === 1024 && loaded.displayH === 600, "load default display size (no header)");
  }

  // load: 204 empty
  {
    const res = new Response(null, { status: 204, headers: { ETag: '"0"' } });
    const client = createDeckClient({ fetch: stub([res]).fetch, sleep: noSleep });
    const loaded = await client.load();
    assert(loaded.cards.length === 0 && loaded.etag === '"0"', "load empty 204");
    assert(loaded.gridCols === 10 && loaded.gridRows === 6, "load 204 default grid size");
    assert(loaded.displayW === 1024 && loaded.displayH === 600, "load 204 default display size");
  }

  // load: 200 with X-Display-* headers set -> reported display size used
  {
    const bytes = encodeDeck([CARD]);
    const res = new Response(bytes as BodyInit, {
      status: 200,
      headers: { ETag: '"7"', "X-Display-Width": "1280", "X-Display-Height": "800" },
    });
    const client = createDeckClient({ fetch: stub([res]).fetch, sleep: noSleep });
    const loaded = await client.load();
    assert(loaded.displayW === 1280 && loaded.displayH === 800, "load reported display size from headers");
  }

  // load: the panel's newest card type, or 22 when the firmware predates the header
  {
    const withHeader = new Response(null, { status: 204, headers: { ETag: '"0"', "X-Deck-Max-Type": "25" } });
    assert((await createDeckClient({ fetch: stub([withHeader]).fetch, sleep: noSleep }).load()).maxType === 25, "max type read");
    const old = new Response(null, { status: 204, headers: { ETag: '"0"' } });
    assert((await createDeckClient({ fetch: stub([old]).fetch, sleep: noSleep }).load()).maxType === 22, "max type default");
  }

  // save: a tile newer than the panel is refused before anything is sent
  {
    const s = stub([new Response(null, { status: 204, headers: { ETag: '"5"', "X-Deck-Max-Type": "22" } })]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, storage: memStorage(KEY) });
    await client.load();
    const result = await client.save([{ ...CARD, type: 24, title: "Pasta" }], '"5"');
    assert(result.status === "error" && result.message.includes("Pasta") && result.message.includes("Timer"), "too-new tile named");
    assert(s.calls.length === 1, "nothing sent after the load");
  }

  // save: the panel's own reason comes through on a 400
  {
    const res = new Response("Deck not saved: tile 3 is card type 24", { status: 400 });
    const client = createDeckClient({ fetch: stub([res]).fetch, sleep: noSleep, storage: memStorage(KEY) });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "error" && result.message === "Deck not saved: tile 3 is card type 24", "400 reason surfaced");
  }

  // save: 204 -> reboot wait polls GET -> applied with new etag; PUT well-formed
  {
    const putRes = new Response(null, { status: 204, headers: { ETag: '"8"' } });
    const getRes = new Response(encodeDeck([CARD]) as BodyInit, { status: 200, headers: { ETag: '"8"' } });
    const s = stub([putRes, getRes]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, pollIntervalMs: 1, rebootTimeoutMs: 1000, storage: memStorage(KEY) });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "applied", "save applied");
    if (result.status === "applied") {
      assert(result.etag === '"8"', "applied new etag");
      assert(result.gridCols === 10 && result.gridRows === 6, "applied default grid size");
    }
    const put = s.calls[0]!;
    assert(put.init.method === "PUT", "used PUT");
    const headers = put.init.headers as Record<string, string>;
    assert(headers["If-Match"] === '"7"', "If-Match sent");
    assert(headers["Content-Type"] === DECK_MEDIA_TYPE, "media type sent");
    assert(put.init.body !== undefined && put.init.body !== null, "PUT has a body");
    assert(headers["Authorization"] === `Bearer ${KEY}`, "session key sent");
  }

  // save: 409 -> conflict with device etag
  {
    const res = new Response("conflict", { status: 409, headers: { ETag: '"9"' } });
    const client = createDeckClient({ fetch: stub([res]).fetch, sleep: noSleep, storage: memStorage(KEY) });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "conflict", "save conflict");
    if (result.status === "conflict") assert(result.etag === '"9"', "conflict device etag");
  }

  // save: 400 -> error
  {
    const res = new Response("bad", { status: 400 });
    const client = createDeckClient({ fetch: stub([res]).fetch, sleep: noSleep, storage: memStorage(KEY) });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "error", "save error on 400");
  }

  // save with no key: claims first, stores the key, then writes with it
  {
    const store = memStorage();
    const s = stub([
      json(201, { key: KEY }),
      new Response(null, { status: 204, headers: { ETag: '"8"' } }),
      new Response(encodeDeck([CARD]) as BodyInit, { status: 200, headers: { ETag: '"8"' } }),
    ]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, pollIntervalMs: 1, rebootTimeoutMs: 1000, storage: store });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "applied", "claim then save applied");
    assert(s.calls[0]!.url === "/api/v1/session" && s.calls[0]!.init.method === "POST", "claimed first");
    const headers = s.calls[1]!.init.headers as Record<string, string>;
    assert(headers["Authorization"] === `Bearer ${KEY}`, "claimed key used");
    assert(store.map.get(SESSION_KEY_STORAGE) === KEY, "key stored for the next save");
  }

  // save while locked: no write attempted
  {
    const s = stub([new Response("locked", { status: 423 })]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, storage: memStorage() });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "locked", "locked on claim");
    assert(s.calls.length === 1, "no PUT while locked");
  }

  // save while another client holds the session
  {
    const s = stub([new Response("taken", { status: 409 })]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, storage: memStorage() });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "taken", "taken on claim");
  }

  // a revoked key: 401, then a fresh claim and one retry
  {
    const store = memStorage("deadbeefdeadbeefdeadbeefdeadbeef");
    const s = stub([
      new Response("no", { status: 401 }),
      json(201, { key: KEY }),
      new Response(null, { status: 204, headers: { ETag: '"8"' } }),
      new Response(encodeDeck([CARD]) as BodyInit, { status: 200, headers: { ETag: '"8"' } }),
    ]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, pollIntervalMs: 1, rebootTimeoutMs: 1000, storage: store });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "applied", "re-claimed and applied");
    assert(store.map.get(SESSION_KEY_STORAGE) === KEY, "new key replaced the dead one");
  }

  // a revoked key while someone else now holds the panel: taken, key dropped
  {
    const store = memStorage("deadbeefdeadbeefdeadbeefdeadbeef");
    const s = stub([new Response("no", { status: 401 }), new Response("taken", { status: 409 })]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, storage: store });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "taken", "taken after revoke");
    assert(!store.map.has(SESSION_KEY_STORAGE), "dead key dropped");
  }

  // a write that finds the panel locked drops the key
  {
    const store = memStorage(KEY);
    const client = createDeckClient({ fetch: stub([new Response("locked", { status: 423 })]).fetch, sleep: noSleep, storage: store });
    const result = await client.reset('"7"');
    assert(result.status === "locked", "reset locked");
    assert(!store.map.has(SESSION_KEY_STORAGE), "key dropped when locked");
  }

  // session(): reports state and sends the key; a key the panel disowns is dropped
  {
    const store = memStorage(KEY);
    const s = stub([json(200, { state: "claimed", mine: true, timeoutMinutes: 30 })]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, storage: store });
    const info = await client.session();
    assert(info.state === "claimed" && info.mine && info.timeoutMinutes === 30, "session mine");
    const headers = s.calls[0]!.init.headers as Record<string, string>;
    assert(headers["Authorization"] === `Bearer ${KEY}`, "session probe sends key");
  }
  {
    const store = memStorage(KEY);
    const client = createDeckClient({ fetch: stub([json(200, { state: "open", mine: false, timeoutMinutes: 30 })]).fetch, sleep: noSleep, storage: store });
    const info = await client.session();
    assert(info.state === "open" && !info.mine, "session open");
    assert(!store.map.has(SESSION_KEY_STORAGE), "disowned key dropped");
  }
}
