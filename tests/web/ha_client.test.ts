import { createHaClient } from "../../web/ha_client";

function assert(cond: boolean, message: string): void {
  if (!cond) throw new Error(message);
}

function fakeStorage(): Pick<Storage, "getItem" | "setItem"> {
  const map = new Map<string, string>();
  return {
    getItem: (k: string): string | null => map.get(k) ?? null,
    setItem: (k: string, v: string): void => { map.set(k, v); },
  };
}

function stub(responder: (url: string) => Response): { fetch: typeof fetch; calls: Array<{ url: string; init: RequestInit }> } {
  const calls: Array<{ url: string; init: RequestInit }> = [];
  const fetchImpl = (async (url: string | URL | Request, init?: RequestInit) => {
    calls.push({ url: String(url), init: init ?? {} });
    return responder(String(url));
  }) as unknown as typeof fetch;
  return { fetch: fetchImpl, calls };
}

export async function runHaClientTests(): Promise<void> {
  {
    const body = JSON.stringify([
      { entity_id: "light.b", attributes: { friendly_name: "Lamp B", icon: "mdi:lightbulb" } },
      { entity_id: "light.a", attributes: { friendly_name: "Lamp A" } },
      { entity_id: "light.a", attributes: { friendly_name: "dupe" } },
      { foo: 1 },
    ]);
    const s = stub(() => new Response(body, { status: 200 }));
    const storage = fakeStorage();
    const ha = createHaClient({ fetch: s.fetch, storage, origin: "http://panel" });
    await ha.connect("http://ha:8123/", "TOK");
    const st = ha.status();
    assert(st.state === "connected", "connected");
    if (st.state === "connected") assert(st.count === 2, "unique count 2");
    assert(JSON.stringify(ha.entities()) === JSON.stringify(["light.a", "light.b"]), "sorted unique ids");
    assert(s.calls[0]!.url === "http://ha:8123/api/states", "trailing slash trimmed");
    const headers = s.calls[0]!.init.headers as Record<string, string>;
    assert(headers["Authorization"] === "Bearer TOK", "bearer token sent");
    assert(storage.getItem("epc.ha.baseUrl") === "http://ha:8123", "base persisted");
    assert(storage.getItem("epc.ha.token") === "TOK", "token persisted");
    // Per-entity metadata is captured from attributes for autofill.
    assert(ha.meta("light.b")?.friendlyName === "Lamp B", "friendly name captured");
    assert(ha.meta("light.b")?.icon === "mdi:lightbulb", "icon captured");
    assert(ha.meta("light.a")?.icon === "", "missing icon defaults empty");
    assert(ha.meta("nope") === undefined, "unknown entity has no meta");
  }

  {
    const s = stub(() => new Response("no", { status: 401 }));
    const ha = createHaClient({ fetch: s.fetch, storage: fakeStorage(), origin: "http://panel" });
    await ha.connect("http://ha", "bad");
    const st = ha.status();
    assert(st.state === "error" && /token/i.test(st.message), "401 token error");
    assert(ha.entities().length === 0, "no entities after error");
  }

  {
    const s = stub(() => { throw new TypeError("Failed to fetch"); });
    const storage = fakeStorage();
    const ha = createHaClient({ fetch: s.fetch, storage, origin: "http://192.168.1.9" });
    await ha.connect("http://ha:8123/", "TOK");
    const st = ha.status();
    assert(st.state === "error", "network error state");
    if (st.state === "error") {
      assert(st.message.includes("http://192.168.1.9"), "origin in message");
      assert(/cors_allowed_origins/.test(st.message), "cors guidance in message");
    }
    // Creds must persist even when the connection fails (the first-run CORS
    // case), so a reload keeps the fields filled and can auto-retry.
    assert(storage.getItem("epc.ha.baseUrl") === "http://ha:8123", "base persisted on error");
    assert(storage.getItem("epc.ha.token") === "TOK", "token persisted on error");
  }

  {
    const storage = fakeStorage();
    storage.setItem("epc.ha.baseUrl", "http://saved:8123");
    storage.setItem("epc.ha.token", "SAVED");
    const ha = createHaClient({ fetch: stub(() => new Response("[]", { status: 200 })).fetch, storage, origin: "http://panel" });
    assert(ha.creds().baseUrl === "http://saved:8123" && ha.creds().token === "SAVED", "creds loaded from storage");
  }
}
