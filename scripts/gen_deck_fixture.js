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

// deck_v6_grid.bin (a v6 document with an 8x4 grid) is frozen the same way:
// encodeDeck now emits v7, which adds a page-options byte after each page name.

// A v7 document whose second page shows the Back button, to pin the page
// options byte on both the TS and C++ decoders.
const FAN = { type: 2, entity: "switch.fan", title: "Fan", w: 2, h: 2, entity2: "", icon: "", iconAlt: "", activeColor: -1, inactiveColor: -1, hideLabel: false, detail: false, followColor: false, showHilo: false, transparent: false, showWeather: true, showClock: true, tightMargins: false, align: 5, page: 0, col: 0, row: 0 };
const PAGES = [FAN, { ...FAN, page: 1 }];
const pagesOut = path.resolve(__dirname, "../tests/firmware/fixtures/deck_v7_pages.bin");
fs.writeFileSync(pagesOut, Buffer.from(encodeDeck(PAGES, -1, ["Home", "Lights"], 12, 7, [false, true])));
console.log(`wrote ${pagesOut} (${fs.statSync(pagesOut).size} bytes)`);
