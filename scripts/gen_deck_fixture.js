"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { loadTypeScriptModule } = require("./load_typescript_module");

const { encodeDeck } = loadTypeScriptModule("web/model/deck.ts");

const BASIC = [
  {
    type: 4, entity: "light.desk", title: "Lamp", w: 2, h: 2, entity2: "",
    icon: "\u{F095F}", iconAlt: "\u{F1B1F}", activeColor: -1, inactiveColor: -1,
    hideLabel: false, detail: false, followColor: true, showHilo: false, transparent: false, showWeather: true, showClock: true, tightMargins: false, align: 5, page: 0, col: 0, row: 0,
  },
  {
    type: 2, entity: "switch.fan", title: "Fan", w: 2, h: 2, entity2: "",
    icon: "", iconAlt: "", activeColor: 0x2e7d32, inactiveColor: -1,
    hideLabel: true, detail: false, followColor: false, showHilo: false, transparent: false, showWeather: true, showClock: true, tightMargins: false, align: 5, page: 0, col: 3, row: 0,
  },
];

const out = path.resolve(__dirname, "../tests/firmware/fixtures/deck_basic.bin");
fs.mkdirSync(path.dirname(out), { recursive: true });
fs.writeFileSync(out, Buffer.from(encodeDeck(BASIC)));
console.log(`wrote ${out} (${fs.statSync(out).size} bytes)`);

// NOTE: deck_v5_pages.bin is intentionally NOT regenerated here — it is a frozen
// v5 fixture (20-byte header, no grid bytes) that the tests use to prove a v5
// document still decodes (with the 10x6 default grid). encodeDeck now emits v6,
// so regenerating it would change its version.

// A v6 document with a non-default grid (8x4), to exercise the two grid header
// bytes on both the TS and C++ decoders.
const GRID = [
  { type: 2, entity: "switch.fan", title: "Fan", w: 2, h: 2, entity2: "", icon: "", iconAlt: "", activeColor: -1, inactiveColor: -1, hideLabel: false, detail: false, followColor: false, showHilo: false, transparent: false, showWeather: true, showClock: true, tightMargins: false, align: 5, page: 0, col: 0, row: 0 },
];
const gridOut = path.resolve(__dirname, "../tests/firmware/fixtures/deck_v6_grid.bin");
fs.writeFileSync(gridOut, Buffer.from(encodeDeck(GRID, -1, ["Home"], 8, 4)));
console.log(`wrote ${gridOut} (${fs.statSync(gridOut).size} bytes)`);
