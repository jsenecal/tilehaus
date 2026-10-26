"use strict";

// Panel CLI: encode a deck JSON spec and talk to a Tilehaus panel's config API.
//   node scripts/deck_cli.js build  <spec.json> <out.bin>   (offline encode + round-trip check)
//   node scripts/deck_cli.js status                --device <ip>
//   node scripts/deck_cli.js push   <spec.json>    --device <ip>
//   node scripts/deck_cli.js reset                 --device <ip>
//   node scripts/deck_cli.js session               --device <ip>
// Reuses the tested codec in web/model/deck.ts, so encoding matches the firmware.
//
// Writes need the panel's Configurator switch on in Home Assistant. The first
// write claims a session; its key is cached per device in
// $XDG_CONFIG_HOME/tilehaus/sessions.json (~/.config by default) and reused
// until the switch goes off. --key <hex> overrides the cache.

const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { loadTypeScriptModule } = require("./load_typescript_module");

const { encodeDeck, decodeDeckDocument } = loadTypeScriptModule("web/model/deck.ts");
const { cardTypeLabel } = loadTypeScriptModule("web/card_types.ts");

const MEDIA_TYPE = "application/vnd.tilehaus.deck";
// Firmware older than the X-Deck-Max-Type header knows card types up to Back (22).
const PANEL_MAX_TYPE_DEFAULT = 22;

function parseArgs(argv) {
  const out = { _: [] };
  for (let i = 0; i < argv.length; i += 1) {
    if (argv[i] === "--device") out.device = argv[++i];
    else if (argv[i] === "--key") out.key = argv[++i];
    else out._.push(argv[i]);
  }
  return out;
}

const url = (device) => `http://${device}/api/v1/config`;
const sessionUrl = (device) => `http://${device}/api/v1/session`;

const LOCKED_HELP = "panel is locked: turn on its Configurator switch in Home Assistant, then re-run";
const TAKEN_HELP = "another client holds this panel's session: turn the Configurator switch off and on in Home Assistant to take over";

const sessionsFile = () =>
  path.join(process.env.XDG_CONFIG_HOME || path.join(os.homedir(), ".config"), "tilehaus", "sessions.json");

function readSessions() {
  try { return JSON.parse(fs.readFileSync(sessionsFile(), "utf8")); } catch { return {}; }
}

function cacheKey(device, key) {
  const all = readSessions();
  if (key) all[device] = key; else delete all[device];
  fs.mkdirSync(path.dirname(sessionsFile()), { recursive: true });
  fs.writeFileSync(sessionsFile(), JSON.stringify(all, null, 2) + "\n", { mode: 0o600 });
}

// POST a claim; returns the key or throws with the reason.
async function claim(device) {
  const res = await fetch(sessionUrl(device), { method: "POST" });
  if (res.status === 201) {
    const { key } = await res.json();
    if (!key) throw new Error("claim returned no key");
    cacheKey(device, key);
    console.log("claimed the configurator session");
    return key;
  }
  if (res.status === 423) throw new Error(LOCKED_HELP);
  if (res.status === 409) throw new Error(TAKEN_HELP);
  throw new Error(`claim failed: ${res.status}`);
}

// Send a write with the session key, claiming one if needed. A cached key the
// panel no longer honours (401) is dropped and the claim retried once.
async function write(args, init) {
  const send = (key) => fetch(url(args.device), {
    ...init, headers: { ...init.headers, Authorization: `Bearer ${key}` },
  });
  let key = args.key || readSessions()[args.device] || await claim(args.device);
  let res = await send(key);
  if (res.status === 401 && !args.key) {
    cacheKey(args.device, null);
    key = await claim(args.device);
    res = await send(key);
  }
  if (res.status === 423) { if (!args.key) cacheKey(args.device, null); throw new Error(LOCKED_HELP); }
  if (res.status === 401) throw new Error(TAKEN_HELP);
  return res;
}
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// Read a spec file ({cards,pages?,pageBack?,gridCols?,gridRows?,accent?} or a bare cards array)
// and encode it to a DECK document (Uint8Array).
function encodeSpec(specFile) {
  const raw = JSON.parse(fs.readFileSync(specFile, "utf8"));
  const cards = Array.isArray(raw) ? raw : raw.cards;
  if (!Array.isArray(cards)) throw new Error("spec must be an array of cards or an object with a `cards` array");
  const pages = (!Array.isArray(raw) && raw.pages) || ["Home"];
  const accent = (!Array.isArray(raw) && raw.accent != null) ? raw.accent : -1;
  const gridCols = (!Array.isArray(raw) && raw.gridCols != null) ? raw.gridCols : 10;
  const gridRows = (!Array.isArray(raw) && raw.gridRows != null) ? raw.gridRows : 6;
  const pageBack = (!Array.isArray(raw) && raw.pageBack) || [];
  return encodeDeck(cards, accent, pages, gridCols, gridRows, pageBack);
}

async function getState(device) {
  const res = await fetch(url(device), { method: "GET" });
  const etag = res.headers.get("etag") || '"0"';
  const dw = res.headers.get("x-display-width");
  const dh = res.headers.get("x-display-height");
  const maxType = Number(res.headers.get("x-deck-max-type")) || PANEL_MAX_TYPE_DEFAULT;
  if (res.status === 204) return { etag, dw, dh, maxType, empty: true };
  if (!res.ok) throw new Error(`GET failed: ${res.status}`);
  const doc = decodeDeckDocument(new Uint8Array(await res.arrayBuffer()));
  return { etag, dw, dh, maxType, empty: false, doc };
}

// Poll GET until the panel is back after a config-apply reboot.
async function waitForReboot(device, timeoutMs = 90000) {
  await sleep(3000); // let it go down first
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try { return await getState(device); } catch { /* still down */ }
    await sleep(2000);
  }
  throw new Error("panel did not come back after reboot");
}

function printState(s) {
  console.log(`ETag ${s.etag}   display ${s.dw || "?"}x${s.dh || "?"}   card types up to ${s.maxType}`);
  if (s.empty) { console.log("awaiting config (204 — no layout stored)"); return; }
  const d = s.doc;
  const back = d.pages.filter((_, i) => d.pageBack[i]);
  console.log(`pages ${JSON.stringify(d.pages)}${back.length ? "   back " + JSON.stringify(back) : ""}   grid ${d.gridCols}x${d.gridRows}   accent ${d.accent}   tiles ${d.cards.length}`);
  d.cards.forEach((c, i) => {
    console.log(`  #${i} type=${c.type} ${c.w}x${c.h} @(${c.col},${c.row}) page ${c.page}` +
      `${c.entity ? " entity=" + c.entity : ""}${c.title ? " title=" + JSON.stringify(c.title) : ""}`);
  });
}

async function main() {
  const [cmd, ...rest] = process.argv.slice(2);
  const args = parseArgs(rest);

  if (cmd === "build") {
    const [specFile, outFile] = args._;
    if (!specFile || !outFile) throw new Error("usage: build <spec.json> <out.bin>");
    const bin = encodeSpec(specFile);
    decodeDeckDocument(new Uint8Array(bin)); // round-trip check
    fs.writeFileSync(outFile, Buffer.from(bin));
    console.log(`wrote ${outFile} (${bin.length} bytes) — encodes + decodes OK`);
    return;
  }

  // Every other command talks to a panel; never guess which one.
  if (["status", "push", "reset", "session"].includes(cmd) && !args.device) {
    throw new Error(`${cmd} needs --device <host>, e.g. --device tilehaus-office.local`);
  }

  if (cmd === "status") {
    printState(await getState(args.device));
    return;
  }

  if (cmd === "push") {
    const [specFile] = args._;
    if (!specFile) throw new Error("usage: push <spec.json> --device <ip>");
    const bin = encodeSpec(specFile);
    const current = await getState(args.device);
    // Refuse what the panel would refuse, naming the tile.
    const cards = decodeDeckDocument(new Uint8Array(bin)).cards;
    const tooNew = cards.findIndex((c) => c.type > current.maxType);
    if (tooNew >= 0) {
      const c = cards[tooNew];
      throw new Error(`tile ${tooNew + 1} ("${c.title || cardTypeLabel(c.type)}") is a ${cardTypeLabel(c.type)} (type ${c.type}), ` +
        `but ${args.device} knows card types up to ${current.maxType}: flash newer firmware first`);
    }
    const res = await write(args, {
      method: "PUT",
      headers: { "Content-Type": MEDIA_TYPE, "If-Match": current.etag },
      body: Buffer.from(bin),
    });
    if (res.status === 204) {
      console.log("pushed; panel rebooting to apply…");
      const back = await waitForReboot(args.device);
      console.log(`applied. now serving ETag ${back.etag}`);
      return;
    }
    if (res.status === 409) {
      console.error(`conflict: device ETag is ${res.headers.get("etag")}, not ${current.etag}. Re-run to retry.`);
      process.exit(1);
    }
    console.error(`PUT failed: ${res.status} ${await res.text().catch(() => "")}`);
    process.exit(1);
  }

  if (cmd === "reset") {
    const current = await getState(args.device);
    const res = await write(args, { method: "DELETE", headers: { "If-Match": current.etag } });
    if (res.status === 204) {
      console.log("cleared; panel rebooting to the awaiting-config screen…");
      await waitForReboot(args.device).catch(() => {});
      console.log("done.");
      return;
    }
    console.error(`DELETE failed: ${res.status}`);
    process.exit(1);
  }

  if (cmd === "session") {
    const key = args.key || readSessions()[args.device];
    const res = await fetch(sessionUrl(args.device), { headers: key ? { Authorization: `Bearer ${key}` } : {} });
    if (!res.ok) throw new Error(`session check failed: ${res.status}`);
    const s = await res.json();
    const who = s.state === "claimed" ? (s.mine ? " (this machine)" : " (another client)") : "";
    console.log(`${s.state}${who}   idle timeout ${s.timeoutMinutes || "none"} min`);
    return;
  }

  console.error("usage: deck_cli.js <build <spec.json> <out.bin> | status | push <spec.json> | reset | session> --device <host> [--key <hex>]");
  process.exit(1);
}

main().catch((err) => { console.error(String(err && err.message ? err.message : err)); process.exit(1); });
