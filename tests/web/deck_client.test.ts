import { createDeckClient, DECK_MEDIA_TYPE } from "../../web/deck_client";
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

  // save: 204 -> reboot wait polls GET -> applied with new etag; PUT well-formed
  {
    const putRes = new Response(null, { status: 204, headers: { ETag: '"8"' } });
    const getRes = new Response(encodeDeck([CARD]) as BodyInit, { status: 200, headers: { ETag: '"8"' } });
    const s = stub([putRes, getRes]);
    const client = createDeckClient({ fetch: s.fetch, sleep: noSleep, pollIntervalMs: 1, rebootTimeoutMs: 1000 });
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
  }

  // save: 409 -> conflict with device etag
  {
    const res = new Response("conflict", { status: 409, headers: { ETag: '"9"' } });
    const client = createDeckClient({ fetch: stub([res]).fetch, sleep: noSleep });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "conflict", "save conflict");
    if (result.status === "conflict") assert(result.etag === '"9"', "conflict device etag");
  }

  // save: 400 -> error
  {
    const res = new Response("bad", { status: 400 });
    const client = createDeckClient({ fetch: stub([res]).fetch, sleep: noSleep });
    const result = await client.save([CARD], '"7"');
    assert(result.status === "error", "save error on 400");
  }
}
