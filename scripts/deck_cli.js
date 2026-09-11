"use strict";

// Panel CLI: encode a deck JSON spec and talk to a Tilehaus panel's config API.
//   node scripts/deck_cli.js build  <spec.json> <out.bin>   (offline encode + round-trip check)
//   node scripts/deck_cli.js status                --device <ip>
//   node scripts/deck_cli.js push   <spec.json>    --device <ip>
//   node scripts/deck_cli.js reset                 --device <ip>
// Reuses the tested codec in web/model/deck.ts, so encoding matches the firmware.

const fs = require("node:fs");
const { loadTypeScriptModule } = require("./load_typescript_module");

const { encodeDeck, decodeDeckDocument } = loadTypeScriptModule("web/model/deck.ts");

const MEDIA_TYPE = "application/vnd.tilehaus.deck";
const DEFAULT_DEVICE = "192.168.252.221";

function parseArgs(argv) {
  const out = { _: [] };
  for (let i = 0; i < argv.length; i += 1) {
    if (argv[i] === "--device") out.device = argv[++i];
    else out._.push(argv[i]);
  }
  out.device = out.device || DEFAULT_DEVICE;
  return out;
}

const url = (device) => `http://${device}/api/v1/config`;
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// Read a spec file ({cards,pages?,gridCols?,gridRows?,accent?} or a bare cards array)
// and encode it to a DECK document (Uint8Array).
function encodeSpec(specFile) {
  const raw = JSON.parse(fs.readFileSync(specFile, "utf8"));
  const cards = Array.isArray(raw) ? raw : raw.cards;
  if (!Array.isArray(cards)) throw new Error("spec must be an array of cards or an object with a `cards` array");
  const pages = (!Array.isArray(raw) && raw.pages) || ["Home"];
  const accent = (!Array.isArray(raw) && raw.accent != null) ? raw.accent : -1;
  const gridCols = (!Array.isArray(raw) && raw.gridCols != null) ? raw.gridCols : 10;
  const gridRows = (!Array.isArray(raw) && raw.gridRows != null) ? raw.gridRows : 6;
  return encodeDeck(cards, accent, pages, gridCols, gridRows);
}

async function getState(device) {
  const res = await fetch(url(device), { method: "GET" });
  const etag = res.headers.get("etag") || '"0"';
  const dw = res.headers.get("x-display-width");
  const dh = res.headers.get("x-display-height");
  if (res.status === 204) return { etag, dw, dh, empty: true };
  if (!res.ok) throw new Error(`GET failed: ${res.status}`);
  const doc = decodeDeckDocument(new Uint8Array(await res.arrayBuffer()));
  return { etag, dw, dh, empty: false, doc };
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
  console.log(`ETag ${s.etag}   display ${s.dw || "?"}x${s.dh || "?"}`);
  if (s.empty) { console.log("awaiting config (204 — no layout stored)"); return; }
  const d = s.doc;
  console.log(`pages ${JSON.stringify(d.pages)}   grid ${d.gridCols}x${d.gridRows}   accent ${d.accent}   tiles ${d.cards.length}`);
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

  if (cmd === "status") {
    printState(await getState(args.device));
    return;
  }

  if (cmd === "push") {
    const [specFile] = args._;
    if (!specFile) throw new Error("usage: push <spec.json> --device <ip>");
    const bin = encodeSpec(specFile);
    const current = await getState(args.device);
    const res = await fetch(url(args.device), {
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
    const res = await fetch(url(args.device), { method: "DELETE", headers: { "If-Match": current.etag } });
    if (res.status === 204) {
      console.log("cleared; panel rebooting to the awaiting-config screen…");
      await waitForReboot(args.device).catch(() => {});
      console.log("done.");
      return;
    }
    console.error(`DELETE failed: ${res.status}`);
    process.exit(1);
  }

  console.error("usage: deck_cli.js <build <spec.json> <out.bin> | status | push <spec.json> | reset> [--device <ip>]");
  process.exit(1);
}

main().catch((err) => { console.error(String(err && err.message ? err.message : err)); process.exit(1); });
