"use strict";

// Fails when a firmware header colours something with a raw 0xRRGGBB instead of
// a named palette role. Night mode recolours by value, so a stray literal is a
// colour the theme can't see. Allowed: a named constant's definition, comments,
// and the files below.
//
//   node scripts/check_palette.js

const fs = require("node:fs");
const path = require("node:path");

const DIR = path.resolve(__dirname, "../firmware/native");
const ALLOWED_FILES = new Map([
  ["deck_ui_asset.h", "generated web bundle"],
  ["color_tab.h", "the colour picker's preset swatches are data, not theme"],
]);
const LITERAL = /0x[0-9A-Fa-f]{6}\b/;
const DEFINITION = /constexpr\s+uint32_t\s+k\w+\s*=\s*0x[0-9A-Fa-f]{6}\b/;
const MASK = /lv_color_to_u32\([^)]*\)\s*&\s*0xFFFFFF/;  // theme.h: a 24-bit mask

const problems = [];
for (const name of fs.readdirSync(DIR).filter((f) => f.endsWith(".h")).sort()) {
  if (ALLOWED_FILES.has(name)) continue;
  fs.readFileSync(path.join(DIR, name), "utf8").split("\n").forEach((line, i) => {
    const code = line.split("//")[0];
    if (!LITERAL.test(code) || DEFINITION.test(code) || MASK.test(code)) return;
    problems.push(`firmware/native/${name}:${i + 1}: ${line.trim()}`);
  });
}

if (problems.length > 0) {
  console.error("Raw colour literals — name them in firmware/native/palette.h:\n  " + problems.join("\n  "));
  process.exit(1);
}
console.log("palette: no raw colour literals");
