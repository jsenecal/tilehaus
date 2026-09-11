export interface HaCreds {
  baseUrl: string;
  token: string;
}

export type HaStatus =
  | { state: "idle" }
  | { state: "connecting" }
  | { state: "connected"; count: number }
  | { state: "error"; message: string };

export interface EntityMeta {
  friendlyName: string;
  icon: string;
  deviceClass: string;
}

export interface HaClient {
  creds(): HaCreds;
  entities(): string[];
  meta(entityId: string): EntityMeta | undefined;
  status(): HaStatus;
  connect(baseUrl: string, token: string): Promise<void>;
}

const KEY_BASE = "epc.ha.baseUrl";
const KEY_TOKEN = "epc.ha.token";

export function createHaClient(options: {
  fetch?: typeof fetch;
  storage?: Pick<Storage, "getItem" | "setItem">;
  origin?: string;
} = {}): HaClient {
  const doFetch = options.fetch ?? fetch;
  const origin = options.origin
    ?? (typeof window !== "undefined" ? window.location.origin : "");
  const storage: Pick<Storage, "getItem" | "setItem"> | undefined =
    options.storage ?? (typeof localStorage !== "undefined" ? localStorage : undefined);

  const readStored = (key: string): string => {
    try { return storage?.getItem(key) ?? ""; } catch { return ""; }
  };
  const writeStored = (key: string, value: string): void => {
    try { storage?.setItem(key, value); } catch { /* storage unavailable */ }
  };

  let baseUrl = readStored(KEY_BASE);
  let token = readStored(KEY_TOKEN);
  let cached: string[] = [];
  let metaById = new Map<string, EntityMeta>();
  let state: HaStatus = { state: "idle" };

  const readStr = (obj: Record<string, unknown>, key: string): string => {
    const v = obj[key];
    return typeof v === "string" ? v : "";
  };

  async function connect(nextBase: string, nextToken: string): Promise<void> {
    baseUrl = nextBase.trim().replace(/\/+$/, "");
    token = nextToken.trim();
    // Persist immediately so the URL/token survive a reload even when the
    // connection fails (the common first-run CORS case) — the fields stay
    // filled and index.ts can auto-retry them.
    writeStored(KEY_BASE, baseUrl);
    writeStored(KEY_TOKEN, token);
    state = { state: "connecting" };

    let response: Response;
    try {
      response = await doFetch(`${baseUrl}/api/states`, {
        headers: { Authorization: `Bearer ${token}` },
      });
    } catch {
      state = {
        state: "error",
        message: `Couldn't reach Home Assistant. If it's running, add ${origin} to http.cors_allowed_origins in configuration.yaml and restart — this page loads from a different origin.`,
      };
      return;
    }

    if (response.status === 401 || response.status === 403) {
      state = { state: "error", message: "Home Assistant rejected the token — check the long-lived access token." };
      return;
    }
    if (!response.ok) {
      state = { state: "error", message: `Home Assistant returned ${response.status}.` };
      return;
    }

    let list: string[];
    const nextMeta = new Map<string, EntityMeta>();
    try {
      const data: unknown = await response.json();
      const ids = new Set<string>();
      if (Array.isArray(data)) {
        for (const item of data) {
          if (item === null || typeof item !== "object") continue;
          const record = item as Record<string, unknown>;
          const id = record["entity_id"];
          if (typeof id !== "string" || id === "") continue;
          ids.add(id);
          const attrs = record["attributes"];
          const a: Record<string, unknown> =
            attrs !== null && typeof attrs === "object" ? (attrs as Record<string, unknown>) : {};
          nextMeta.set(id, {
            friendlyName: readStr(a, "friendly_name"),
            icon: readStr(a, "icon"),
            deviceClass: readStr(a, "device_class"),
          });
        }
      }
      list = Array.from(ids).sort();
    } catch {
      state = { state: "error", message: "Home Assistant sent an unexpected response." };
      return;
    }

    cached = list;
    metaById = nextMeta;
    state = { state: "connected", count: cached.length };
  }

  return {
    creds: (): HaCreds => ({ baseUrl, token }),
    entities: (): string[] => cached,
    meta: (entityId: string): EntityMeta | undefined => metaById.get(entityId),
    status: (): HaStatus => state,
    connect,
  };
}
